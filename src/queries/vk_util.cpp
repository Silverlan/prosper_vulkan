// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_prosper_vulkan.h"
#include "vk_util.hpp"
#include "vk_context.hpp"
#include "shader/prosper_shader.hpp"
#include "image/vk_image.hpp"
#include "vk_memory_tracker.hpp"
#ifdef PROSPER_VULKAN_ENABLE_LUNAR_GLASS
#include <util_lunarglass/util_lunarglass.hpp>
#endif
#include <prosper_glsl.hpp>
#include <prosper_util.hpp>
#include <misc/descriptor_set_create_info.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
#include <sharedutils/util_path.hpp>
#include <fsys/filesystem.h>
#include <util_image_buffer.hpp>
#include <wrappers/device.h>
#include <wrappers/physical_device.h>
#include <wrappers/instance.h>
#include <wrappers/shader_module.h>
#include <wrappers/memory_block.h>
#include <misc/glsl_to_spirv.h>
#include <sstream>

using namespace prosper;

std::unique_ptr<Anvil::DescriptorSetCreateInfo> prosper::ToAnvilDescriptorSetInfo(const DescriptorSetInfo &descSetInfo)
{
	auto dsInfo = Anvil::DescriptorSetCreateInfo::create();
	for(auto &binding : descSetInfo.bindings) {
		dsInfo->add_binding(binding.bindingIndex, static_cast<Anvil::DescriptorType>(binding.type), binding.descriptorArraySize, static_cast<Anvil::ShaderStageFlagBits>(binding.shaderStages));
	}
	return dsInfo;
}

static bool glsl_to_spv(prosper::IPrContext &context, prosper::ShaderStage stage, const std::string &shaderCode, std::vector<unsigned int> &spirv, std::string *infoLog, std::string *debugInfoLog, const std::string &fileName, const std::vector<prosper::glsl::IncludeLine> &includeLines,
  unsigned int lineOffset, bool bHlsl = false, bool withDebugInfo = false)
{
	auto &dev = static_cast<prosper::VlkContext &>(context).GetDevice();
	auto shaderPtr = Anvil::GLSLShaderToSPIRVGenerator::create(&dev, Anvil::GLSLShaderToSPIRVGenerator::MODE_USE_SPECIFIED_SOURCE, shaderCode, static_cast<Anvil::ShaderStage>(stage));
	if(shaderPtr == nullptr)
		return false;
	auto relPath = ::util::FilePath(fileName);
	if(relPath.GetFront() == "shaders")
		relPath.PopFront();
	shaderPtr->set_glsl_file_path(relPath.GetString());
	shaderPtr->set_with_debug_info(withDebugInfo);
	if(shaderPtr->get_spirv_blob_size() == 0u) {
		auto &shaderInfoLog = shaderPtr->get_shader_info_log();
		if(shaderInfoLog.empty() == false) {
			if(infoLog != nullptr) {
				prosper::glsl::translate_error(shaderCode, shaderInfoLog, fileName, includeLines, CUInt32(lineOffset), infoLog);
				*infoLog = std::string("Shader File: \"") + fileName + "\"\n" + (*infoLog);
			}
			if(debugInfoLog != nullptr)
				*debugInfoLog = shaderPtr->get_debug_info_log();
		}
		else {
			auto &programInfoLog = shaderPtr->get_program_info_log();
			if(programInfoLog.empty() == false) {
				if(infoLog != nullptr) {
					prosper::glsl::translate_error(shaderCode, programInfoLog, fileName, includeLines, CUInt32(lineOffset), infoLog);
					*infoLog = std::string("Shader File: \"") + fileName + "\"\n" + (*infoLog);
				}
				if(debugInfoLog != nullptr)
					*debugInfoLog = shaderPtr->get_program_debug_info_log();
			}
			else if(infoLog != nullptr)
				*infoLog = "An unknown error has occurred!";
		}
		return false;
	}
	auto shaderMod = Anvil::ShaderModule::create_from_spirv_generator(&dev, shaderPtr.get());
	spirv = shaderMod->get_spirv_blob();
	return true;
}

static std::string get_cache_path(const std::string &shaderRootPath, bool withDebugInfo) { return ::util::DirPath("cache", shaderRootPath, withDebugInfo ? "spirv_full" : "spirv").GetString(); }

bool prosper::glsl_to_spv(IPrContext &context, prosper::ShaderStage stage, const std::string &shaderRootPath, const std::string &relFileName, std::vector<unsigned int> &spirv, std::string *infoLog, std::string *debugInfoLog, bool bReload, const std::string &prefixCode,
  const std::unordered_map<std::string, std::string> &definitions, bool withDebugInfo)
{
	auto spvCachePath = get_cache_path(shaderRootPath, withDebugInfo);
	auto fileName = ::util::FilePath(shaderRootPath, relFileName).GetString();
	auto fName = fileName;
	std::string ext;
	if(!ufile::get_extension(fileName, &ext)) {
		auto stageExt = prosper::glsl::get_shader_file_extension(stage);
		auto fullFileName = fileName + '.' + stageExt;
		auto fNameSpv = ::util::FilePath(spvCachePath, fullFileName + ".spv").GetString();
		auto bSpvExists = filemanager::exists(fNameSpv);
		if(bSpvExists == true) {
			ext = "spv";
			fName = fNameSpv;
		}
		if(bSpvExists == false || bReload == true) {
			if(filemanager::exists(fullFileName)) {
				ext = stageExt;
				fName = fullFileName;
			}
		}
	}
	ustring::to_lower(ext);
	if(!filemanager::exists(fName)) {
		if(infoLog != nullptr)
			*infoLog = std::string("File '") + fName + std::string("' not found!");
		return false;
	}

	auto isGlslExt = prosper::glsl::is_glsl_file_extension(ext);
	if(!isGlslExt && ext != "hlsl") // We'll assume it's a SPIR-V file
	{
		auto f = filemanager::open_file(fName, filemanager::FileMode::Read | filemanager::FileMode::Binary);
		if(f == nullptr) {
			if(infoLog != nullptr)
				*infoLog = std::string("Unable to open file '") + fName + std::string("'!");
			return false;
		}
		auto sz = f->GetSize();
		assert((sz % sizeof(unsigned int)) == 0);
		if((sz % sizeof(unsigned int)) != 0)
			return false;
		auto origSize = spirv.size();
		spirv.resize(origSize + sz / sizeof(unsigned int));
		f->Read(spirv.data() + origSize, sz);
		return true;
	}

	auto applyPreprocessing = true;
	auto glslFileName = fName;
	if(isGlslExt) {
		// Use specialized vulkan shader if one exists
		auto tmp = glslFileName;
		auto ext = ufile::remove_extension_from_filename(tmp, prosper::glsl::get_glsl_file_extensions());
		assert(ext.has_value());
		tmp += "_vk." + *ext;
		if(filemanager::exists(tmp)) {
			applyPreprocessing = false;
			glslFileName = tmp;
		}
	}
	if(applyPreprocessing && context.IsValidationEnabled()) {
		std::stringstream ss;
		ss << "[VK] WARNING: No optimized version found for shader '" << fileName << "'! This may cause poor performance!\n";
		context.ValidationCallback(prosper::DebugMessageSeverityFlags::WarningBit, ss.str());
	}

	std::vector<prosper::glsl::IncludeLine> includeLines;
	unsigned int lineOffset = 0;
	::util::Path fPath {glslFileName};
	fPath.PopFront();

	auto shaderCode = load_glsl(context, stage, fPath.GetString(), infoLog, debugInfoLog, includeLines, lineOffset, prefixCode, definitions, applyPreprocessing);
	if(shaderCode.has_value() == false)
		return false;

	auto glslCachePath = ::util::FilePath(fName);
	if(glslCachePath.GetFront() == "shaders") {
		glslCachePath.PopFront();
		glslCachePath = ::util::FilePath("cache", "shaders", "preprocessed", glslCachePath);
		filemanager::create_path(glslCachePath.GetPath());
		filemanager::write_file(glslCachePath.GetString(), *shaderCode);
	}

	auto r = ::glsl_to_spv(context, stage, *shaderCode, spirv, infoLog, debugInfoLog, fName, includeLines, lineOffset, (ext == "hlsl") ? true : false, withDebugInfo);
	if(r == false)
		return r;
	auto spirvName = ::util::FilePath(spvCachePath, fName + ".spv").GetString();
	filemanager::create_path(ufile::get_path_from_filename(spirvName));
	auto fOut = filemanager::open_file<VFilePtrReal>(spirvName, filemanager::FileMode::Write | filemanager::FileMode::Binary);
	if(fOut == nullptr)
		return r;
	fOut->Write(spirv.data(), spirv.size() * sizeof(unsigned int));
	return r;
}

static void fix_optimized_shader(std::string &inOutShader, const std::vector<std::optional<prosper::IPrContext::ShaderDescriptorSetInfo>> &descSetInfos)
{
	// LunarGLASS has some issues and creates incorrect results in certain cases, we'll try to fix some of the issues here

	auto fReplaceText = [&inOutShader](size_t start, size_t end, const std::string &str) { inOutShader = inOutShader.substr(0, start) + str + inOutShader.substr(end); };

	// Push constant blocks are replaced with 'layout(std430)', we'll correct it here
	auto *str = "layout(std430) uniform";
	auto posPushC = inOutShader.find(str);
	if(posPushC != std::string::npos)
		fReplaceText(posPushC, posPushC + strlen(str), "layout(push_constant) uniform");

	auto fFindDescriptorSetBinding = [&descSetInfos](const std::string &uniformName, uint32_t &outSetIndex, uint32_t &outBindingIndex) -> bool {
		for(auto itDs = descSetInfos.begin(); itDs != descSetInfos.end(); ++itDs) {
			auto &dsInfo = *itDs;
			if(dsInfo.has_value() == false)
				continue;
			for(auto itBinding = dsInfo->bindingPoints.begin(); itBinding != dsInfo->bindingPoints.end(); ++itBinding) {
				auto &binding = *itBinding;
				auto it = std::find_if(binding.args.begin(), binding.args.end(),
				  [&uniformName](const std::string &arg) { return ustring::compare(arg.c_str(), uniformName.c_str(), true, uniformName.length()) && (uniformName.length() == arg.length() || (arg.length() > uniformName.length() && arg[uniformName.length()] == '[')); });
				if(it == binding.args.end())
					continue;
				auto &name = *it;
				outSetIndex = (itDs - descSetInfos.begin());
				outBindingIndex = (itBinding - dsInfo->bindingPoints.begin());
				return true;
			}
		}
		return false;
	};

	// Set and binding indices are lost through optimization, so we need to restore them
	auto posLayout = inOutShader.find("layout(");
	while(posLayout != std::string::npos) {
		auto end = inOutShader.find(")", posLayout);
		if(end == std::string::npos)
			break; // Should be unreachable
		auto layout = inOutShader.substr(posLayout, end - posLayout + 1);
		auto posBinding = layout.find("binding");
		if(posBinding != std::string::npos) {
			auto argEnd = inOutShader.find_first_of("{;[", end);
			if(argEnd != std::string::npos) {
				auto strArgs = inOutShader.substr(end + 1, argEnd - (end + 1));
				std::vector<std::string> args;
				ustring::explode_whitespace(strArgs, args);
				if(args.empty() == false) {
					auto &arg = args.back();
					uint32_t setIndex, bindingIndex;
					if(fFindDescriptorSetBinding(arg, setIndex, bindingIndex) == false)
						std::cout << "WARNING: Could not determine set index and binding index for uniform '" << arg << "'!" << std::endl;
					else {
						auto bindingEnd = layout.find_first_of(",)", posBinding);
						if(bindingEnd != std::string::npos)
							fReplaceText(posLayout + posBinding, posLayout + bindingEnd, "set=" + std::to_string(setIndex) + ",binding=" + std::to_string(bindingIndex));
					}
				}
			}
		}
		posLayout = inOutShader.find("layout(", posLayout + 1);
	}
}

std::optional<std::unordered_map<prosper::ShaderStage, std::string>> prosper::optimize_glsl(prosper::IPrContext &context, const std::unordered_map<prosper::ShaderStage, std::string> &shaderStages, std::string &outInfoLog)
{
#ifdef PROSPER_VULKAN_ENABLE_LUNAR_GLASS
	std::vector<std::optional<prosper::IPrContext::ShaderDescriptorSetInfo>> descSetInfos;
	std::vector<prosper::IPrContext::ShaderMacroLocation> macroLocations;
	std::string log;
	std::unordered_map<lunarglass::ShaderStage, std::string> shaderStageCode;
	for(auto &pair : shaderStages) {
		lunarglass::ShaderStage stage;
		switch(pair.first) {
		case ShaderStage::Compute:
			stage = lunarglass::ShaderStage::Compute;
			break;
		case ShaderStage::Fragment:
			stage = lunarglass::ShaderStage::Fragment;
			break;
		case ShaderStage::Geometry:
			stage = lunarglass::ShaderStage::Geometry;
			break;
		case ShaderStage::TessellationControl:
			stage = lunarglass::ShaderStage::TessellationControl;
			break;
		case ShaderStage::TessellationEvaluation:
			stage = lunarglass::ShaderStage::TessellationEvaluation;
			break;
		case ShaderStage::Vertex:
			stage = lunarglass::ShaderStage::Vertex;
			break;
		}
		static_assert(umath::to_integral(ShaderStage::Count) == 6u);

		auto path = pair.second;
		std::string infoLog, debugInfoLog;
		std::vector<prosper::glsl::IncludeLine> includeLines;
		unsigned int lineOffset = 0;
		auto shaderCode = glsl::load_glsl(context, pair.first, path, &infoLog, &debugInfoLog, includeLines, lineOffset);
		if(shaderCode.has_value() == false) {
			outInfoLog = !infoLog.empty() ? infoLog : debugInfoLog;
			return {};
		}
		shaderStageCode[stage] = *shaderCode;

		std::unordered_map<std::string, int32_t> definitions;
		prosper::IPrContext::ParseShaderUniforms(*shaderCode, definitions, descSetInfos, macroLocations);
	}
	auto optimizedShaders = lunarglass::optimize_glsl(shaderStageCode, log);
	if(optimizedShaders.has_value() == false)
		return {};

	std::unordered_map<prosper::ShaderStage, std::string> result {};
	for(auto &pair : *optimizedShaders) {
		ShaderStage stage;
		switch(pair.first) {
		case lunarglass::ShaderStage::Compute:
			stage = ShaderStage::Compute;
			break;
		case lunarglass::ShaderStage::Fragment:
			stage = ShaderStage::Fragment;
			break;
		case lunarglass::ShaderStage::Geometry:
			stage = ShaderStage::Geometry;
			break;
		case lunarglass::ShaderStage::TessellationControl:
			stage = ShaderStage::TessellationControl;
			break;
		case lunarglass::ShaderStage::TessellationEvaluation:
			stage = ShaderStage::TessellationEvaluation;
			break;
		case lunarglass::ShaderStage::Vertex:
			stage = ShaderStage::Vertex;
			break;
		}
		static_assert(umath::to_integral(ShaderStage::Count) == 6u);
		result[stage] = pair.second;
		fix_optimized_shader(result[stage], descSetInfos);
	}
	return result;
#else
	return {};
#endif
}

void prosper::util::initialize_image(Anvil::BaseDevice &dev, const uimg::ImageBuffer &imgSrc, IImage &img)
{
	auto extents = img.GetExtents();
	auto w = extents.width;
	auto h = extents.height;
	auto srcFormat = img.GetFormat();
	if(srcFormat != Format::R8G8B8A8_UNorm && srcFormat != Format::R8G8B8_UNorm_PoorCoverage)
		throw std::invalid_argument("Invalid image format for tga target image: Only VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8_UNORM are supported!");
	if(imgSrc.GetWidth() != w || imgSrc.GetHeight() != h)
		throw std::invalid_argument("Invalid image extents for tga target image: Extents must be the same for source tga image and target image!");

	auto memBlock = static_cast<VlkImage &>(img)->get_memory_block();
	auto size = w * h * get_byte_size(srcFormat);
	uint8_t *outDataPtr = nullptr;
	memBlock->map(0ull, size, reinterpret_cast<void **>(&outDataPtr));

	auto *imgData = static_cast<const uint8_t *>(imgSrc.GetData());
	auto pos = decltype(imgSrc.GetSize()) {0};
	auto tgaFormat = imgSrc.GetFormat();
	auto srcPxSize = get_byte_size(srcFormat);
	auto rowPitch = w * srcPxSize;
	for(auto y = h; y > 0;) {
		--y;
		auto *row = outDataPtr + y * rowPitch;
		for(auto x = decltype(w) {0}; x < w; ++x) {
			auto *px = row;
			px[0] = imgData[pos];
			px[1] = imgData[pos + 1];
			px[2] = imgData[pos + 2];
			switch(tgaFormat) {
			case uimg::Format::RGBA8:
				{
					pos += 4;
					if(srcFormat == Format::R8G8B8A8_UNorm)
						px[3] = imgData[pos + 3];
					break;
				}
			default:
				{
					if(srcFormat == Format::R8G8B8A8_UNorm)
						px[3] = std::numeric_limits<uint8_t>::max();
					pos += 3;
					break;
				}
			}
			row += srcPxSize;
		}
	}

	memBlock->unmap();
}

static std::string decode_driver_version(prosper::Vendor vendor, uint32_t versionEncoded)
{
	// See https://github.com/SaschaWillems/vulkan.gpuinfo.org/blob/1e6ca6e3c0763daabd6a101b860ab4354a07f5d3/functions.php#L294
	switch(vendor) {
	case prosper::Vendor::Nvidia:
		{
			auto major = (versionEncoded >> 22) & 0x3ff;
			auto minor = (versionEncoded >> 14) & 0x0ff;
			auto revision = (versionEncoded >> 6) & 0x0ff;
			auto misc = (versionEncoded) & 0x003f;
			return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision) + "." + std::to_string(misc);
		}
	case prosper::Vendor::Intel:
		{
			auto major = versionEncoded >> 14;
			auto minor = versionEncoded & 0x3fff;
			return std::to_string(major) + "." + std::to_string(minor);
		}
	}
	auto major = versionEncoded >> 22;
	auto minor = (versionEncoded >> 12) & 0x3ff;
	auto revision = versionEncoded & 0xfff;
	return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision);
}

static std::optional<prosper::util::VendorDeviceInfo> get_vendor_device_info(const Anvil::PhysicalDevice &dev)
{
	auto *props = dev.get_device_properties().core_vk1_0_properties_ptr;
	if(!props)
		return {};
	auto apiVersion = props->api_version;
	prosper::util::VendorDeviceInfo deviceInfo {};

	// See https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#extendingvulkan-coreversions-versionnumbers
	auto variant = apiVersion >> 29;
	auto major = (apiVersion >> 22) & 0x7FU;
	auto minor = (apiVersion >> 12) & 0x3FFU;
	auto patch = apiVersion & 0xFFFU;
	deviceInfo.apiVersion = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);

	deviceInfo.deviceType = static_cast<prosper::PhysicalDeviceType>(props->device_type);
	deviceInfo.deviceName = props->device_name;
	deviceInfo.vendor = static_cast<Vendor>(props->vendor_id);
	deviceInfo.driverVersion = decode_driver_version(deviceInfo.vendor, props->driver_version);
	deviceInfo.deviceId = props->device_id;
	return deviceInfo;
}

prosper::util::VendorDeviceInfo prosper::util::get_vendor_device_info(const Anvil::PhysicalDevice &dev)
{
	auto devInfo = ::get_vendor_device_info(dev);
	assert(devInfo.has_value());
	if(!devInfo.has_value())
		return {};
	return *devInfo;
}

prosper::util::VendorDeviceInfo prosper::util::get_vendor_device_info(const IPrContext &context)
{
	auto &dev = static_cast<VlkContext &>(const_cast<IPrContext &>(context)).GetDevice();
	if(&dev == nullptr)
		return {};
	auto *devPhys = dev.get_physical_device();
	assert(devPhys);
	if(!devPhys)
		return {};
	return get_vendor_device_info(*devPhys);
}

std::vector<prosper::util::VendorDeviceInfo> prosper::util::get_available_vendor_devices(const IPrContext &context)
{
	auto &instance = static_cast<VlkContext &>(const_cast<IPrContext &>(context)).GetAnvilInstance();
	auto numDevices = instance.get_n_physical_devices();
	std::vector<prosper::util::VendorDeviceInfo> devices;
	devices.reserve(numDevices);
	for(auto i = decltype(numDevices) {0u}; i < numDevices; ++i) {
		auto *pDevice = instance.get_physical_device(i);
		if(!pDevice)
			continue;
		auto devInfo = ::get_vendor_device_info(*pDevice);
		if(!devInfo.has_value())
			continue;
		devices.push_back(*devInfo);
	}
	return devices;
}

std::optional<prosper::util::PhysicalDeviceMemoryProperties> prosper::util::get_physical_device_memory_properties(const IPrContext &context)
{
	prosper::util::PhysicalDeviceMemoryProperties memProps {};
	auto &vkMemProps = static_cast<VlkContext &>(const_cast<IPrContext &>(context)).GetDevice().get_physical_device_memory_properties();
	auto totalSize = 0ull;
	auto numHeaps = vkMemProps.n_heaps;
	for(auto i = decltype(numHeaps) {0}; i < numHeaps; ++i)
		memProps.heapSizes.push_back(vkMemProps.heaps[i].size);
	return memProps;
}

bool prosper::util::get_memory_stats(IPrContext &context, MemoryPropertyFlags memPropFlags, DeviceSize &outAvailableSize, DeviceSize &outAllocatedSize, std::vector<uint32_t> *optOutMemIndices)
{
	auto &dev = static_cast<VlkContext &>(context).GetDevice();
	auto &memProps = dev.get_physical_device_memory_properties();
	auto allocatedDeviceLocalSize = 0ull;
	auto &memTracker = prosper::MemoryTracker::GetInstance();
	std::vector<uint32_t> deviceLocalTypes = {};
	deviceLocalTypes.reserve(memProps.types.size());
	for(auto i = decltype(memProps.types.size()) {0u}; i < memProps.types.size(); ++i) {
		auto &type = memProps.types.at(i);
		if(type.heap_ptr == nullptr || (type.flags & static_cast<Anvil::MemoryPropertyFlagBits>(memPropFlags)) == Anvil::MemoryPropertyFlagBits::NONE)
			continue;
		if(deviceLocalTypes.empty() == false)
			assert(type.heap_ptr->size == memProps.types.at(deviceLocalTypes.front()).heap_ptr->size);
		deviceLocalTypes.push_back(i);
		uint64_t allocatedSize = 0ull;
		uint64_t totalSize = 0ull;
		memTracker.GetMemoryStats(context, i, allocatedSize, totalSize);
		allocatedDeviceLocalSize += allocatedSize;
	}
	if(deviceLocalTypes.empty())
		return false;
	std::stringstream ss;
	auto totalMemory = memProps.types.at(deviceLocalTypes.front()).heap_ptr->size;
	outAvailableSize = totalMemory;
	outAllocatedSize = allocatedDeviceLocalSize;
	if(optOutMemIndices)
		*optOutMemIndices = deviceLocalTypes;
	return true;
}

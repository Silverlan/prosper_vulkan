/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_util.hpp"
#include "vk_context.hpp"
#include "shader/prosper_shader.hpp"
#include "image/vk_image.hpp"
#include "vk_memory_tracker.hpp"
#include <prosper_glsl.hpp>
#include <prosper_util.hpp>
#include <misc/descriptor_set_create_info.h>
#include <sharedutils/util_file.h>
#include <sharedutils/util_string.h>
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
	for(auto &binding : descSetInfo.bindings)
	{
		dsInfo->add_binding(
			binding.bindingIndex,static_cast<Anvil::DescriptorType>(binding.type),binding.descriptorArraySize,
			static_cast<Anvil::ShaderStageFlagBits>(binding.shaderStages)
		);
	}
	return dsInfo;
}

static bool glsl_to_spv(
	prosper::IPrContext &context,prosper::ShaderStage stage,const std::string &shaderCode,std::vector<unsigned int> &spirv,std::string *infoLog,std::string *debugInfoLog,const std::string &fileName,
	const std::vector<prosper::glsl::IncludeLine> &includeLines,unsigned int lineOffset,bool bHlsl=false
)
{
	auto &dev = static_cast<prosper::VlkContext&>(context).GetDevice();
	auto shaderPtr = Anvil::GLSLShaderToSPIRVGenerator::create(
		&dev,
		Anvil::GLSLShaderToSPIRVGenerator::MODE_USE_SPECIFIED_SOURCE,
		shaderCode,
		static_cast<Anvil::ShaderStage>(stage)
	);
	if(shaderPtr == nullptr)
		return false;
	if(shaderPtr->get_spirv_blob_size() == 0u)
	{
		auto &shaderInfoLog = shaderPtr->get_shader_info_log();
		if(shaderInfoLog.empty() == false)
		{
			if(infoLog != nullptr)
			{
				prosper::glsl::translate_error(shaderCode,shaderInfoLog,fileName,includeLines,CUInt32(lineOffset),infoLog);
				*infoLog = std::string("Shader File: \"") +fileName +"\"\n" +(*infoLog);
			}
			if(debugInfoLog != nullptr)
				*debugInfoLog = shaderPtr->get_debug_info_log();
		}
		else
		{
			auto &programInfoLog = shaderPtr->get_program_info_log();
			if(programInfoLog.empty() == false)
			{
				if(infoLog != nullptr)
				{
					prosper::glsl::translate_error(shaderCode,programInfoLog,fileName,includeLines,CUInt32(lineOffset),infoLog);
					*infoLog = std::string("Shader File: \"") +fileName +"\"\n" +(*infoLog);
				}
				if(debugInfoLog != nullptr)
					*debugInfoLog = shaderPtr->get_program_debug_info_log();
			}
			else if(infoLog != nullptr)
				*infoLog = "An unknown error has occurred!";
		}
		return false;
	}
	auto shaderMod = Anvil::ShaderModule::create_from_spirv_generator(&dev,shaderPtr.get());
	spirv = shaderMod->get_spirv_blob();
	return true;
}

bool prosper::glsl_to_spv(IPrContext &context,prosper::ShaderStage stage,const std::string &fileName,std::vector<unsigned int> &spirv,std::string *infoLog,std::string *debugInfoLog,bool bReload)
{
	auto fName = fileName;
	std::string ext;
	if(!ufile::get_extension(fileName,&ext))
	{
		auto fNameSpv = "cache/" +fileName +".spv";
		auto bSpvExists = FileManager::Exists(fNameSpv);
		if(bSpvExists == true)
		{
			ext = "spv";
			fName = fNameSpv;
		}
		if(bSpvExists == false || bReload == true)
		{
			auto fNameGls = fileName +".gls";
			if(FileManager::Exists(fNameGls))
			{
				ext = "gls";
				fName = fNameGls;
			}
			else if(bSpvExists == false)
			{
				ext = "hls";
				fName = fileName +".hls";
			}
		}
	}
	ustring::to_lower(ext);
	if(!FileManager::Exists(fName))
	{
		if(infoLog != nullptr)
			*infoLog = std::string("File '") +fName +std::string("' not found!");
		return false;
	}
	if(ext != "gls" && ext != "hls") // We'll assume it's a SPIR-V file
	{
		auto f = FileManager::OpenFile(fName.c_str(),"rb");
		if(f == nullptr)
		{
			if(infoLog != nullptr)
				*infoLog = std::string("Unable to open file '") +fName +std::string("'!");
			return false;
		}
		auto sz = f->GetSize();
		assert((sz %sizeof(unsigned int)) == 0);
		if((sz %sizeof(unsigned int)) != 0)
			return false;
		auto origSize = spirv.size();
		spirv.resize(origSize +sz /sizeof(unsigned int));
		f->Read(spirv.data() +origSize,sz);
		return true;
	}
	std::vector<prosper::glsl::IncludeLine> includeLines;
	unsigned int lineOffset = 0;
	auto shaderCode = load_glsl(context,stage,fName,infoLog,debugInfoLog,includeLines,lineOffset);
	if(shaderCode.has_value() == false)
		return false;
	auto r = ::glsl_to_spv(context,stage,*shaderCode,spirv,infoLog,debugInfoLog,fName,includeLines,lineOffset,(ext == "hls") ? true : false);
	if(r == false)
		return r;
	auto spirvName = "cache/" +fName.substr(0,fName.length() -ext.length()) +"spv";
	FileManager::CreatePath(ufile::get_path_from_filename(spirvName).c_str());
	auto fOut = FileManager::OpenFile<VFilePtrReal>(spirvName.c_str(),"wb");
	if(fOut == nullptr)
		return r;
	fOut->Write(spirv.data(),spirv.size() *sizeof(unsigned int));
	return r;
}

void prosper::util::initialize_image(Anvil::BaseDevice &dev,const uimg::ImageBuffer &imgSrc,IImage &img)
{
	auto extents = img.GetExtents();
	auto w = extents.width;
	auto h = extents.height;
	auto srcFormat = img.GetFormat();
	if(srcFormat != Format::R8G8B8A8_UNorm && srcFormat != Format::R8G8B8_UNorm_PoorCoverage)
		throw std::invalid_argument("Invalid image format for tga target image: Only VK_FORMAT_R8G8B8A8_UNORM and VK_FORMAT_R8G8B8_UNORM are supported!");
	if(imgSrc.GetWidth() != w || imgSrc.GetHeight() != h)
		throw std::invalid_argument("Invalid image extents for tga target image: Extents must be the same for source tga image and target image!");

	auto memBlock = static_cast<VlkImage&>(img)->get_memory_block();
	auto size = w *h *get_byte_size(srcFormat);
	uint8_t *outDataPtr = nullptr;
	memBlock->map(0ull,size,reinterpret_cast<void**>(&outDataPtr));

	auto *imgData = static_cast<const uint8_t*>(imgSrc.GetData());
	auto pos = decltype(imgSrc.GetSize()){0};
	auto tgaFormat = imgSrc.GetFormat();
	auto srcPxSize = get_byte_size(srcFormat);
	auto rowPitch = w *srcPxSize;
	for(auto y=h;y>0;)
	{
		--y;
		auto *row = outDataPtr +y *rowPitch;
		for(auto x=decltype(w){0};x<w;++x)
		{
			auto *px = row;
			px[0] = imgData[pos];
			px[1] = imgData[pos +1];
			px[2] = imgData[pos +2];
			switch(tgaFormat)
			{
			case uimg::ImageBuffer::Format::RGBA8:
			{
				pos += 4;
				if(srcFormat == Format::R8G8B8A8_UNorm)
					px[3] = imgData[pos +3];
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

prosper::util::VendorDeviceInfo prosper::util::get_vendor_device_info(const IPrContext &context)
{
	auto &dev = static_cast<VlkContext&>(const_cast<IPrContext&>(context)).GetDevice();
	auto &gpuProperties = dev.get_physical_device_properties();
	VendorDeviceInfo deviceInfo {};
	deviceInfo.apiVersion = gpuProperties.core_vk1_0_properties_ptr->api_version;
	deviceInfo.deviceType = static_cast<prosper::PhysicalDeviceType>(gpuProperties.core_vk1_0_properties_ptr->device_type);
	deviceInfo.deviceName = gpuProperties.core_vk1_0_properties_ptr->device_name;
	deviceInfo.driverVersion = gpuProperties.core_vk1_0_properties_ptr->driver_version;
	deviceInfo.vendor = static_cast<Vendor>(gpuProperties.core_vk1_0_properties_ptr->vendor_id);
	deviceInfo.deviceId = gpuProperties.core_vk1_0_properties_ptr->device_id;
	return deviceInfo;
}

std::vector<prosper::util::VendorDeviceInfo> prosper::util::get_available_vendor_devices(const IPrContext &context)
{
	auto &instance = static_cast<const VlkContext&>(context).GetAnvilInstance();
	auto numDevices = instance.get_n_physical_devices();
	std::vector<prosper::util::VendorDeviceInfo> devices {};
	devices.reserve(numDevices);
	for(auto i=decltype(numDevices){0u};i<numDevices;++i)
	{
		auto &dev = *instance.get_physical_device(i);
		auto &gpuProperties = dev.get_device_properties();
		VendorDeviceInfo deviceInfo {};
		deviceInfo.apiVersion = gpuProperties.core_vk1_0_properties_ptr->api_version;
		deviceInfo.deviceType = static_cast<prosper::PhysicalDeviceType>(gpuProperties.core_vk1_0_properties_ptr->device_type);
		deviceInfo.deviceName = gpuProperties.core_vk1_0_properties_ptr->device_name;
		deviceInfo.driverVersion = gpuProperties.core_vk1_0_properties_ptr->driver_version;
		deviceInfo.vendor = static_cast<Vendor>(gpuProperties.core_vk1_0_properties_ptr->vendor_id);
		deviceInfo.deviceId = gpuProperties.core_vk1_0_properties_ptr->device_id;
		devices.push_back(deviceInfo);
	}
	return devices;
}

std::optional<prosper::util::PhysicalDeviceMemoryProperties> prosper::util::get_physical_device_memory_properties(const IPrContext &context)
{
	prosper::util::PhysicalDeviceMemoryProperties memProps {};
	auto &vkMemProps = static_cast<VlkContext&>(const_cast<IPrContext&>(context)).GetDevice().get_physical_device_memory_properties();
	auto totalSize = 0ull;
	auto numHeaps = vkMemProps.n_heaps;
	for(auto i=decltype(numHeaps){0};i<numHeaps;++i)
		memProps.heapSizes.push_back(vkMemProps.heaps[i].size);
	return memProps;
}

bool prosper::util::get_memory_stats(IPrContext &context,MemoryPropertyFlags memPropFlags,DeviceSize &outAvailableSize,DeviceSize &outAllocatedSize,std::vector<uint32_t> *optOutMemIndices)
{
	auto &dev = static_cast<VlkContext&>(context).GetDevice();
	auto &memProps = dev.get_physical_device_memory_properties();
	auto allocatedDeviceLocalSize = 0ull;
	auto &memTracker = prosper::MemoryTracker::GetInstance();
	std::vector<uint32_t> deviceLocalTypes = {};
	deviceLocalTypes.reserve(memProps.types.size());
	for(auto i=decltype(memProps.types.size()){0u};i<memProps.types.size();++i)
	{
		auto &type = memProps.types.at(i);
		if(type.heap_ptr == nullptr || (type.flags &static_cast<Anvil::MemoryPropertyFlagBits>(memPropFlags)) == Anvil::MemoryPropertyFlagBits::NONE)
			continue;
		if(deviceLocalTypes.empty() == false)
			assert(type.heap_trp->size() == memProps.types.at(deviceLocalTypes.front()).heap_ptr->size());
		deviceLocalTypes.push_back(i);
		uint64_t allocatedSize = 0ull;
		uint64_t totalSize = 0ull;
		memTracker.GetMemoryStats(context,i,allocatedSize,totalSize);
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

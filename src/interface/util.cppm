// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/device.h>

export module pragma.prosper.vulkan:util;

export import pragma.image;
export import pragma.prosper;

export namespace prosper {
	namespace util {
		PR_EXPORT void initialize_image(Anvil::BaseDevice &dev, const uimg::ImageBuffer &imgSrc, IImage &img);
		PR_EXPORT util::VendorDeviceInfo get_vendor_device_info(const Anvil::PhysicalDevice &dev);
		PR_EXPORT util::VendorDeviceInfo get_vendor_device_info(const IPrContext &context);
		PR_EXPORT std::vector<util::VendorDeviceInfo> get_available_vendor_devices(const IPrContext &context);
		PR_EXPORT std::optional<util::PhysicalDeviceMemoryProperties> get_physical_device_memory_properties(const IPrContext &context);
		PR_EXPORT bool get_memory_stats(IPrContext &context, MemoryPropertyFlags memPropFlags, DeviceSize &outAvailableSize, DeviceSize &outAllocatedSize, std::vector<uint32_t> *optOutMemIndices = nullptr);
	};
	std::unique_ptr<Anvil::DescriptorSetCreateInfo> ToAnvilDescriptorSetInfo(const DescriptorSetInfo &descSetInfo);
	PR_EXPORT bool glsl_to_spv(IPrContext &context, prosper::ShaderStage stage, const std::string &shaderRootPath, const std::string &fileName, std::vector<unsigned int> &spirv, std::string *infoLog, std::string *debugInfoLog, bool bReload, const std::string &prefixCode = {},
	  const std::unordered_map<std::string, std::string> &definitions = {}, bool withDebugInfo = false);
	PR_EXPORT std::optional<std::unordered_map<prosper::ShaderStage, std::string>> optimize_glsl(prosper::IPrContext &context, const std::unordered_map<prosper::ShaderStage, std::string> &shaderStages, std::string &outInfoLog);
};

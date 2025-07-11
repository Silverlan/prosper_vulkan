// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __VK_UTIL_HPP__
#define __VK_UTIL_HPP__

#include "prosper_vulkan_definitions.hpp"
#include <memory>
#include <string>
#include <vector>
#include <optional>

namespace Anvil {
	class DescriptorSetCreateInfo;
	class BaseDevice;
};
namespace uimg {
	class ImageBuffer;
};
namespace prosper {
	namespace util {
		struct VendorDeviceInfo;
		struct PhysicalDeviceMemoryProperties;
		DLLPROSPER_VK void initialize_image(Anvil::BaseDevice &dev, const uimg::ImageBuffer &imgSrc, IImage &img);
		DLLPROSPER_VK util::VendorDeviceInfo get_vendor_device_info(const Anvil::PhysicalDevice &dev);
		DLLPROSPER_VK util::VendorDeviceInfo get_vendor_device_info(const IPrContext &context);
		DLLPROSPER_VK std::vector<util::VendorDeviceInfo> get_available_vendor_devices(const IPrContext &context);
		DLLPROSPER_VK std::optional<util::PhysicalDeviceMemoryProperties> get_physical_device_memory_properties(const IPrContext &context);
		DLLPROSPER_VK bool get_memory_stats(IPrContext &context, MemoryPropertyFlags memPropFlags, DeviceSize &outAvailableSize, DeviceSize &outAllocatedSize, std::vector<uint32_t> *optOutMemIndices = nullptr);
	};
	class IImage;
	class IPrContext;
	struct DescriptorSetInfo;
	enum class ShaderStage : uint8_t;
	std::unique_ptr<Anvil::DescriptorSetCreateInfo> ToAnvilDescriptorSetInfo(const DescriptorSetInfo &descSetInfo);
	DLLPROSPER_VK bool glsl_to_spv(IPrContext &context, prosper::ShaderStage stage, const std::string &shaderRootPath, const std::string &fileName, std::vector<unsigned int> &spirv, std::string *infoLog, std::string *debugInfoLog, bool bReload, const std::string &prefixCode = {},
	  const std::unordered_map<std::string, std::string> &definitions = {}, bool withDebugInfo = false);
	DLLPROSPER_VK std::optional<std::unordered_map<prosper::ShaderStage, std::string>> optimize_glsl(prosper::IPrContext &context, const std::unordered_map<prosper::ShaderStage, std::string> &shaderStages, std::string &outInfoLog);
};

#endif

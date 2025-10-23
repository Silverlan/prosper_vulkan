// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"
#include <vulkan/vulkan.h>
#include <memory>

export module pragma.prosper.vulkan:raytracing.acceleration_structure;

export import pragma.prosper;

export namespace prosper {
	class DLLPROSPER_VK VlkAccelerationStructure : public ContextObject, public std::enable_shared_from_this<VlkAccelerationStructure> {
	  public:
		static std::shared_ptr<VlkAccelerationStructure> Create(IPrContext &context);

		virtual ~VlkAccelerationStructure() override;

		VkAccelerationStructureKHR GetVkAccelerationStructure() const { return m_vkAccStruct; };
	  protected:
		VlkAccelerationStructure(IPrContext &context, VkAccelerationStructureKHR vkAccStruct, VkAllocationCallbacks vkAllocCallbacks);

		VkAccelerationStructureKHR m_vkAccStruct;
		VkAllocationCallbacks m_vkAllocCallbacks;
	};
};

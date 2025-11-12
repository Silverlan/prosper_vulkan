// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "vulkan_api.hpp"

export module pragma.prosper.vulkan:raytracing.acceleration_structure;

export import pragma.prosper;

export namespace prosper {
	class PR_EXPORT VlkAccelerationStructure : public ContextObject, public std::enable_shared_from_this<VlkAccelerationStructure> {
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

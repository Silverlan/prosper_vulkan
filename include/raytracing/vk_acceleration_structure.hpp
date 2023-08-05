/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VR_ACCELERATION_STRUCTURE_HPP__
#define __PR_PROSPER_VR_ACCELERATION_STRUCTURE_HPP__

#include "prosper_vulkan_definitions.hpp"
#include <prosper_includes.hpp>
#include <prosper_context_object.hpp>
#include <vulkan/vulkan.h>

namespace prosper {
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

#endif

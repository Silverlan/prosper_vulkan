// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/device.h>

module pragma.prosper.vulkan;

import :raytracing.acceleration_structure;

using namespace prosper;

std::shared_ptr<VlkAccelerationStructure> VlkAccelerationStructure::Create(IPrContext &context)
{
	// TODO: This is a stub
	auto &vlkContext = static_cast<VlkContext &>(context);
	if(vlkContext.GetRaytracingFunctions().IsValid() == false)
		return nullptr;
	auto vkDevice = vlkContext.GetDevice().get_device_vk();
	VkAccelerationStructureCreateInfoKHR createInfo {};
	createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	createInfo.type = VkAccelerationStructureTypeKHR::VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR;                                              // TODO
	createInfo.size = 0;                                                                                                                       // TODO
	createInfo.offset = 0;                                                                                                                     // TODO
	createInfo.buffer = nullptr;                                                                                                               // TODO
	createInfo.deviceAddress = {};                                                                                                             // TODO
	createInfo.createFlags = VkAccelerationStructureCreateFlagBitsKHR::VK_ACCELERATION_STRUCTURE_CREATE_DEVICE_ADDRESS_CAPTURE_REPLAY_BIT_KHR; // TODO

	VkAllocationCallbacks allocCallbacks {};
	// TODO

	VkAccelerationStructureKHR vkAccStruct;
	auto result = vlkContext.GetRaytracingFunctions().vkCreateAccelerationStructureKHR(vkDevice, &createInfo, &allocCallbacks, &vkAccStruct);
	if(result != VK_SUCCESS)
		return nullptr;
	return std::shared_ptr<VlkAccelerationStructure> {new VlkAccelerationStructure {context, vkAccStruct, allocCallbacks}};
}

VlkAccelerationStructure::~VlkAccelerationStructure()
{
	auto &vlkContext = static_cast<VlkContext &>(GetContext());
	vlkContext.GetRaytracingFunctions().vkDestroyAccelerationStructureKHR(vlkContext.GetDevice().get_device_vk(), m_vkAccStruct, &m_vkAllocCallbacks);
}

VlkAccelerationStructure::VlkAccelerationStructure(IPrContext &context, VkAccelerationStructureKHR vkAccStruct, VkAllocationCallbacks vkAllocCallbacks) : ContextObject {context}, m_vkAccStruct {vkAccStruct}, m_vkAllocCallbacks {vkAllocCallbacks} {}

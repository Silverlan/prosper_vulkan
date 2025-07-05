// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_prosper_vulkan.h"
#include "vk_memory_tracker.hpp"
#include "vk_context.hpp"
#include "image/vk_image.hpp"
#include "buffers/vk_buffer.hpp"
#include "buffers/vk_uniform_resizable_buffer.hpp"
#include "buffers/vk_dynamic_resizable_buffer.hpp"
#include <wrappers/device.h>
#include <wrappers/memory_block.h>

using namespace prosper;

Anvil::MemoryBlock *MemoryTracker::Resource::GetMemoryBlock(uint32_t i) const
{
	if(resource == nullptr)
		return nullptr;
	if((typeFlags & Resource::TypeFlags::BufferBit) != Resource::TypeFlags::None) {
		auto &anvBuffer = static_cast<prosper::VlkBuffer *>(resource)->GetAnvilBuffer();
		if(i >= anvBuffer.get_n_memory_blocks())
			return nullptr;
		return anvBuffer.get_memory_block(i);
	}
	if((typeFlags & Resource::TypeFlags::ImageBit) != Resource::TypeFlags::None) {
		if(i > 0u)
			return nullptr;
		return static_cast<prosper::VlkImage *>(resource)->GetAnvilImage().get_memory_block();
	}
	return nullptr;
}
uint32_t MemoryTracker::Resource::GetMemoryBlockCount() const
{
	if(resource == nullptr)
		return 0u;
	if((typeFlags & Resource::TypeFlags::BufferBit) != Resource::TypeFlags::None)
		return static_cast<prosper::VlkBuffer *>(resource)->GetAnvilBuffer().get_n_memory_blocks();
	if((typeFlags & Resource::TypeFlags::ImageBit) != Resource::TypeFlags::None)
		return 1u;
	return 0u;
}

MemoryTracker &MemoryTracker::GetInstance()
{
	static MemoryTracker r {};
	return r;
}
bool MemoryTracker::GetMemoryStats(prosper::IPrContext &context, uint32_t memType, uint64_t &allocatedSize, uint64_t &totalSize, Resource::TypeFlags typeFlags) const
{
	auto &dev = static_cast<VlkContext &>(context).GetDevice();
	auto &memProps = dev.get_physical_device_memory_properties();
	if(memType >= memProps.types.size() || memProps.types.at(memType).heap_ptr == nullptr)
		return false;
	totalSize = memProps.types.at(memType).heap_ptr->size;
	allocatedSize = 0ull;
	std::scoped_lock lock {m_resourceMutex};
	for(auto &res : m_resources) {
		if(res.resource == nullptr || (res.typeFlags & typeFlags) == Resource::TypeFlags::None)
			continue;
		auto numMemoryBlocks = res.GetMemoryBlockCount();
		for(auto i = decltype(numMemoryBlocks) {0u}; i < numMemoryBlocks; ++i) {
			auto *mem = res.GetMemoryBlock(i);
			auto *pInfo = (mem != nullptr) ? mem->get_create_info_ptr() : nullptr;
			if(pInfo == nullptr || pInfo->get_memory_type_index() != memType)
				continue;
			allocatedSize += pInfo->get_size();
		}
	}
	return true;
}
const std::vector<MemoryTracker::Resource> &MemoryTracker::GetResources() const { return m_resources; };
void MemoryTracker::GetResources(uint32_t memType, std::vector<const Resource *> &outResources, Resource::TypeFlags typeFlags) const
{
	std::scoped_lock lock {m_resourceMutex};
	outResources.reserve(m_resources.size());
	for(auto &res : m_resources) {
		if(res.resource == nullptr || (res.typeFlags & typeFlags) == Resource::TypeFlags::None)
			continue;
		auto numMemoryBlocks = res.GetMemoryBlockCount();
		for(auto i = decltype(numMemoryBlocks) {0u}; i < numMemoryBlocks; ++i) {
			auto *mem = res.GetMemoryBlock(i);
			auto *pInfo = (mem != nullptr) ? mem->get_create_info_ptr() : nullptr;
			if(pInfo == nullptr || pInfo->get_memory_type_index() != memType)
				continue;
			outResources.push_back(&res);
			break;
		}
	}
}
void MemoryTracker::AddResource(IBuffer &buffer)
{
	if(buffer.GetParent() != nullptr)
		return; // We're already tracking the top-most buffer, we usually don't care about child-buffers
	auto typeFlags = Resource::TypeFlags::BufferBit;
	if(dynamic_cast<VkDynamicResizableBuffer *>(&buffer) != nullptr)
		typeFlags |= Resource::TypeFlags::DynamicBufferBit;
	else if(dynamic_cast<VkUniformResizableBuffer *>(&buffer) != nullptr)
		typeFlags |= Resource::TypeFlags::UniformBufferBit;
	else
		typeFlags |= Resource::TypeFlags::StandAloneBufferBit;
	std::scoped_lock lock {m_resourceMutex};
	m_resources.push_back(Resource {&buffer, typeFlags});
}
void MemoryTracker::AddResource(IImage &image)
{
	std::scoped_lock lock {m_resourceMutex};
	m_resources.push_back(Resource {&image, Resource::TypeFlags::ImageBit});
}
void MemoryTracker::RemoveResource(void *ptrResource)
{
	std::scoped_lock lock {m_resourceMutex};
	auto it = std::find_if(m_resources.begin(), m_resources.end(), [&ptrResource](const Resource &resource) { return ptrResource == resource.resource; });
	if(it == m_resources.end())
		return;
	m_resources.erase(it);
}
void MemoryTracker::RemoveResource(IBuffer &buffer) { RemoveResource(&buffer); }
void MemoryTracker::RemoveResource(IImage &image) { RemoveResource(&image); }

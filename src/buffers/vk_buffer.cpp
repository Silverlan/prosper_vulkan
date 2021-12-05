/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "buffers/vk_buffer.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include "vk_memory_tracker.hpp"
#include <wrappers/buffer.h>
#include <wrappers/memory_block.h>
#include <misc/buffer_create_info.h>
#if 0
#include "vk_context.hpp"
#include <wrappers/device.h>
#include <wrappers/instance.h>
static void interop_text(prosper::VlkContext &context,uint32_t w,uint32_t h)
{
	// https://github.com/jherico/Vulkan/blob/cpp/examples/glinterop/glinterop.cpp#L267
	prosper::util::ImageCreateInfo imgCreateInfo {};
	imgCreateInfo.width = w;
	imgCreateInfo.height = h;
	imgCreateInfo.usage = prosper::ImageUsageFlags::SampledBit;
	imgCreateInfo.tiling = prosper::ImageTiling::Linear;
	imgCreateInfo.flags |= prosper::util::ImageCreateInfo::Flags::DontAllocateMemory;
	imgCreateInfo.postCreateLayout = prosper::ImageLayout::ColorAttachmentOptimal;

	auto img = context.CreateImage(imgCreateInfo);

	context.GetDevice().get_device_vk();
    vk::MemoryRequirements memReqs = device.getImageMemoryRequirementxs(texture.image);
    vk::MemoryAllocateInfo memAllocInfo;
    vk::ExportMemoryAllocateInfo exportAllocInfo{ vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32 };
    memAllocInfo.pNext = &exportAllocInfo;
    memAllocInfo.allocationSize = texture.allocSize = memReqs.size;
    memAllocInfo.memoryTypeIndex = context.getMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    texture.memory = device.allocateMemory(memAllocInfo);
    device.bindImageMemory(texture.image, texture.memory, 0);
   // handles.memory = device.getMemoryWin32HandleKHR({ texture.memory, vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32 }, dynamicLoader);

	auto tex = context.CreateTexture({},*img,prosper::util::ImageViewCreateInfo{},prosper::util::SamplerCreateInfo{});

}
#endif

std::mutex &get_mem_alloc_mutex();
prosper::VlkBuffer::VlkBuffer(IPrContext &context,const util::BufferCreateInfo &bufCreateInfo,DeviceSize startOffset,DeviceSize size,Anvil::BufferUniquePtr buf)
	: IBuffer{context,bufCreateInfo,startOffset,size},m_buffer{std::move(buf)}
{
	if(m_buffer != nullptr)
	{
		auto &memAllocMutex = get_mem_alloc_mutex();
		memAllocMutex.lock();
			m_buffer->get_buffer(); // This will invoke memory allocation and is not thread-safe!
		memAllocMutex.unlock();
		prosper::debug::register_debug_object(m_buffer->get_buffer(),*this,prosper::debug::ObjectType::Buffer);
		m_vkBuffer = m_buffer->get_buffer();
	}
	m_apiTypePtr = this;
}
prosper::VlkBuffer::~VlkBuffer()
{
	MemoryTracker::GetInstance().RemoveResource(*this);
	if(m_buffer != nullptr)
		prosper::debug::deregister_debug_object(m_buffer->get_buffer());
}
void prosper::VlkBuffer::Initialize()
{
	IBuffer::Initialize();
	MemoryTracker::GetInstance().AddResource(*this);
}
std::shared_ptr<prosper::VlkBuffer> prosper::VlkBuffer::Create(IPrContext &context,Anvil::BufferUniquePtr buf,const util::BufferCreateInfo &bufCreateInfo,DeviceSize startOffset,DeviceSize size,const std::function<void(IBuffer&)> &onDestroyedCallback)
{
	if(buf == nullptr)
		return nullptr;
	auto r = std::shared_ptr<VlkBuffer>(new VlkBuffer(context,bufCreateInfo,startOffset,size,std::move(buf)),[onDestroyedCallback](VlkBuffer *buf) {
		buf->OnRelease();
		if(onDestroyedCallback != nullptr)
			onDestroyedCallback(*buf);
		buf->GetContext().ReleaseResource<VlkBuffer>(buf);
		});
	r->Initialize();
	return r;
}

std::shared_ptr<prosper::VlkBuffer> prosper::VlkBuffer::GetParent() {return std::dynamic_pointer_cast<VlkBuffer>(IBuffer::GetParent());}
const std::shared_ptr<prosper::VlkBuffer> prosper::VlkBuffer::GetParent() const {return const_cast<VlkBuffer*>(this)->GetParent();}

std::shared_ptr<prosper::IBuffer> prosper::VlkBuffer::CreateSubBuffer(DeviceSize offset,DeviceSize size,const std::function<void(IBuffer&)> &onDestroyedCallback)
{
	auto subBufferCreateInfo = m_createInfo;
	subBufferCreateInfo.size = size;
	auto buf = Anvil::Buffer::create(Anvil::BufferCreateInfo::create_no_alloc_child(&GetAnvilBuffer(),offset,size));
	auto subBuffer = Create(GetContext(),std::move(buf),subBufferCreateInfo,(m_parent ? m_parent->GetStartOffset() : 0ull) +offset,size,onDestroyedCallback);
	subBuffer->SetParent(*this);
	return subBuffer;
}

Anvil::Buffer &prosper::VlkBuffer::GetAnvilBuffer() const {return *m_buffer;}
Anvil::Buffer &prosper::VlkBuffer::GetBaseAnvilBuffer() const {return m_parent ? GetParent()->GetAnvilBuffer() : GetAnvilBuffer();}
Anvil::Buffer &prosper::VlkBuffer::operator*() {return *m_buffer;}
const Anvil::Buffer &prosper::VlkBuffer::operator*() const {return const_cast<VlkBuffer*>(this)->operator*();}
Anvil::Buffer *prosper::VlkBuffer::operator->() {return m_buffer.get();}
const Anvil::Buffer *prosper::VlkBuffer::operator->() const {return const_cast<VlkBuffer*>(this)->operator->();}

bool prosper::VlkBuffer::DoWrite(Offset offset,Size size,const void *data) const
{
	if(size == 0)
		return true;
	return m_buffer->get_memory_block(0u)->write(offset,size,data);
}
bool prosper::VlkBuffer::DoRead(Offset offset,Size size,void *data) const
{
	if(size == 0)
		return true;
	return m_buffer->get_memory_block(0u)->read(offset,size,data);
}
bool prosper::VlkBuffer::DoMap(Offset offset,Size size,MapFlags mapFlags,void **optOutMappedPtr) const
{
	if(size == 0)
		return false;
	return m_buffer->get_memory_block(0u)->map(offset,size,optOutMappedPtr);
}
bool prosper::VlkBuffer::DoUnmap() const {return m_buffer->get_memory_block(0u)->unmap();}

void prosper::VlkBuffer::RecreateInternalSubBuffer(IBuffer &newParentBuffer)
{
	SetBuffer(Anvil::Buffer::create(Anvil::BufferCreateInfo::create_no_alloc_child(&newParentBuffer.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),GetStartOffset(),GetSize())));
}

void prosper::VlkBuffer::SetBuffer(Anvil::BufferUniquePtr buf)
{
	auto bPermanentlyMapped = m_permanentlyMapped;
	SetPermanentlyMapped(false,prosper::IBuffer::MapFlags::None);

	if(m_buffer != nullptr)
		prosper::debug::deregister_debug_object(m_buffer->get_buffer());
	m_buffer = std::move(buf);
	if(m_buffer != nullptr)
	{
		prosper::debug::register_debug_object(m_buffer->get_buffer(),*this,prosper::debug::ObjectType::Buffer);
		m_vkBuffer = m_buffer->get_buffer();
	}

	if(m_permanentlyMapped.has_value())
		SetPermanentlyMapped(true,*m_permanentlyMapped);
}

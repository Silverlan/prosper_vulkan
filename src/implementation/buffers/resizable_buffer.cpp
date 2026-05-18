// SPDX-FileCopyrightText: (c) 2026 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/device.h>

module pragma.prosper.vulkan;

import :buffer.resizable_buffer;

using namespace prosper;

VkResizableBuffer::VkResizableBuffer(IBuffer &buffer)
    : IResizableBuffer {buffer}, IBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize()}, VlkBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize(), nullptr}
{
	VlkBuffer::m_buffer = std::move(buffer.GetAPITypeRef<VlkBuffer>().m_buffer);
	VlkBuffer::m_vkBuffer = VlkBuffer::m_buffer->get_buffer();

	debug::deregister_debug_object(m_buffer->get_buffer());
	debug::register_debug_object(m_buffer->get_buffer(), *this, debug::ObjectType::Buffer);
}

void VkResizableBuffer::MoveInternalBuffer(IBuffer &other) { SetBuffer(std::move(other.GetAPITypeRef<VlkBuffer>().m_buffer)); }

void VkResizableBuffer::ReleaseBufferSafely()
{
	if(!m_buffer)
		return;
	debug::deregister_debug_object(m_buffer->get_buffer());
	std::shared_ptr keepAliveResource = std::move(m_buffer);
	m_buffer = {};
	GetContext().KeepResourceAliveUntilPresentationComplete(keepAliveResource);
}

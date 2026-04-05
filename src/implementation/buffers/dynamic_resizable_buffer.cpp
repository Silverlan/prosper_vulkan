// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/device.h>

module pragma.prosper.vulkan;

import :buffer.dynamic_resizable_buffer;

using namespace prosper;

VkDynamicResizableBuffer::VkDynamicResizableBuffer(IPrContext &context, IBuffer &buffer, const util::BufferCreateInfo &createInfo)
    : IDynamicResizableBuffer {context, buffer, createInfo}, IBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize()}, VlkBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize(), nullptr}
{
	VlkBuffer::m_buffer = std::move(buffer.GetAPITypeRef<VlkBuffer>().m_buffer);
	VlkBuffer::m_vkBuffer = VlkBuffer::m_buffer->get_buffer();
}

void VkDynamicResizableBuffer::MoveInternalBuffer(IBuffer &other) { SetBuffer(std::move(other.GetAPITypeRef<VlkBuffer>().m_buffer)); }

void VkDynamicResizableBuffer::ReleaseBufferSafely()
{
	if(!m_buffer)
		return;
	std::shared_ptr keepAliveResource = std::move(m_buffer);
	m_buffer = {};
	GetContext().KeepResourceAliveUntilPresentationComplete(keepAliveResource);
}

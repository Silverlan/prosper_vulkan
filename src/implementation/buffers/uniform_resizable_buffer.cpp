// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "vulkan_api.hpp"
#include <wrappers/device.h>

module pragma.prosper.vulkan;

import :buffer.uniform_resizable_buffer;

using namespace prosper;

prosper::VkUniformResizableBuffer::VkUniformResizableBuffer(IPrContext &context, IBuffer &buffer, uint64_t bufferInstanceSize, uint64_t alignedBufferBaseSize, uint64_t maxTotalSize, uint32_t alignment)
    : IUniformResizableBuffer {context, buffer, bufferInstanceSize, alignedBufferBaseSize, maxTotalSize, alignment}, IBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize()}, VlkBuffer {buffer.GetContext(), buffer.GetCreateInfo(),
                                                                                                                                                                                                                         buffer.GetStartOffset(), buffer.GetSize(), nullptr}
{
	VlkBuffer::m_buffer = std::move(buffer.GetAPITypeRef<VlkBuffer>().m_buffer);
	VlkBuffer::m_vkBuffer = VlkBuffer::m_buffer->get_buffer();
}

void prosper::VkUniformResizableBuffer::MoveInternalBuffer(IBuffer &other) { SetBuffer(std::move(other.GetAPITypeRef<VlkBuffer>().m_buffer)); }

// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "prosper_vulkan_definitions.hpp"
#include "stdafx_prosper_vulkan.h"
#include "buffers/vk_dynamic_resizable_buffer.hpp"
#include "prosper_util.hpp"
#include "buffers/prosper_buffer.hpp"
#include "vk_context.hpp"
#include <wrappers/device.h>

using namespace prosper;

prosper::VkDynamicResizableBuffer::VkDynamicResizableBuffer(IPrContext &context, IBuffer &buffer, const util::BufferCreateInfo &createInfo, uint64_t maxTotalSize)
    : IDynamicResizableBuffer {context, buffer, createInfo, maxTotalSize}, IBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize()}, VlkBuffer {buffer.GetContext(), buffer.GetCreateInfo(), buffer.GetStartOffset(), buffer.GetSize(), nullptr}
{
	VlkBuffer::m_buffer = std::move(buffer.GetAPITypeRef<VlkBuffer>().m_buffer);
	VlkBuffer::m_vkBuffer = VlkBuffer::m_buffer->get_buffer();
}

void prosper::VkDynamicResizableBuffer::MoveInternalBuffer(IBuffer &other) { SetBuffer(std::move(other.GetAPITypeRef<VlkBuffer>().m_buffer)); }

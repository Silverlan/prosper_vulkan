/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prosper_vulkan_definitions.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include "stdafx_prosper_vulkan.h"
#include "buffers/vk_uniform_resizable_buffer.hpp"
#include "prosper_util.hpp"
#include "buffers/prosper_buffer.hpp"
#include "vk_context.hpp"
#include <wrappers/device.h>

using namespace prosper;

prosper::VkUniformResizableBuffer::VkUniformResizableBuffer(
	IPrContext &context,IBuffer &buffer,
	uint64_t bufferInstanceSize,
	uint64_t alignedBufferBaseSize,uint64_t maxTotalSize,uint32_t alignment
)
	: IUniformResizableBuffer{context,buffer,bufferInstanceSize,alignedBufferBaseSize,maxTotalSize,alignment},
	IBuffer{buffer.GetContext(),buffer.GetCreateInfo(),buffer.GetStartOffset(),buffer.GetSize()},
	VlkBuffer{buffer.GetContext(),buffer.GetCreateInfo(),buffer.GetStartOffset(),buffer.GetSize(),nullptr}
{
	VlkBuffer::m_buffer = std::move(buffer.GetAPITypeRef<VlkBuffer>().m_buffer);
	VlkBuffer::m_vkBuffer = VlkBuffer::m_buffer->get_buffer();
}

void prosper::VkUniformResizableBuffer::MoveInternalBuffer(IBuffer &other)
{
	SetBuffer(std::move(other.GetAPITypeRef<VlkBuffer>().m_buffer));
}

/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prosper_vulkan_definitions.hpp"
#include "stdafx_prosper_vulkan.h"
#include "buffers/vk_dynamic_resizable_buffer.hpp"
#include "prosper_util.hpp"
#include "buffers/prosper_buffer.hpp"
#include "vk_context.hpp"
#include <wrappers/device.h>

using namespace prosper;

prosper::VkDynamicResizableBuffer::VkDynamicResizableBuffer(
	IPrContext &context,IBuffer &buffer,const util::BufferCreateInfo &createInfo,uint64_t maxTotalSize
)
	: IDynamicResizableBuffer{context,buffer,createInfo,maxTotalSize},
	IBuffer{buffer.GetContext(),buffer.GetCreateInfo(),buffer.GetStartOffset(),buffer.GetSize()},
	VlkBuffer{buffer.GetContext(),buffer.GetCreateInfo(),buffer.GetStartOffset(),buffer.GetSize(),nullptr}
{
	VlkBuffer::m_buffer = std::move(dynamic_cast<VlkBuffer&>(buffer).m_buffer);
}

void prosper::VkDynamicResizableBuffer::MoveInternalBuffer(IBuffer &other)
{
	SetBuffer(std::move(dynamic_cast<VlkBuffer&>(other).m_buffer));
}

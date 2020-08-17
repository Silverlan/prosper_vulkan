/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "buffers/vk_render_buffer.hpp"
#include "buffers/vk_buffer.hpp"
#include "vk_context.hpp"

using namespace prosper;

#pragma optimize("",off)
VlkRenderBuffer::VlkRenderBuffer(
	prosper::IPrContext &context,const std::vector<prosper::IBuffer*> &buffers,const std::vector<prosper::DeviceSize> &offsets,const std::optional<IndexBufferInfo> &indexBufferInfo
)
	: IRenderBuffer{context,buffers,indexBufferInfo},m_offsets{offsets}
{}
std::shared_ptr<VlkRenderBuffer> VlkRenderBuffer::Create(
	prosper::VlkContext &context,const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo,const std::vector<prosper::IBuffer*> &buffers,
	const std::vector<prosper::DeviceSize> &offsets,const std::optional<IndexBufferInfo> &indexBufferInfo
)
{
	return std::shared_ptr<VlkRenderBuffer>{new VlkRenderBuffer{context,buffers,offsets,indexBufferInfo}};
}
const std::vector<prosper::DeviceSize> &VlkRenderBuffer::GetOffsets() const {return m_offsets;}
#pragma optimize("",on)

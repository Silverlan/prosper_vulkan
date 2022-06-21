/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "buffers/vk_render_buffer.hpp"
#include "buffers/vk_buffer.hpp"
#include "vk_context.hpp"
#include <wrappers/command_buffer.h>

using namespace prosper;


VlkRenderBuffer::VlkRenderBuffer(
	prosper::IPrContext &context,const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo,
	const std::vector<prosper::IBuffer*> &buffers,const std::vector<prosper::DeviceSize> &offsets,
	const std::optional<IndexBufferInfo> &indexBufferInfo
)
	: IRenderBuffer{context,pipelineCreateInfo,buffers,offsets,indexBufferInfo}
{
	Initialize();
}
std::shared_ptr<VlkRenderBuffer> VlkRenderBuffer::Create(
	prosper::VlkContext &context,const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo,const std::vector<prosper::IBuffer*> &buffers,
	const std::vector<prosper::DeviceSize> &offsets,const std::optional<IndexBufferInfo> &indexBufferInfo
)
{
	return std::shared_ptr<VlkRenderBuffer>{new VlkRenderBuffer{context,pipelineCreateInfo,buffers,offsets,indexBufferInfo}};
}
bool VlkRenderBuffer::Record(VkCommandBuffer cmdBuf) const
{
	// return cmdBuf.record_bind_vertex_buffers(0u,m_anvBuffers.size(),m_anvBuffers.data(),m_anvOffsets.data()) &&
	// 	(!m_anvIndexBuffer || cmdBuf.record_bind_index_buffer(m_anvIndexBuffer,m_anvIndexBufferOffset,static_cast<Anvil::IndexType>(m_indexBufferInfo->indexType)));
	vkCmdBindVertexBuffers(cmdBuf,0u /* firstBinding */,m_vkBuffers.size(),m_vkBuffers.data(),m_vkOffsets.data());
	if(m_vkIndexBuffer)
		vkCmdBindIndexBuffer(cmdBuf,m_vkIndexBuffer,m_vkIndexBufferOffset,static_cast<VkIndexType>(m_indexBufferInfo->indexType));
	return true;
}
void VlkRenderBuffer::Reload()
{
	m_vkBuffers.clear();
	m_vkOffsets.clear();
	m_vkIndexBuffer = nullptr;
	m_vkIndexBufferOffset = 0;
	Initialize();
}
void VlkRenderBuffer::Initialize()
{
	m_vkBuffers.reserve(m_buffers.size());
	for(auto &buf : m_buffers)
		m_vkBuffers.push_back(buf->GetAPITypeRef<VlkBuffer>().GetVkBuffer());
	if(m_offsets.empty())
		m_vkOffsets.resize(m_buffers.size(),0);
	else
		m_vkOffsets = m_offsets;
	for(auto i=decltype(m_buffers.size()){0u};i<m_buffers.size();++i)
		m_vkOffsets.at(i) += m_buffers.at(i)->GetStartOffset();

	if(m_indexBufferInfo.has_value())
	{
		m_vkIndexBuffer = m_indexBufferInfo->buffer->GetAPITypeRef<VlkBuffer>().GetVkBuffer();
		m_vkIndexBufferOffset = m_indexBufferInfo->buffer->GetStartOffset() +m_indexBufferInfo->offset;
	}
}

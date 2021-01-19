/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_command_buffer.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include "shader/prosper_shader.hpp"
#include "buffers/vk_buffer.hpp"
#include "buffers/vk_render_buffer.hpp"
#include "image/vk_image.hpp"
#include "image/vk_image_view.hpp"
#include "queries/prosper_occlusion_query.hpp"
#include "queries/prosper_query_pool.hpp"
#include "queries/prosper_timestamp_query.hpp"
#include "queries/prosper_pipeline_statistics_query.hpp"
#include "queries/vk_query_pool.hpp"
#include "vk_context.hpp"
#include "vk_framebuffer.hpp"
#include "vk_render_pass.hpp"
#include "vk_descriptor_set_group.hpp"
#include <debug/prosper_debug.hpp>
#include <wrappers/buffer.h>
#include <wrappers/memory_block.h>
#include <wrappers/command_buffer.h>
#include <wrappers/framebuffer.h>
#include <wrappers/command_pool.h>
#include <misc/render_pass_create_info.h>
#include <misc/buffer_create_info.h>
#include <sharedutils/util.h>
#include <sstream>

// Note: Most command buffer methods use the vulkan functions directly instead of Anvil, because
// Anvil includes an additional mutex overhead

Anvil::CommandBufferBase &prosper::VlkCommandBuffer::GetAnvilCommandBuffer() const {return *m_cmdBuffer;}
Anvil::CommandBufferBase &prosper::VlkCommandBuffer::operator*() {return *m_cmdBuffer;}
const Anvil::CommandBufferBase &prosper::VlkCommandBuffer::operator*() const {return const_cast<VlkCommandBuffer*>(this)->operator*();}
Anvil::CommandBufferBase *prosper::VlkCommandBuffer::operator->() {return m_cmdBuffer.get();}
const Anvil::CommandBufferBase *prosper::VlkCommandBuffer::operator->() const {return const_cast<VlkCommandBuffer*>(this)->operator->();}

bool prosper::VlkCommandBuffer::RecordBindDescriptorSets(
	PipelineBindPoint bindPoint,prosper::Shader &shader,PipelineID pipelineIdx,uint32_t firstSet,const std::vector<prosper::IDescriptorSet*> &descSets,const std::vector<uint32_t> dynamicOffsets
)
{
	std::vector<Anvil::DescriptorSet*> anvDescSets {};
	anvDescSets.reserve(descSets.size());
	for(auto *ds : descSets)
	{
		UpdateLastUsageTimes(*ds);
		anvDescSets.push_back(&static_cast<prosper::VlkDescriptorSet&>(*ds).GetAnvilDescriptorSet());
	}
	prosper::PipelineID pipelineId;
	return shader.GetPipelineId(pipelineId,pipelineIdx) && (*this)->record_bind_descriptor_sets(
		static_cast<Anvil::PipelineBindPoint>(bindPoint),static_cast<VlkContext&>(GetContext()).GetPipelineLayout(shader.IsGraphicsShader(),pipelineId),firstSet,anvDescSets.size(),anvDescSets.data(),
		dynamicOffsets.size(),dynamicOffsets.data()
	);
}

bool prosper::VlkCommandBuffer::RecordBindDescriptorSets(
	PipelineBindPoint bindPoint,const IShaderPipelineLayout &pipelineLayout,uint32_t firstSet,
	const std::vector<prosper::IDescriptorSet*> &descSets,const std::vector<uint32_t> dynamicOffsets
)
{
	std::vector<VkDescriptorSet> vkDescSets {};
	vkDescSets.reserve(descSets.size());
	for(auto *ds : descSets)
	{
		UpdateLastUsageTimes(*ds);
		vkDescSets.push_back(static_cast<prosper::VlkDescriptorSet&>(*ds).GetVkDescriptorSet());
	}
	vkCmdBindDescriptorSets(
		m_vkCommandBuffer,static_cast<VkPipelineBindPoint>(bindPoint),static_cast<const VlkShaderPipelineLayout&>(pipelineLayout).GetVkPipelineLayout(),
		firstSet,vkDescSets.size(),vkDescSets.data(),dynamicOffsets.size(),dynamicOffsets.data()
	);
	return true;
}

bool prosper::VlkCommandBuffer::RecordBindDescriptorSets(
	PipelineBindPoint bindPoint,const IShaderPipelineLayout &pipelineLayout,uint32_t firstSet,
	const prosper::IDescriptorSet &descSet,uint32_t *optDynamicOffset
)
{
	UpdateLastUsageTimes(const_cast<IDescriptorSet&>(descSet));
	auto vkDescSet = descSet.GetAPITypeRef<VlkDescriptorSet>().GetVkDescriptorSet();
	vkCmdBindDescriptorSets(m_vkCommandBuffer,static_cast<VkPipelineBindPoint>(bindPoint),static_cast<const VlkShaderPipelineLayout&>(pipelineLayout).GetVkPipelineLayout(),firstSet,1u,&vkDescSet,optDynamicOffset ? 1 : 0,optDynamicOffset);
	return true;
}

bool prosper::VlkCommandBuffer::RecordPushConstants(const IShaderPipelineLayout &pipelineLayout,ShaderStageFlags stageFlags,uint32_t offset,uint32_t size,const void *data)
{
	vkCmdPushConstants(m_vkCommandBuffer,static_cast<const VlkShaderPipelineLayout&>(pipelineLayout).GetVkPipelineLayout(),static_cast<VkShaderStageFlags>(stageFlags),offset,size,data);
	return true;
}

bool prosper::VlkCommandBuffer::RecordPushConstants(prosper::Shader &shader,PipelineID pipelineIdx,ShaderStageFlags stageFlags,uint32_t offset,uint32_t size,const void *data)
{
	prosper::PipelineID pipelineId;
	return shader.GetPipelineId(pipelineId,pipelineIdx) && (*this)->record_push_constants(static_cast<VlkContext&>(GetContext()).GetPipelineLayout(shader.IsGraphicsShader(),pipelineId),static_cast<Anvil::ShaderStageFlagBits>(stageFlags),offset,size,data);
}
bool prosper::VlkCommandBuffer::DoRecordBindShaderPipeline(prosper::Shader &shader,PipelineID shaderPipelineId,PipelineID pipelineId)
{
	return (*this)->record_bind_pipeline(static_cast<Anvil::PipelineBindPoint>(shader.GetPipelineBindPoint()),static_cast<VlkContext&>(GetContext()).GetAnvilPipelineId(pipelineId));
}
bool prosper::VlkCommandBuffer::RecordSetLineWidth(float lineWidth)
{
	// return (*this)->record_set_line_width(lineWidth);
	vkCmdSetLineWidth(m_vkCommandBuffer,lineWidth);
	return true;
}
bool prosper::VlkCommandBuffer::RecordBindIndexBuffer(IBuffer &buf,IndexType indexType,DeviceSize offset)
{
	// return (*this)->record_bind_index_buffer(&buf.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),buf.GetStartOffset() +offset,static_cast<Anvil::IndexType>(indexType));
	vkCmdBindIndexBuffer(m_vkCommandBuffer,buf.GetAPITypeRef<VlkBuffer>().GetVkBuffer(),buf.GetStartOffset() +offset,static_cast<VkIndexType>(indexType));
	return true;
}
bool prosper::VlkCommandBuffer::RecordBindVertexBuffer(const prosper::ShaderGraphics &shader,const IBuffer &buf,uint32_t startBinding,DeviceSize offset)
{
	auto vkBuf = buf.GetAPITypeRef<VlkBuffer>().GetVkBuffer();
	vkCmdBindVertexBuffers(m_vkCommandBuffer,startBinding,1u /* bindingCount */,&vkBuf,&offset);
	return true;
}
bool prosper::VlkCommandBuffer::RecordBindVertexBuffers(
	const prosper::ShaderGraphics &shader,const std::vector<IBuffer*> &buffers,uint32_t startBinding,const std::vector<DeviceSize> &offsets
)
{
	return RecordBindVertexBuffers(buffers,startBinding,offsets);
}
bool prosper::VlkCommandBuffer::RecordBindVertexBuffers(
	const std::vector<IBuffer*> &buffers,uint32_t startBinding,const std::vector<DeviceSize> &offsets
)
{
	// Note: Same implementation as below
	std::vector<Anvil::Buffer*> anvBuffers {};
	anvBuffers.reserve(buffers.size());
	for(auto *buf : buffers)
		anvBuffers.push_back(&buf->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer());
	std::vector<DeviceSize> anvOffsets;
	if(offsets.empty())
		anvOffsets.resize(buffers.size(),0);
	else
		anvOffsets = offsets;
	for(auto i=decltype(buffers.size()){0u};i<buffers.size();++i)
		anvOffsets.at(i) += buffers.at(i)->GetStartOffset();
	return (*this)->record_bind_vertex_buffers(startBinding,anvBuffers.size(),anvBuffers.data(),anvOffsets.data());
}
bool prosper::VlkCommandBuffer::RecordBindVertexBuffers(const std::vector<std::shared_ptr<IBuffer>> &buffers,uint32_t startBinding,const std::vector<DeviceSize> &offsets)
{
	// Note: Same implementation as above
	std::vector<Anvil::Buffer*> anvBuffers {};
	anvBuffers.reserve(buffers.size());
	for(auto &buf : buffers)
		anvBuffers.push_back(&buf->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer());
	std::vector<DeviceSize> anvOffsets;
	if(offsets.empty())
		anvOffsets.resize(buffers.size(),0);
	else
		anvOffsets = offsets;
	for(auto i=decltype(buffers.size()){0u};i<buffers.size();++i)
		anvOffsets.at(i) += buffers.at(i)->GetStartOffset();
	return (*this)->record_bind_vertex_buffers(startBinding,anvBuffers.size(),anvBuffers.data(),anvOffsets.data());
}
bool prosper::VlkCommandBuffer::RecordBindRenderBuffer(const IRenderBuffer &renderBuffer)
{
	auto &vkBuf = static_cast<const VlkRenderBuffer&>(renderBuffer);
	return vkBuf.Record(m_vkCommandBuffer);
}
bool prosper::VlkCommandBuffer::RecordDispatchIndirect(prosper::IBuffer &buffer,DeviceSize size)
{
	// return (*this)->record_dispatch_indirect(&buffer.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),size);
	vkCmdDispatchIndirect(m_vkCommandBuffer,buffer.GetAPITypeRef<VlkBuffer>().GetVkBuffer(),size);
	return true;
}
bool prosper::VlkCommandBuffer::RecordDispatch(uint32_t x,uint32_t y,uint32_t z)
{
	// return (*this)->record_dispatch(x,y,z);
	vkCmdDispatch(m_vkCommandBuffer,x,y,z);
	return true;
}
bool prosper::VlkCommandBuffer::RecordDraw(uint32_t vertCount,uint32_t instanceCount,uint32_t firstVertex,uint32_t firstInstance)
{
	// return (*this)->record_draw(vertCount,instanceCount,firstVertex,firstInstance);
	vkCmdDraw(m_vkCommandBuffer,vertCount,instanceCount,firstVertex,firstInstance);
	return true;
}
bool prosper::VlkCommandBuffer::RecordDrawIndexed(uint32_t indexCount,uint32_t instanceCount,uint32_t firstIndex,uint32_t firstInstance)
{
	// return (*this)->record_draw_indexed(indexCount,instanceCount,firstIndex,0 /* vertexOffset */,firstInstance);
	vkCmdDrawIndexed(m_vkCommandBuffer,indexCount,instanceCount,firstIndex,0u /* vertexOffset */,firstInstance);
	return true;
}
bool prosper::VlkCommandBuffer::RecordDrawIndexedIndirect(IBuffer &buf,DeviceSize offset,uint32_t drawCount,uint32_t stride)
{
	// return (*this)->record_draw_indexed_indirect(&buf.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),offset,drawCount,stride);
	vkCmdDrawIndexedIndirect(m_vkCommandBuffer,buf.GetAPITypeRef<VlkBuffer>().GetVkBuffer(),offset,drawCount,stride);
	return true;
}
bool prosper::VlkCommandBuffer::RecordDrawIndirect(IBuffer &buf,DeviceSize offset,uint32_t count,uint32_t stride)
{
	// return (*this)->record_draw_indirect(&buf.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),offset,count,stride);
	vkCmdDrawIndirect(m_vkCommandBuffer,buf.GetAPITypeRef<VlkBuffer>().GetVkBuffer(),offset,count,stride);
	return true;
}
bool prosper::VlkCommandBuffer::RecordFillBuffer(IBuffer &buf,DeviceSize offset,DeviceSize size,uint32_t data)
{
	// return (*this)->record_fill_buffer(&buf.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),offset,size,data);
	vkCmdFillBuffer(m_vkCommandBuffer,buf.GetAPITypeRef<VlkBuffer>().GetVkBuffer(),offset,size,data);
	return true;
}
bool prosper::VlkCommandBuffer::RecordSetBlendConstants(const std::array<float,4> &blendConstants)
{
	// return (*this)->record_set_blend_constants(blendConstants.data());
	vkCmdSetBlendConstants(m_vkCommandBuffer,blendConstants.data());
	return true;
}
bool prosper::VlkCommandBuffer::RecordSetDepthBounds(float minDepthBounds,float maxDepthBounds)
{
	// return (*this)->record_set_depth_bounds(minDepthBounds,maxDepthBounds);
	vkCmdSetDepthBounds(m_vkCommandBuffer,minDepthBounds,maxDepthBounds);
	return true;
}
bool prosper::VlkCommandBuffer::RecordSetStencilCompareMask(StencilFaceFlags faceMask,uint32_t stencilCompareMask)
{
	// return (*this)->record_set_stencil_compare_mask(static_cast<Anvil::StencilFaceFlagBits>(faceMask),stencilCompareMask);
	vkCmdSetStencilCompareMask(m_vkCommandBuffer,static_cast<VkStencilFaceFlags>(faceMask),stencilCompareMask);
	return true;
}
bool prosper::VlkCommandBuffer::RecordSetStencilReference(StencilFaceFlags faceMask,uint32_t stencilReference)
{
	// return (*this)->record_set_stencil_reference(static_cast<Anvil::StencilFaceFlagBits>(faceMask),stencilReference);
	vkCmdSetStencilReference(m_vkCommandBuffer,static_cast<VkStencilFaceFlags>(faceMask),stencilReference);
	return true;
}
bool prosper::VlkCommandBuffer::RecordSetStencilWriteMask(StencilFaceFlags faceMask,uint32_t stencilWriteMask)
{
	// return (*this)->record_set_stencil_write_mask(static_cast<Anvil::StencilFaceFlagBits>(faceMask),stencilWriteMask);
	vkCmdSetStencilWriteMask(m_vkCommandBuffer,static_cast<VkStencilFaceFlags>(faceMask),stencilWriteMask);
	return true;
}
bool prosper::VlkCommandBuffer::RecordBeginOcclusionQuery(const prosper::OcclusionQuery &query) const
{
	auto *pQueryPool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pQueryPool == nullptr)
		return false;
	auto *anvPool = &pQueryPool->GetAnvilQueryPool();
	// return const_cast<VlkCommandBuffer&>(*this)->record_reset_query_pool(anvPool,query.GetQueryId(),1u /* queryCount */) &&
	// 	const_cast<VlkCommandBuffer&>(*this)->record_begin_query(anvPool,query.GetQueryId(),{});
	vkCmdResetQueryPool(m_vkCommandBuffer,anvPool->get_query_pool(),query.GetQueryId(),1u /* queryCount */);
	vkCmdBeginQuery(m_vkCommandBuffer,anvPool->get_query_pool(),query.GetQueryId(),1u /* queryCount */);
	return true;
}
bool prosper::VlkCommandBuffer::RecordEndOcclusionQuery(const prosper::OcclusionQuery &query) const
{
	auto *pQueryPool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pQueryPool == nullptr)
		return false;
	auto *anvPool = &pQueryPool->GetAnvilQueryPool();
	// return const_cast<VlkCommandBuffer&>(*this)->record_end_query(anvPool,query.GetQueryId());
	vkCmdEndQuery(m_vkCommandBuffer,anvPool->get_query_pool(),query.GetQueryId());
	return true;
}
bool prosper::VlkCommandBuffer::WriteTimestampQuery(const prosper::TimestampQuery &query) const
{
	auto *pQueryPool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pQueryPool == nullptr)
		return false;
	auto *anvPool = &pQueryPool->GetAnvilQueryPool();
	if(query.IsReset() == false && query.Reset(const_cast<VlkCommandBuffer&>(*this)) == false)
		return false;
	// return const_cast<prosper::VlkCommandBuffer&>(*this)->record_write_timestamp(static_cast<Anvil::PipelineStageFlagBits>(query.GetPipelineStage()),anvPool,query.GetQueryId());
	vkCmdWriteTimestamp(m_vkCommandBuffer,static_cast<VkPipelineStageFlagBits>(query.GetPipelineStage()),anvPool->get_query_pool(),query.GetQueryId());
	return true;
}

bool prosper::VlkCommandBuffer::ResetQuery(const Query &query) const
{
	auto *pQueryPool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pQueryPool == nullptr)
		return false;
	auto *anvPool = &pQueryPool->GetAnvilQueryPool();
	// auto success = const_cast<VlkCommandBuffer&>(*this)->record_reset_query_pool(anvPool,query.GetQueryId(),1u /* queryCount */);
	vkCmdResetQueryPool(m_vkCommandBuffer,anvPool->get_query_pool(),query.GetQueryId(),1u /* queryCount */);
	auto success = true;
	if(success)
		const_cast<Query&>(query).OnReset(const_cast<VlkCommandBuffer&>(*this));
	return success;
}

bool prosper::VlkCommandBuffer::RecordPresentImage(IImage &img,uint32_t swapchainImgIndex)
{
	auto &context = GetContext();
	// Change swapchain image layout to TransferDst
	prosper::util::ImageSubresourceRange subresourceRange {0,1,0,1};
	auto queueFamilyIndex = context.GetUniversalQueueFamilyIndex();

	{

		prosper::util::ImageBarrierInfo imgBarrierInfo {};
		imgBarrierInfo.srcAccessMask = prosper::AccessFlags{};
		imgBarrierInfo.dstAccessMask = prosper::AccessFlags::TransferWriteBit;
		imgBarrierInfo.oldLayout = prosper::ImageLayout::Undefined;
		imgBarrierInfo.newLayout = prosper::ImageLayout::TransferDstOptimal;
		imgBarrierInfo.subresourceRange = subresourceRange;
		imgBarrierInfo.srcQueueFamilyIndex = imgBarrierInfo.dstQueueFamilyIndex = queueFamilyIndex;

		prosper::util::PipelineBarrierInfo barrierInfo {};
		barrierInfo.srcStageMask = prosper::PipelineStageFlags::TopOfPipeBit;
		barrierInfo.dstStageMask = prosper::PipelineStageFlags::TransferBit;
		barrierInfo.imageBarriers.push_back(prosper::util::create_image_barrier(*context.GetSwapchainImage(swapchainImgIndex),imgBarrierInfo));
		RecordPipelineBarrier(barrierInfo);
	}

	RecordBlitImage({},img,*context.GetSwapchainImage(swapchainImgIndex));

	/* Change the swap-chain image's layout to presentable */
	{
		prosper::util::ImageBarrierInfo imgBarrierInfo {};
		imgBarrierInfo.srcAccessMask = prosper::AccessFlags::TransferWriteBit;
		imgBarrierInfo.dstAccessMask = prosper::AccessFlags::MemoryReadBit;
		imgBarrierInfo.oldLayout = prosper::ImageLayout::TransferDstOptimal;
		imgBarrierInfo.newLayout = prosper::ImageLayout::PresentSrcKHR;
		imgBarrierInfo.subresourceRange = subresourceRange;
		imgBarrierInfo.srcQueueFamilyIndex = imgBarrierInfo.dstQueueFamilyIndex = queueFamilyIndex;

		prosper::util::PipelineBarrierInfo barrierInfo {};
		barrierInfo.srcStageMask = prosper::PipelineStageFlags::TransferBit;
		barrierInfo.dstStageMask = prosper::PipelineStageFlags::AllCommands;
		barrierInfo.imageBarriers.push_back(prosper::util::create_image_barrier(*context.GetSwapchainImage(swapchainImgIndex),imgBarrierInfo));
		RecordPipelineBarrier(barrierInfo);
	}
	///
	return true;
}

bool prosper::VlkCommandBuffer::RecordBeginPipelineStatisticsQuery(const PipelineStatisticsQuery &query) const
{
	auto *pQueryPool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pQueryPool == nullptr)
		return false;
	auto *anvPool = &pQueryPool->GetAnvilQueryPool();
	if(query.IsReset() == false && query.Reset(const_cast<VlkCommandBuffer&>(*this)) == false)
		return false;
	return const_cast<prosper::VlkCommandBuffer&>(*this)->record_begin_query(anvPool,query.GetQueryId(),{});
}
bool prosper::VlkCommandBuffer::RecordEndPipelineStatisticsQuery(const PipelineStatisticsQuery &query) const
{
	auto *pQueryPool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pQueryPool == nullptr)
		return false;
	auto *anvPool = &pQueryPool->GetAnvilQueryPool();
	return const_cast<VlkCommandBuffer&>(*this)->record_end_query(anvPool,query.GetQueryId());
}
static Anvil::ImageSubresourceRange to_anvil_subresource_range(const prosper::util::ImageSubresourceRange &range,prosper::IImage &img)
{
	Anvil::ImageSubresourceRange anvRange {};
	anvRange.base_array_layer = range.baseArrayLayer;
	anvRange.base_mip_level = range.baseMipLevel;
	anvRange.layer_count = range.layerCount;
	anvRange.level_count = range.levelCount;
	anvRange.aspect_mask = static_cast<Anvil::ImageAspectFlagBits>(prosper::util::get_aspect_mask(img));
	return anvRange;
}
bool prosper::VlkCommandBuffer::RecordClearImage(IImage &img,ImageLayout layout,const std::array<float,4> &clearColor,const util::ClearImageInfo &clearImageInfo)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) != nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");
	// if(cmdBuffer.GetContext().IsValidationEnabled() && is_compressed_format(img.get_create_info_ptr()->get_format()))
	// 	throw std::logic_error("Attempted to clear image with compressed image format! This is not allowed!");

	prosper::util::ImageSubresourceRange resourceRange {};
	apply_image_subresource_range(clearImageInfo.subresourceRange,resourceRange,img);

	auto anvResourceRange = to_anvil_subresource_range(resourceRange,img);
	prosper::ClearColorValue clearColorVal {clearColor};
	return m_cmdBuffer->record_clear_color_image(
		&*static_cast<VlkImage&>(img),static_cast<Anvil::ImageLayout>(layout),reinterpret_cast<VkClearColorValue*>(&clearColorVal),
		1u,&anvResourceRange
	);
}
bool prosper::VlkCommandBuffer::RecordClearImage(IImage &img,ImageLayout layout,float clearDepth,const util::ClearImageInfo &clearImageInfo)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) != nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");
	// if(cmdBuffer.GetContext().IsValidationEnabled() && is_compressed_format(img.get_create_info_ptr()->get_format()))
	// 	throw std::logic_error("Attempted to clear image with compressed image format! This is not allowed!");

	prosper::util::ImageSubresourceRange resourceRange {};
	apply_image_subresource_range(clearImageInfo.subresourceRange,resourceRange,img);

	auto anvResourceRange = to_anvil_subresource_range(resourceRange,img);
	prosper::ClearDepthStencilValue clearDepthValue {clearDepth};
	return m_cmdBuffer->record_clear_depth_stencil_image(
		&*static_cast<VlkImage&>(img),static_cast<Anvil::ImageLayout>(layout),reinterpret_cast<VkClearDepthStencilValue*>(&clearDepthValue),
		1u,&anvResourceRange
	);
}
bool prosper::VlkCommandBuffer::RecordPipelineBarrier(const util::PipelineBarrierInfo &barrierInfo)
{
	if(static_cast<VlkContext&>(GetContext()).IsCustomValidationEnabled())
	{
		for(auto &imgBarrier : barrierInfo.imageBarriers)
		{
			auto &range = imgBarrier.subresourceRange;
			for(auto i=range.baseArrayLayer;i<(range.baseArrayLayer +range.layerCount);++i)
			{
				for(auto j=range.baseMipLevel;j<(range.baseMipLevel +range.levelCount);++j)
				{
					ImageLayout layout;
					if(debug::get_last_recorded_image_layout(*this,*imgBarrier.image,layout,i,j) == false)
						continue;
					if(layout != imgBarrier.oldLayout && imgBarrier.oldLayout != ImageLayout::Undefined)
					{
						debug::exec_debug_validation_callback(
							GetContext(),prosper::DebugReportObjectTypeEXT::Image,"Record pipeline barrier: Image 0x" +::util::to_hex_string(reinterpret_cast<uint64_t>(imgBarrier.image)) +
							" at array layer " +std::to_string(i) +", mipmap " +std::to_string(j) +" has current layout " +vk::to_string(static_cast<vk::ImageLayout>(layout)) +
							", but should have " +vk::to_string(static_cast<vk::ImageLayout>(imgBarrier.oldLayout)) +" according to barrier info!"
						);
					}
				}
			}
		}
		for(auto &imgBarrier : barrierInfo.imageBarriers)
		{
			debug::set_last_recorded_image_layout(
				*this,*imgBarrier.image,imgBarrier.newLayout,
				imgBarrier.subresourceRange.baseArrayLayer,imgBarrier.subresourceRange.layerCount,
				imgBarrier.subresourceRange.baseMipLevel,imgBarrier.subresourceRange.levelCount
			);
		}
	}
	std::vector<Anvil::BufferBarrier> anvBufBarriers {};
	anvBufBarriers.reserve(barrierInfo.bufferBarriers.size());
	for(auto &barrier : barrierInfo.bufferBarriers)
	{
		if(barrier.size == 0)
			continue;
		anvBufBarriers.push_back(Anvil::BufferBarrier{
			static_cast<Anvil::AccessFlagBits>(barrier.srcAccessMask),static_cast<Anvil::AccessFlagBits>(barrier.dstAccessMask),
			barrier.srcQueueFamilyIndex,barrier.dstQueueFamilyIndex,
			&barrier.buffer->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),barrier.offset,barrier.size
			});
	}
	std::vector<Anvil::ImageBarrier> anvImgBarriers {};
	anvImgBarriers.reserve(barrierInfo.imageBarriers.size());
	for(auto &barrier : barrierInfo.imageBarriers)
	{
		anvImgBarriers.push_back(Anvil::ImageBarrier{
			static_cast<Anvil::AccessFlagBits>(barrier.srcAccessMask),static_cast<Anvil::AccessFlagBits>(barrier.dstAccessMask),
			static_cast<Anvil::ImageLayout>(barrier.oldLayout),static_cast<Anvil::ImageLayout>(barrier.newLayout),
			barrier.srcQueueFamilyIndex,barrier.dstQueueFamilyIndex,
			&static_cast<VlkImage*>(barrier.image)->GetAnvilImage(),to_anvil_subresource_range(barrier.subresourceRange,*barrier.image)
			});
	}
	if(anvBufBarriers.empty() && anvImgBarriers.empty())
		return true;
	return (*this)->record_pipeline_barrier(
		static_cast<Anvil::PipelineStageFlagBits>(barrierInfo.srcStageMask),static_cast<Anvil::PipelineStageFlagBits>(barrierInfo.dstStageMask),
		Anvil::DependencyFlagBits::NONE,0u,nullptr,
		anvBufBarriers.size(),anvBufBarriers.data(),
		anvImgBarriers.size(),anvImgBarriers.data()
	);
}

///////////////////

std::shared_ptr<prosper::VlkCommandPool> prosper::VlkCommandPool::Create(prosper::IPrContext &context,prosper::QueueFamilyType queueFamilyType,Anvil::CommandPoolUniquePtr cmdPool)
{
	return std::shared_ptr<VlkCommandPool>{new VlkCommandPool{context,queueFamilyType,std::move(cmdPool)}};
}

std::shared_ptr<prosper::IPrimaryCommandBuffer> prosper::VlkCommandPool::AllocatePrimaryCommandBuffer() const
{
	return prosper::VlkPrimaryCommandBuffer::Create(GetContext(),m_commandPool->alloc_primary_level_command_buffer(),GetQueueFamilyType());
}
std::shared_ptr<prosper::ISecondaryCommandBuffer> prosper::VlkCommandPool::AllocateSecondaryCommandBuffer() const
{
	return prosper::VlkSecondaryCommandBuffer::Create(GetContext(),m_commandPool->alloc_secondary_level_command_buffer(),GetQueueFamilyType());
}
prosper::VlkCommandPool::VlkCommandPool(prosper::IPrContext &context,prosper::QueueFamilyType queueFamilyType,Anvil::CommandPoolUniquePtr cmdPool)
	: prosper::ICommandBufferPool{context,queueFamilyType},m_commandPool{std::move(cmdPool)}
{}

///////////////////

std::shared_ptr<prosper::VlkPrimaryCommandBuffer> prosper::VlkPrimaryCommandBuffer::Create(IPrContext &context,Anvil::PrimaryCommandBufferUniquePtr cmdBuffer,prosper::QueueFamilyType queueFamilyType,const std::function<void(VlkCommandBuffer&)> &onDestroyedCallback)
{
	std::shared_ptr<prosper::VlkPrimaryCommandBuffer> cmdBuf;
	if(onDestroyedCallback == nullptr)
		cmdBuf = std::shared_ptr<VlkPrimaryCommandBuffer>(new VlkPrimaryCommandBuffer(context,std::move(cmdBuffer),queueFamilyType));
	else
	{
		cmdBuf = std::shared_ptr<VlkPrimaryCommandBuffer>(new VlkPrimaryCommandBuffer(context,std::move(cmdBuffer),queueFamilyType),[onDestroyedCallback](VlkCommandBuffer *buf) {
			buf->OnRelease();
			onDestroyedCallback(*buf);
			delete buf;
		});
	}
	cmdBuf->Initialize();
	return cmdBuf;
}
prosper::VlkPrimaryCommandBuffer::VlkPrimaryCommandBuffer(IPrContext &context,Anvil::PrimaryCommandBufferUniquePtr cmdBuffer,prosper::QueueFamilyType queueFamilyType)
	: VlkCommandBuffer(context,prosper::util::unique_ptr_to_shared_ptr(std::move(cmdBuffer)),queueFamilyType),
	ICommandBuffer{context,queueFamilyType}
{
	prosper::debug::register_debug_object(m_cmdBuffer->get_command_buffer(),*this,prosper::debug::ObjectType::CommandBuffer);
	m_apiTypePtr = static_cast<VlkCommandBuffer*>(this);
}
bool prosper::VlkPrimaryCommandBuffer::StartRecording(bool oneTimeSubmit,bool simultaneousUseAllowed) const
{
	return IPrimaryCommandBuffer::StartRecording(oneTimeSubmit,simultaneousUseAllowed) && static_cast<Anvil::PrimaryCommandBuffer&>(*m_cmdBuffer).start_recording(oneTimeSubmit,simultaneousUseAllowed);
}
bool prosper::VlkPrimaryCommandBuffer::IsPrimary() const {return true;}
Anvil::PrimaryCommandBuffer &prosper::VlkPrimaryCommandBuffer::GetAnvilCommandBuffer() const {return static_cast<Anvil::PrimaryCommandBuffer&>(VlkCommandBuffer::GetAnvilCommandBuffer());}
Anvil::PrimaryCommandBuffer &prosper::VlkPrimaryCommandBuffer::operator*() {return static_cast<Anvil::PrimaryCommandBuffer&>(VlkCommandBuffer::operator*());}
const Anvil::PrimaryCommandBuffer &prosper::VlkPrimaryCommandBuffer::operator*() const {return static_cast<const Anvil::PrimaryCommandBuffer&>(VlkCommandBuffer::operator*());}
Anvil::PrimaryCommandBuffer *prosper::VlkPrimaryCommandBuffer::operator->() {return static_cast<Anvil::PrimaryCommandBuffer*>(VlkCommandBuffer::operator->());}
const Anvil::PrimaryCommandBuffer *prosper::VlkPrimaryCommandBuffer::operator->() const {return static_cast<const Anvil::PrimaryCommandBuffer*>(VlkCommandBuffer::operator->());}
bool prosper::VlkPrimaryCommandBuffer::StopRecording() const
{
	return IPrimaryCommandBuffer::StopRecording() && m_cmdBuffer->stop_recording();
}
bool prosper::VlkPrimaryCommandBuffer::DoRecordEndRenderPass()
{
#ifdef DEBUG_VERBOSE
	std::cout<<"[PR] Ending render pass..."<<std::endl;
#endif
	if(static_cast<VlkContext&>(GetContext()).IsCustomValidationEnabled())
	{
		auto *rtInfo = GetActiveRenderPassTargetInfo();
		if(rtInfo)
		{
			auto rp = rtInfo->renderPass.lock();
			auto fb = rtInfo->framebuffer.lock();
			if(rp && fb)
			{
				auto &context = rp->GetContext();
				auto rt = rtInfo->renderTarget.lock();
				auto &rpCreateInfo = *static_cast<VlkRenderPass*>(rp.get())->GetAnvilRenderPass().get_render_pass_create_info();
				auto numAttachments = fb->GetAttachmentCount();
				for(auto i=decltype(numAttachments){0};i<numAttachments;++i)
				{
					auto *imgView = fb->GetAttachment(i);
					if(!imgView)
						continue;
					auto &img = imgView->GetImage();
					Anvil::AttachmentType attType;
					if(rpCreateInfo.get_attachment_type(i,&attType) == false)
						continue;
					Anvil::ImageLayout finalLayout;
					auto bValid = false;
					switch(attType)
					{
					case Anvil::AttachmentType::COLOR:
						bValid = rpCreateInfo.get_color_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,nullptr,&finalLayout);
						break;
					case Anvil::AttachmentType::DEPTH_STENCIL:
						bValid = rpCreateInfo.get_depth_stencil_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&finalLayout);
						break;
					}
					if(bValid == false)
						continue;
					auto layerCount = umath::min(imgView->GetLayerCount(),fb->GetLayerCount());
					auto baseLayer = imgView->GetBaseLayer();
					prosper::debug::set_last_recorded_image_layout(*this,img,static_cast<prosper::ImageLayout>(finalLayout),baseLayer,layerCount);
				}
			}
		}
	}
	return (*this)->record_end_render_pass();
}
bool prosper::VlkPrimaryCommandBuffer::DoRecordBeginRenderPass(
	prosper::IImage &img,prosper::IRenderPass &rp,prosper::IFramebuffer &fb,
	uint32_t *layerId,const std::vector<prosper::ClearValue> &clearValues,
	RenderPassFlags renderPassFlags
)
{
#ifdef DEBUG_VERBOSE
	auto numAttachments = rt.GetAttachmentCount();
	std::cout<<"[PR] Beginning render pass with "<<numAttachments<<" attachments:"<<std::endl;
	for(auto i=decltype(numAttachments){0u};i<numAttachments;++i)
	{
		auto &tex = rt.GetTexture(i);
		auto img = (tex != nullptr) ? tex->GetImage() : nullptr;
		std::cout<<"\t"<<((img != nullptr) ? img->GetAnvilImage().get_image() : nullptr)<<std::endl;
	}
#endif
	auto &context = GetContext();
	if(static_cast<VlkContext&>(context).IsCustomValidationEnabled())
	{
		auto &rpCreateInfo = *static_cast<prosper::VlkRenderPass&>(rp).GetAnvilRenderPass().get_render_pass_create_info();
		auto numAttachments = fb.GetAttachmentCount();
		for(auto i=decltype(numAttachments){0};i<numAttachments;++i)
		{
			auto *imgView = fb.GetAttachment(i);
			if(imgView == nullptr)
				continue;
			auto &img = imgView->GetImage();
			Anvil::AttachmentType attType;
			if(rpCreateInfo.get_attachment_type(i,&attType) == false)
				continue;
			Anvil::ImageLayout initialLayout;
			auto bValid = false;
			switch(attType)
			{
			case Anvil::AttachmentType::COLOR:
				bValid = rpCreateInfo.get_color_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,&initialLayout,nullptr);
				break;
			case Anvil::AttachmentType::DEPTH_STENCIL:
				bValid = rpCreateInfo.get_depth_stencil_attachment_properties(i,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,&initialLayout,nullptr);
				break;
			}
			if(bValid == false)
				continue;
			prosper::ImageLayout imgLayout;
			if(prosper::debug::get_last_recorded_image_layout(*this,img,imgLayout,(layerId != nullptr) ? *layerId : 0u) == false)
				continue;
			if(imgLayout != static_cast<prosper::ImageLayout>(initialLayout))
			{
				prosper::debug::exec_debug_validation_callback(
					context,prosper::DebugReportObjectTypeEXT::Image,"Begin render pass: Image 0x" +::util::to_hex_string(reinterpret_cast<uint64_t>(static_cast<prosper::VlkImage&>(img).GetAnvilImage().get_image())) +
					" has to be in layout " +vk::to_string(static_cast<vk::ImageLayout>(initialLayout)) +", but is in layout " +
					vk::to_string(static_cast<vk::ImageLayout>(imgLayout)) +"!"
				);
			}
		}
	}
	auto extents = img.GetExtents();
	auto renderArea = vk::Rect2D(vk::Offset2D(),reinterpret_cast<vk::Extent2D&>(extents));
	return static_cast<prosper::VlkPrimaryCommandBuffer&>(*this)->record_begin_render_pass(
		clearValues.size(),reinterpret_cast<const VkClearValue*>(clearValues.data()),
		&static_cast<prosper::VlkFramebuffer&>(fb).GetAnvilFramebuffer(),renderArea,&static_cast<prosper::VlkRenderPass&>(rp).GetAnvilRenderPass(),umath::is_flag_set(renderPassFlags,RenderPassFlags::SecondaryCommandBuffers) ? Anvil::SubpassContents::SECONDARY_COMMAND_BUFFERS : Anvil::SubpassContents::INLINE
	);// && RecordSetViewport(extents.width,extents.height) && RecordSetScissor(extents.width,extents.height);
}

///////////////////

std::shared_ptr<prosper::VlkSecondaryCommandBuffer> prosper::VlkSecondaryCommandBuffer::Create(IPrContext &context,std::unique_ptr<Anvil::SecondaryCommandBuffer,std::function<void(Anvil::SecondaryCommandBuffer*)>> cmdBuffer,prosper::QueueFamilyType queueFamilyType,const std::function<void(VlkCommandBuffer&)> &onDestroyedCallback)
{
	std::shared_ptr<prosper::VlkSecondaryCommandBuffer> cmdBuf;
	if(onDestroyedCallback == nullptr)
		cmdBuf = std::shared_ptr<VlkSecondaryCommandBuffer>(new VlkSecondaryCommandBuffer(context,std::move(cmdBuffer),queueFamilyType));
	else
	{
		cmdBuf = std::shared_ptr<VlkSecondaryCommandBuffer>(new VlkSecondaryCommandBuffer(context,std::move(cmdBuffer),queueFamilyType),[onDestroyedCallback](VlkCommandBuffer *buf) {
			buf->OnRelease();
			onDestroyedCallback(*buf);
			delete buf;
		});
	}
	cmdBuf->Initialize();
	return cmdBuf;
}
prosper::VlkSecondaryCommandBuffer::VlkSecondaryCommandBuffer(IPrContext &context,Anvil::SecondaryCommandBufferUniquePtr cmdBuffer,prosper::QueueFamilyType queueFamilyType)
	: VlkCommandBuffer(context,prosper::util::unique_ptr_to_shared_ptr(std::move(cmdBuffer)),queueFamilyType),
	ICommandBuffer{context,queueFamilyType},ISecondaryCommandBuffer{context,queueFamilyType}
{
	prosper::debug::register_debug_object(m_cmdBuffer->get_command_buffer(),*this,prosper::debug::ObjectType::CommandBuffer);
	m_apiTypePtr = static_cast<VlkCommandBuffer*>(this);
}
bool prosper::VlkSecondaryCommandBuffer::StartRecording(bool oneTimeSubmit,bool simultaneousUseAllowed) const
{
	return ISecondaryCommandBuffer::StartRecording(oneTimeSubmit,simultaneousUseAllowed) && static_cast<Anvil::SecondaryCommandBuffer&>(*m_cmdBuffer).start_recording(
		oneTimeSubmit,simultaneousUseAllowed,true /* renderPassUsageOnly */,
		nullptr,nullptr,0 /* subPass */,Anvil::OcclusionQuerySupportScope::NOT_REQUIRED,false,
		Anvil::QueryPipelineStatisticFlagBits::NONE
	);
}
bool prosper::VlkSecondaryCommandBuffer::StartRecording(prosper::IRenderPass &rp,prosper::IFramebuffer &fb,bool oneTimeSubmit,bool simultaneousUseAllowed) const
{
	return ISecondaryCommandBuffer::StartRecording(rp,fb,oneTimeSubmit,simultaneousUseAllowed) && static_cast<Anvil::SecondaryCommandBuffer&>(*m_cmdBuffer).start_recording(
		oneTimeSubmit,simultaneousUseAllowed,true /* renderPassUsageOnly */,
		&static_cast<const VlkFramebuffer&>(fb).GetAnvilFramebuffer(),&static_cast<const VlkRenderPass&>(rp).GetAnvilRenderPass(),0 /* subPass */,Anvil::OcclusionQuerySupportScope::NOT_REQUIRED,false,
		Anvil::QueryPipelineStatisticFlagBits::NONE
	);
}
bool prosper::VlkSecondaryCommandBuffer::StartRecording(
	bool oneTimeSubmit,bool simultaneousUseAllowed,bool renderPassUsageOnly,
	const IFramebuffer &framebuffer,const IRenderPass &rp,Anvil::SubPassID subPassId,
	Anvil::OcclusionQuerySupportScope occlusionQuerySupportScope,bool occlusionQueryUsedByPrimaryCommandBuffer,
	Anvil::QueryPipelineStatisticFlags statisticsFlags
) const
{
	return ISecondaryCommandBuffer::StartRecording(const_cast<IRenderPass&>(rp),const_cast<IFramebuffer&>(framebuffer),oneTimeSubmit,simultaneousUseAllowed) && static_cast<Anvil::SecondaryCommandBuffer&>(*m_cmdBuffer).start_recording(
		oneTimeSubmit,simultaneousUseAllowed,
		renderPassUsageOnly,&static_cast<const VlkFramebuffer&>(framebuffer).GetAnvilFramebuffer(),&static_cast<const VlkRenderPass&>(rp).GetAnvilRenderPass(),subPassId,occlusionQuerySupportScope,
		occlusionQueryUsedByPrimaryCommandBuffer,statisticsFlags
	);
}
bool prosper::VlkSecondaryCommandBuffer::StopRecording() const
{
	return ISecondaryCommandBuffer::StopRecording() && m_cmdBuffer->stop_recording();
}
bool prosper::VlkSecondaryCommandBuffer::IsSecondary() const {return true;}
Anvil::SecondaryCommandBuffer &prosper::VlkSecondaryCommandBuffer::GetAnvilCommandBuffer() const {return static_cast<Anvil::SecondaryCommandBuffer&>(VlkCommandBuffer::GetAnvilCommandBuffer());}
Anvil::SecondaryCommandBuffer &prosper::VlkSecondaryCommandBuffer::operator*() {return static_cast<Anvil::SecondaryCommandBuffer&>(VlkCommandBuffer::operator*());}
const Anvil::SecondaryCommandBuffer &prosper::VlkSecondaryCommandBuffer::operator*() const {return static_cast<const Anvil::SecondaryCommandBuffer&>(VlkCommandBuffer::operator*());}
Anvil::SecondaryCommandBuffer *prosper::VlkSecondaryCommandBuffer::operator->() {return static_cast<Anvil::SecondaryCommandBuffer*>(VlkCommandBuffer::operator->());}
const Anvil::SecondaryCommandBuffer *prosper::VlkSecondaryCommandBuffer::operator->() const {return static_cast<const Anvil::SecondaryCommandBuffer*>(VlkCommandBuffer::operator->());}

///////////////

prosper::VlkCommandBuffer::VlkCommandBuffer(IPrContext &context,const std::shared_ptr<Anvil::CommandBufferBase> &cmdBuffer,prosper::QueueFamilyType queueFamilyType)
	: ICommandBuffer{context,queueFamilyType},m_cmdBuffer{cmdBuffer},m_vkCommandBuffer{cmdBuffer->get_command_buffer()}
{
	prosper::debug::register_debug_object(m_cmdBuffer->get_command_buffer(),*this,prosper::debug::ObjectType::CommandBuffer);
}
prosper::VlkCommandBuffer::~VlkCommandBuffer()
{
	prosper::debug::deregister_debug_object(m_cmdBuffer->get_command_buffer());
}
bool prosper::VlkCommandBuffer::Reset(bool shouldReleaseResources) const
{
	return m_cmdBuffer->reset(shouldReleaseResources);
}
bool prosper::VlkCommandBuffer::RecordSetDepthBias(float depthBiasConstantFactor,float depthBiasClamp,float depthBiasSlopeFactor)
{
	return m_cmdBuffer->record_set_depth_bias(depthBiasConstantFactor,depthBiasClamp,depthBiasSlopeFactor);
}
bool prosper::VlkCommandBuffer::RecordClearAttachment(IImage &img,const std::array<float,4> &clearColor,uint32_t attId,uint32_t layerId,uint32_t layerCount)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) == nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	vk::ClearValue clearVal {vk::ClearColorValue{clearColor}};
	Anvil::ClearAttachment clearAtt {Anvil::ImageAspectFlagBits::COLOR_BIT,attId,clearVal};
	vk::ClearRect clearRect {
		vk::Rect2D{vk::Offset2D{0,0},static_cast<prosper::VlkImage&>(img)->get_image_extent_2D(0u)},
		layerId,layerCount
	};
	return m_cmdBuffer->record_clear_attachments(1u,&clearAtt,1u,reinterpret_cast<VkClearRect*>(&clearRect));
}
bool prosper::VlkCommandBuffer::RecordClearAttachment(IImage &img,float clearDepth,uint32_t layerId)
{
	// TODO
	//if(bufferDst.GetContext().IsValidationEnabled() && cmdBuffer.get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && get_current_render_pass_target(static_cast<Anvil::PrimaryCommandBuffer&>(cmdBuffer)) == nullptr)
	//	throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	vk::ClearValue clearVal {vk::ClearDepthStencilValue{clearDepth}};
	Anvil::ClearAttachment clearAtt {Anvil::ImageAspectFlagBits::DEPTH_BIT,0u /* color attachment */,clearVal};
	vk::ClearRect clearRect {
		vk::Rect2D{vk::Offset2D{0,0},static_cast<prosper::VlkImage&>(img)->get_image_extent_2D(0u)},
		layerId,1 /* layerCount */
	};
	return m_cmdBuffer->record_clear_attachments(1u,&clearAtt,1u,reinterpret_cast<VkClearRect*>(&clearRect));
}
bool prosper::VlkCommandBuffer::RecordSetViewport(uint32_t width,uint32_t height,uint32_t x,uint32_t y,float minDepth,float maxDepth)
{
	auto vp = vk::Viewport(x,y,width,height,minDepth,maxDepth);
	return m_cmdBuffer->record_set_viewport(0u,1u,reinterpret_cast<VkViewport*>(&vp));
}
bool prosper::VlkCommandBuffer::RecordSetScissor(uint32_t width,uint32_t height,uint32_t x,uint32_t y)
{
	auto scissor = vk::Rect2D(vk::Offset2D(x,y),vk::Extent2D(width,height));
	return m_cmdBuffer->record_set_scissor(0u,1u,reinterpret_cast<VkRect2D*>(&scissor));
}
bool prosper::VlkCommandBuffer::DoRecordCopyBuffer(const util::BufferCopy &copyInfo,IBuffer &bufferSrc,IBuffer &bufferDst)
{
	if(static_cast<VlkContext&>(bufferDst.GetContext()).IsCustomValidationEnabled() && m_cmdBuffer->get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && static_cast<VlkPrimaryCommandBuffer&>(*this).GetActiveRenderPassTargetInfo())
		throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	static_assert(sizeof(util::BufferCopy) == sizeof(Anvil::BufferCopy));
	return m_cmdBuffer->record_copy_buffer(&bufferSrc.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(),&bufferDst.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(),1u,reinterpret_cast<const Anvil::BufferCopy*>(&copyInfo));
}
bool prosper::VlkCommandBuffer::DoRecordCopyBufferToImage(const util::BufferImageCopyInfo &copyInfo,IBuffer &bufferSrc,IImage &imgDst,uint32_t w,uint32_t h)
{
	if(static_cast<VlkContext&>(bufferSrc.GetContext()).IsCustomValidationEnabled() && m_cmdBuffer->get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && static_cast<VlkPrimaryCommandBuffer&>(*this).GetActiveRenderPassTargetInfo())
		throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	Anvil::BufferImageCopy bufferImageCopy {};
	bufferImageCopy.buffer_offset = bufferSrc.GetStartOffset() +copyInfo.bufferOffset;
	bufferImageCopy.image_extent = vk::Extent3D(w,h,1);
	bufferImageCopy.image_subresource = Anvil::ImageSubresourceLayers{
		static_cast<Anvil::ImageAspectFlagBits>(copyInfo.aspectMask),copyInfo.mipLevel,copyInfo.baseArrayLayer,copyInfo.layerCount
	};
	return m_cmdBuffer->record_copy_buffer_to_image(
		&bufferSrc.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(),&*static_cast<VlkImage&>(imgDst),
		static_cast<Anvil::ImageLayout>(copyInfo.dstImageLayout),1u,&bufferImageCopy
	);
}
bool prosper::VlkCommandBuffer::DoRecordCopyImage(const util::CopyInfo &copyInfo,IImage &imgSrc,IImage &imgDst,uint32_t w,uint32_t h)
{
	vk::Extent3D extent{w,h,1};
	static_assert(sizeof(Anvil::ImageSubresourceLayers) == sizeof(util::ImageSubresourceLayers));
	static_assert(sizeof(vk::Offset3D) == sizeof(Offset3D));
	Anvil::ImageCopy copyRegion{
		reinterpret_cast<const Anvil::ImageSubresourceLayers&>(copyInfo.srcSubresource),reinterpret_cast<const vk::Offset3D&>(copyInfo.srcOffset),
		reinterpret_cast<const Anvil::ImageSubresourceLayers&>(copyInfo.dstSubresource),reinterpret_cast<const vk::Offset3D&>(copyInfo.dstOffset),extent
	};
	return m_cmdBuffer->record_copy_image(
		&*static_cast<VlkImage&>(imgSrc),static_cast<Anvil::ImageLayout>(copyInfo.srcImageLayout),
		&*static_cast<VlkImage&>(imgDst),static_cast<Anvil::ImageLayout>(copyInfo.dstImageLayout),
		1,&copyRegion
	);
}
bool prosper::VlkCommandBuffer::DoRecordCopyImageToBuffer(const util::BufferImageCopyInfo &copyInfo,IImage &imgSrc,ImageLayout srcImageLayout,IBuffer &bufferDst,uint32_t w,uint32_t h)
{
	if(static_cast<VlkContext&>(bufferDst.GetContext()).IsCustomValidationEnabled() && m_cmdBuffer->get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && static_cast<VlkPrimaryCommandBuffer&>(*this).GetActiveRenderPassTargetInfo())
		throw std::logic_error("Attempted to copy image to buffer while render pass is active!");

	Anvil::BufferImageCopy bufferImageCopy {};
	bufferImageCopy.buffer_offset = bufferDst.GetStartOffset() +copyInfo.bufferOffset;
	bufferImageCopy.image_extent = vk::Extent3D(w,h,1);
	bufferImageCopy.image_subresource = Anvil::ImageSubresourceLayers{
		static_cast<Anvil::ImageAspectFlagBits>(copyInfo.aspectMask),copyInfo.mipLevel,copyInfo.baseArrayLayer,copyInfo.layerCount
	};
	return m_cmdBuffer->record_copy_image_to_buffer(
		&*static_cast<VlkImage&>(imgSrc),static_cast<Anvil::ImageLayout>(srcImageLayout),&bufferDst.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),1u,&bufferImageCopy
	);
}
bool prosper::VlkCommandBuffer::DoRecordBlitImage(const util::BlitInfo &blitInfo,IImage &imgSrc,IImage &imgDst,const std::array<Offset3D,2> &srcOffsets,const std::array<Offset3D,2> &dstOffsets)
{
	static_assert(sizeof(util::ImageSubresourceLayers) == sizeof(Anvil::ImageSubresourceLayers));
	static_assert(sizeof(Offset3D) == sizeof(vk::Offset3D));
	Anvil::ImageBlit blit {};
	blit.src_subresource = reinterpret_cast<const Anvil::ImageSubresourceLayers&>(blitInfo.srcSubresourceLayer);
	blit.src_offsets[0] = reinterpret_cast<const vk::Offset3D&>(srcOffsets.at(0));
	blit.src_offsets[1] = reinterpret_cast<const vk::Offset3D&>(srcOffsets.at(1));
	blit.dst_subresource = reinterpret_cast<const Anvil::ImageSubresourceLayers&>(blitInfo.dstSubresourceLayer);
	blit.dst_offsets[0] = reinterpret_cast<const vk::Offset3D&>(dstOffsets.at(0));
	blit.dst_offsets[1] = reinterpret_cast<const vk::Offset3D&>(dstOffsets.at(1));
	auto bDepth = util::is_depth_format(imgSrc.GetFormat());
	blit.src_subresource.aspect_mask = blit.dst_subresource.aspect_mask = (bDepth == true) ? Anvil::ImageAspectFlagBits::DEPTH_BIT : Anvil::ImageAspectFlagBits::COLOR_BIT;
	return m_cmdBuffer->record_blit_image(
		&*static_cast<VlkImage&>(imgSrc),Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,
		&*static_cast<VlkImage&>(imgDst),Anvil::ImageLayout::TRANSFER_DST_OPTIMAL,
		1u,&blit,
		bDepth ? Anvil::Filter::NEAREST : Anvil::Filter::LINEAR
	);
}
bool prosper::VlkCommandBuffer::DoRecordResolveImage(IImage &imgSrc,IImage &imgDst,const util::ImageResolve &resolve)
{
	static_assert(sizeof(util::ImageResolve) == sizeof(Anvil::ImageResolve));
	return m_cmdBuffer->record_resolve_image(
		&*static_cast<VlkImage&>(imgSrc),Anvil::ImageLayout::TRANSFER_SRC_OPTIMAL,
		&*static_cast<VlkImage&>(imgDst),Anvil::ImageLayout::TRANSFER_DST_OPTIMAL,
		1u,&reinterpret_cast<const Anvil::ImageResolve&>(resolve)
	);
}
bool prosper::VlkCommandBuffer::RecordUpdateBuffer(IBuffer &buffer,uint64_t offset,uint64_t size,const void *data)
{
	if(static_cast<VlkContext&>(buffer.GetContext()).IsCustomValidationEnabled() && m_cmdBuffer->get_command_buffer_type() == Anvil::CommandBufferType::COMMAND_BUFFER_TYPE_PRIMARY && static_cast<VlkPrimaryCommandBuffer&>(*this).GetActiveRenderPassTargetInfo())
		throw std::logic_error("Attempted to update buffer while render pass is active!");
	return m_cmdBuffer->record_update_buffer(&buffer.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(),buffer.GetStartOffset() +offset,size,reinterpret_cast<const uint32_t*>(data));
}

///////////////

bool prosper::VlkPrimaryCommandBuffer::RecordNextSubPass()
{
	return (*this)->record_next_subpass(Anvil::SubpassContents::INLINE);
}
bool prosper::VlkPrimaryCommandBuffer::ExecuteCommands(prosper::ISecondaryCommandBuffer &cmdBuf)
{
	auto *anvCmdBuf = &static_cast<VlkSecondaryCommandBuffer&>(cmdBuf).GetAnvilCommandBuffer();
	return static_cast<Anvil::PrimaryCommandBuffer&>(**this).record_execute_commands(1,&anvCmdBuf);
}

// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_COMMAND_BUFFER_HPP__
#define __PR_PROSPER_VK_COMMAND_BUFFER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include "prosper_definitions.hpp"
#include "prosper_includes.hpp"
#include "prosper_context_object.hpp"
#include "prosper_command_buffer.hpp"
#include <wrappers/command_buffer.h>
#include <misc/types_enums.h>

namespace prosper {
	class DLLPROSPER_VK VlkCommandBuffer : virtual public ICommandBuffer {
	  public:
		virtual ~VlkCommandBuffer() override;
		Anvil::CommandBufferBase &GetAnvilCommandBuffer() const;
		Anvil::CommandBufferBase &operator*();
		const Anvil::CommandBufferBase &operator*() const;
		Anvil::CommandBufferBase *operator->();
		const Anvil::CommandBufferBase *operator->() const;

		virtual const void *GetInternalHandle() const override { return GetAnvilCommandBuffer().get_command_buffer(); }

		virtual bool Reset(bool shouldReleaseResources) const override;

		virtual bool RecordPipelineBarrier(const util::PipelineBarrierInfo &barrierInfo) override;
		virtual bool RecordSetDepthBias(float depthBiasConstantFactor = 0.f, float depthBiasClamp = 0.f, float depthBiasSlopeFactor = 0.f) override;
		virtual bool RecordClearImage(IImage &img, ImageLayout layout, const std::array<float, 4> &clearColor, const util::ClearImageInfo &clearImageInfo = {}) override;
		virtual bool RecordClearImage(IImage &img, ImageLayout layout, std::optional<float> clearDepth, std::optional<uint32_t> clearStencil, const util::ClearImageInfo &clearImageInfo = {}) override;
		using ICommandBuffer::RecordClearAttachment;
		virtual bool RecordClearAttachment(IImage &img, const std::array<float, 4> &clearColor, uint32_t attId, uint32_t layerId, uint32_t layerCount) override;
		virtual bool RecordClearAttachment(IImage &img, std::optional<float> clearDepth, std::optional<uint32_t> clearStencil, uint32_t layerId = 0u) override;
		virtual bool RecordSetViewport(uint32_t width, uint32_t height, uint32_t x = 0u, uint32_t y = 0u, float minDepth = 0.f, float maxDepth = 0.f) override;
		virtual bool RecordSetScissor(uint32_t width, uint32_t height, uint32_t x = 0u, uint32_t y = 0u) override;
		virtual bool RecordUpdateBuffer(IBuffer &buffer, uint64_t offset, uint64_t size, const void *data) override;
		virtual bool RecordBindDescriptorSets(PipelineBindPoint bindPoint, prosper::Shader &shader, PipelineID pipelineId, uint32_t firstSet, const std::vector<prosper::IDescriptorSet *> &descSets, const std::vector<uint32_t> dynamicOffsets = {}) override;
		virtual bool RecordBindDescriptorSets(PipelineBindPoint bindPoint, const IShaderPipelineLayout &pipelineLayout, uint32_t firstSet, const prosper::IDescriptorSet &descSet, uint32_t *optDynamicOffset = nullptr) override;
		virtual bool RecordBindDescriptorSets(PipelineBindPoint bindPoint, const IShaderPipelineLayout &pipelineLayout, uint32_t firstSet, uint32_t numDescSets, const prosper::IDescriptorSet *const *descSets, uint32_t numDynamicOffsets = 0,
		  const uint32_t *dynamicOffsets = nullptr) override;
		virtual bool RecordPushConstants(prosper::Shader &shader, PipelineID pipelineId, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data) override;
		virtual bool RecordPushConstants(const IShaderPipelineLayout &pipelineLayout, ShaderStageFlags stageFlags, uint32_t offset, uint32_t size, const void *data) override;

		virtual bool RecordSetLineWidth(float lineWidth) override;
		virtual bool RecordBindIndexBuffer(IBuffer &buf, IndexType indexType = IndexType::UInt16, DeviceSize offset = 0) override;
		virtual bool RecordBindVertexBuffers(const prosper::ShaderGraphics &shader, const std::vector<IBuffer *> &buffers, uint32_t startBinding = 0u, const std::vector<DeviceSize> &offsets = {}) override;
		virtual bool RecordBindVertexBuffer(const prosper::ShaderGraphics &shader, const IBuffer &buf, uint32_t startBinding = 0u, DeviceSize offset = 0u) override;
		virtual bool RecordBindRenderBuffer(const IRenderBuffer &renderBuffer) override;
		virtual bool RecordDispatchIndirect(prosper::IBuffer &buffer, DeviceSize size) override;
		virtual bool RecordDispatch(uint32_t x, uint32_t y, uint32_t z) override;
		virtual bool RecordDraw(uint32_t vertCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
		virtual bool RecordDrawIndexed(uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, uint32_t firstInstance = 0) override;
		virtual bool RecordDrawIndexedIndirect(IBuffer &buf, DeviceSize offset, uint32_t drawCount, uint32_t stride) override;
		virtual bool RecordDrawIndirect(IBuffer &buf, DeviceSize offset, uint32_t count, uint32_t stride) override;
		virtual bool RecordFillBuffer(IBuffer &buf, DeviceSize offset, DeviceSize size, uint32_t data) override;
		// bool RecordResetEvent(Event &ev,PipelineStateFlags stageMask);
		virtual bool RecordSetBlendConstants(const std::array<float, 4> &blendConstants) override;
		virtual bool RecordSetDepthBounds(float minDepthBounds, float maxDepthBounds) override;
		// bool RecordSetEvent(Event &ev,PipelineStageFlags stageMask);
		virtual bool RecordSetStencilCompareMask(StencilFaceFlags faceMask, uint32_t stencilCompareMask) override;
		virtual bool RecordSetStencilReference(StencilFaceFlags faceMask, uint32_t stencilReference) override;
		virtual bool RecordSetStencilWriteMask(StencilFaceFlags faceMask, uint32_t stencilWriteMask) override;

		virtual bool RecordBeginPipelineStatisticsQuery(const PipelineStatisticsQuery &query) const override;
		virtual bool RecordEndPipelineStatisticsQuery(const PipelineStatisticsQuery &query) const override;
		virtual bool RecordBeginOcclusionQuery(const OcclusionQuery &query) const override;
		virtual bool RecordEndOcclusionQuery(const OcclusionQuery &query) const override;
		virtual bool WriteTimestampQuery(const TimestampQuery &query) const override;
		virtual bool ResetQuery(const Query &query) const override;

		virtual bool RecordPresentImage(IImage &img, IImage &swapchainImg, IFramebuffer &swapchainFramebuffer) override;

		VkCommandBuffer GetVkCommandBuffer() const { return m_vkCommandBuffer; }
	  protected:
		VlkCommandBuffer(IPrContext &context, const std::shared_ptr<Anvil::CommandBufferBase> &cmdBuffer, prosper::QueueFamilyType queueFamilyType);
		virtual bool DoRecordBindShaderPipeline(prosper::Shader &shader, PipelineID shaderPipelineId, PipelineID pipelineId) override;
		virtual bool DoRecordCopyBuffer(const util::BufferCopy &copyInfo, IBuffer &bufferSrc, IBuffer &bufferDst) override;
		virtual bool DoRecordCopyImage(const util::CopyInfo &copyInfo, IImage &imgSrc, IImage &imgDst, uint32_t w, uint32_t h) override;
		virtual bool DoRecordCopyBufferToImage(const util::BufferImageCopyInfo &copyInfo, IBuffer &bufferSrc, IImage &imgDst) override;
		virtual bool DoRecordCopyImageToBuffer(const util::BufferImageCopyInfo &copyInfo, IImage &imgSrc, ImageLayout srcImageLayout, IBuffer &bufferDst) override;
		virtual bool DoRecordBlitImage(const util::BlitInfo &blitInfo, IImage &imgSrc, IImage &imgDst, const std::array<Offset3D, 2> &srcOffsets, const std::array<Offset3D, 2> &dstOffsets, std::optional<prosper::ImageAspectFlags> aspectFlags = {}) override;
		virtual bool DoRecordResolveImage(IImage &imgSrc, IImage &imgDst, const util::ImageResolve &resolve) override;
		bool RecordBindVertexBuffers(const std::vector<IBuffer *> &buffers, uint32_t startBinding = 0u, const std::vector<DeviceSize> &offsets = {});
		bool RecordBindVertexBuffers(const std::vector<std::shared_ptr<IBuffer>> &buffers, uint32_t startBinding = 0u, const std::vector<DeviceSize> &offsets = {});

		std::shared_ptr<Anvil::CommandBufferBase> m_cmdBuffer = nullptr;
		VkCommandBuffer m_vkCommandBuffer = nullptr;
	};

	class DLLPROSPER_VK VlkCommandPool : public prosper::ICommandBufferPool {
	  public:
		static std::shared_ptr<VlkCommandPool> Create(prosper::IPrContext &context, prosper::QueueFamilyType queueFamilyType, Anvil::CommandPoolUniquePtr cmdPool);
		std::shared_ptr<IPrimaryCommandBuffer> AllocatePrimaryCommandBuffer() const;
		std::shared_ptr<ISecondaryCommandBuffer> AllocateSecondaryCommandBuffer() const;
	  private:
		VlkCommandPool(prosper::IPrContext &context, prosper::QueueFamilyType queueFamilyType, Anvil::CommandPoolUniquePtr cmdPool);
		Anvil::CommandPoolUniquePtr m_commandPool = nullptr;
	};

	///////////////////

	class VlkContext;
	class DLLPROSPER_VK VlkPrimaryCommandBuffer : public VlkCommandBuffer, public IPrimaryCommandBuffer {
	  public:
		static std::shared_ptr<VlkPrimaryCommandBuffer> Create(IPrContext &context, std::unique_ptr<Anvil::PrimaryCommandBuffer, std::function<void(Anvil::PrimaryCommandBuffer *)>> cmdBuffer, prosper::QueueFamilyType queueFamilyType,
		  const std::function<void(VlkCommandBuffer &)> &onDestroyedCallback = nullptr);
		virtual bool IsPrimary() const override;
		virtual bool StopRecording() const override;

		Anvil::PrimaryCommandBuffer &GetAnvilCommandBuffer() const;
		Anvil::PrimaryCommandBuffer &operator*();
		const Anvil::PrimaryCommandBuffer &operator*() const;
		Anvil::PrimaryCommandBuffer *operator->();
		const Anvil::PrimaryCommandBuffer *operator->() const;

		virtual bool StartRecording(bool oneTimeSubmit = true, bool simultaneousUseAllowed = false) const override;
		virtual bool RecordNextSubPass() override;
		virtual bool ExecuteCommands(prosper::ISecondaryCommandBuffer &cmdBuf) override;
	  protected:
		void SetRecording(bool b) { IPrimaryCommandBuffer::m_recording = b; }
		friend VlkContext;
		VlkPrimaryCommandBuffer(IPrContext &context, std::unique_ptr<Anvil::PrimaryCommandBuffer, std::function<void(Anvil::PrimaryCommandBuffer *)>> cmdBuffer, prosper::QueueFamilyType queueFamilyType);
		virtual bool DoRecordEndRenderPass() override;
		virtual bool DoRecordBeginRenderPass(prosper::IImage &img, prosper::IRenderPass &rp, prosper::IFramebuffer &fb, uint32_t *layerId, const std::vector<prosper::ClearValue> &clearValues, RenderPassFlags renderPassFlags) override;
	};

	///////////////////

	class DLLPROSPER_VK VlkSecondaryCommandBuffer : public VlkCommandBuffer, public ISecondaryCommandBuffer {
	  public:
		static std::shared_ptr<VlkSecondaryCommandBuffer> Create(IPrContext &context, std::unique_ptr<Anvil::SecondaryCommandBuffer, std::function<void(Anvil::SecondaryCommandBuffer *)>> cmdBuffer, prosper::QueueFamilyType queueFamilyType,
		  const std::function<void(VlkCommandBuffer &)> &onDestroyedCallback = nullptr);
		virtual bool IsSecondary() const override;

		Anvil::SecondaryCommandBuffer &GetAnvilCommandBuffer() const;
		Anvil::SecondaryCommandBuffer &operator*();
		const Anvil::SecondaryCommandBuffer &operator*() const;
		Anvil::SecondaryCommandBuffer *operator->();
		const Anvil::SecondaryCommandBuffer *operator->() const;

		virtual bool StartRecording(bool oneTimeSubmit = true, bool simultaneousUseAllowed = false) const override;
		virtual bool StartRecording(prosper::IRenderPass &rp, prosper::IFramebuffer &fb, bool oneTimeSubmit = true, bool simultaneousUseAllowed = false) const override;
		virtual bool StopRecording() const override;
		bool StartRecording(bool oneTimeSubmit, bool simultaneousUseAllowed, bool renderPassUsageOnly, const IFramebuffer &framebuffer, const IRenderPass &rp, prosper::SubPassID subPassId, Anvil::OcclusionQuerySupportScope occlusionQuerySupportScope,
		  bool occlusionQueryUsedByPrimaryCommandBuffer, Anvil::QueryPipelineStatisticFlags statisticsFlags) const;
	  protected:
		VlkSecondaryCommandBuffer(IPrContext &context, std::unique_ptr<Anvil::SecondaryCommandBuffer, std::function<void(Anvil::SecondaryCommandBuffer *)>> cmdBuffer, prosper::QueueFamilyType queueFamilyType);
	};
};

#endif

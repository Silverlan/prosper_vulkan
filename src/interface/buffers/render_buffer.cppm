// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <prosper_vulkan_definitions.hpp>
#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

export module pragma.prosper.vulkan:buffer.render_buffer;

export import pragma.prosper;

export namespace prosper {
	class VlkContext;
	class DLLPROSPER_VK VlkRenderBuffer : public prosper::IRenderBuffer {
	  public:
		static std::shared_ptr<VlkRenderBuffer> Create(prosper::VlkContext &context, const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo, const std::vector<prosper::IBuffer *> &buffers, const std::vector<prosper::DeviceSize> &offsets = {},
		  const std::optional<IndexBufferInfo> &indexBufferInfo = {});

		bool Record(VkCommandBuffer cmdBuf) const;
	  private:
		VlkRenderBuffer(prosper::IPrContext &context, const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo, const std::vector<prosper::IBuffer *> &buffers, const std::vector<prosper::DeviceSize> &offsets, const std::optional<IndexBufferInfo> &indexBufferInfo = {});
		void Initialize();
		virtual void Reload() override;

		std::vector<DeviceSize> m_vkOffsets {};
		mutable std::vector<VkBuffer> m_vkBuffers {};

		VkBuffer m_vkIndexBuffer = nullptr;
		prosper::DeviceSize m_vkIndexBufferOffset = 0;
	};
};

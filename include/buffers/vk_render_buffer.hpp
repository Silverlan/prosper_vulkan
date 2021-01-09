/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_RENDER_BUFFER_HPP__
#define __PR_PROSPER_VK_RENDER_BUFFER_HPP__

#include <prosper_vulkan_definitions.hpp>
#include <prosper_includes.hpp>
#include <prosper_context_object.hpp>
#include <buffers/prosper_render_buffer.hpp>
#include <vulkan/vulkan.h>

namespace Anvil {class CommandBufferBase; class Buffer;};
namespace prosper
{
	class VlkContext;
	class GraphicsPipelineCreateInfo;
	class DLLPROSPER_VK VlkRenderBuffer
		: public prosper::IRenderBuffer
	{
	public:
		static std::shared_ptr<VlkRenderBuffer> Create(
			prosper::VlkContext &context,const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo,const std::vector<prosper::IBuffer*> &buffers,
			const std::vector<prosper::DeviceSize> &offsets={},const std::optional<IndexBufferInfo> &indexBufferInfo={}
		);
		const std::vector<prosper::DeviceSize> &GetOffsets() const;

		bool Record(VkCommandBuffer cmdBuf) const;
	private:
		VlkRenderBuffer(
			prosper::IPrContext &context,const std::vector<prosper::IBuffer*> &buffers,const std::vector<prosper::DeviceSize> &offsets,const std::optional<IndexBufferInfo> &indexBufferInfo={}
		);
		void Initialize();
		std::vector<prosper::DeviceSize> m_offsets {};

		std::vector<DeviceSize> m_vkOffsets {};
		mutable std::vector<VkBuffer> m_vkBuffers {};

		VkBuffer m_vkIndexBuffer = nullptr;
		prosper::DeviceSize m_vkIndexBufferOffset = 0;
	};
};

#endif

// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_DYNAMIC_RESIZABLE_BUFFER_HPP__
#define __PR_PROSPER_VK_DYNAMIC_RESIZABLE_BUFFER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_includes.hpp"
#include "vk_buffer.hpp"
#include "buffers/prosper_dynamic_resizable_buffer.hpp"

namespace prosper {
	namespace util {
		struct BufferCreateInfo;
		DLLPROSPER_VK std::shared_ptr<VkDynamicResizableBuffer> create_dynamic_resizable_buffer(IPrContext &context, BufferCreateInfo createInfo, uint64_t maxTotalSize, float clampSizeToAvailableGPUMemoryPercentage = 1.f, const void *data = nullptr);
	};
	class DLLPROSPER_VK VkDynamicResizableBuffer : public IDynamicResizableBuffer, virtual public VlkBuffer {
	  public:
		VkDynamicResizableBuffer(IPrContext &context, IBuffer &buffer, const util::BufferCreateInfo &createInfo, uint64_t maxTotalSize);
	  protected:
		virtual void MoveInternalBuffer(IBuffer &other) override;
	};
};

#endif

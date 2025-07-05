// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_UNIFORM_RESIZABLE_BUFFER_HPP__
#define __PR_PROSPER_VK_UNIFORM_RESIZABLE_BUFFER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_includes.hpp"
#include "vk_buffer.hpp"
#include "buffers/prosper_uniform_resizable_buffer.hpp"

namespace prosper {
	class DLLPROSPER_VK VkUniformResizableBuffer : public IUniformResizableBuffer, virtual public VlkBuffer {
	  public:
		VkUniformResizableBuffer(IPrContext &context, IBuffer &buffer, uint64_t bufferInstanceSize, uint64_t alignedBufferBaseSize, uint64_t maxTotalSize, uint32_t alignment);
	  protected:
		virtual void MoveInternalBuffer(IBuffer &other) override;
	};
};

#endif

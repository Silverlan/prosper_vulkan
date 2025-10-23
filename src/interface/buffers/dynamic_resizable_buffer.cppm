// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"

export module pragma.prosper.vulkan:buffer.dynamic_resizable_buffer;

export import :buffer.buffer;

export namespace prosper {
	class VkDynamicResizableBuffer;
	namespace util {
		DLLPROSPER_VK std::shared_ptr<VkDynamicResizableBuffer> create_dynamic_resizable_buffer(IPrContext &context, BufferCreateInfo createInfo, uint64_t maxTotalSize, float clampSizeToAvailableGPUMemoryPercentage = 1.f, const void *data = nullptr);
	};
	class DLLPROSPER_VK VkDynamicResizableBuffer : public IDynamicResizableBuffer, virtual public VlkBuffer {
	  public:
		VkDynamicResizableBuffer(IPrContext &context, IBuffer &buffer, const util::BufferCreateInfo &createInfo, uint64_t maxTotalSize);
	  protected:
		virtual void MoveInternalBuffer(IBuffer &other) override;
	};
};

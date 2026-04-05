// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

export module pragma.prosper.vulkan:buffer.dynamic_resizable_buffer;

export import :buffer.buffer;

export namespace prosper {
	class VkDynamicResizableBuffer;
	namespace util {
		PR_EXPORT std::shared_ptr<VkDynamicResizableBuffer> create_dynamic_resizable_buffer(IPrContext &context, BufferCreateInfo createInfo, const void *data = nullptr);
	};
	class PR_EXPORT VkDynamicResizableBuffer : public IDynamicResizableBuffer, virtual public VlkBuffer {
	  public:
		VkDynamicResizableBuffer(IPrContext &context, IBuffer &buffer, const util::BufferCreateInfo &createInfo);
	  protected:
		void MoveInternalBuffer(IBuffer &other) override;
		void ReleaseBufferSafely() override;
	};
};

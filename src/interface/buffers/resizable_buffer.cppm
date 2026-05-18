// SPDX-FileCopyrightText: (c) 2026 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

export module pragma.prosper.vulkan:buffer.resizable_buffer;

export import :buffer.buffer;

export namespace prosper {
	class VkResizableBuffer;
	namespace util {
		PR_EXPORT std::shared_ptr<VkResizableBuffer> create_resizable_buffer(IPrContext &context, BufferCreateInfo createInfo, const void *data = nullptr);
	};
	class PR_EXPORT VkResizableBuffer : public IResizableBuffer, virtual public VlkBuffer {
	public:
		VkResizableBuffer(IBuffer &buffer);
	protected:
		void MoveInternalBuffer(IBuffer &other) override;
		void ReleaseBufferSafely() override;
	};
};

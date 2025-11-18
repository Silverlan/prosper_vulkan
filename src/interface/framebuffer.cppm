// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/framebuffer.h>

export module pragma.prosper.vulkan:framebuffer;

export import :debug.object;

export namespace prosper {
	class PR_EXPORT VlkFramebuffer : public IFramebuffer, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkFramebuffer> Create(IPrContext &context, const std::vector<IImageView *> &attachments, uint32_t width, uint32_t height, uint32_t depth, uint32_t layers, std::unique_ptr<Anvil::Framebuffer, std::function<void(Anvil::Framebuffer *)>> fb,
		  const std::function<void(IFramebuffer &)> &onDestroyedCallback = nullptr);
		virtual ~VlkFramebuffer() override;
		virtual const void *GetInternalHandle() const override;
		virtual void Bake(IRenderPass &rp) override;
		Anvil::Framebuffer &GetAnvilFramebuffer() const;
		Anvil::Framebuffer &operator*();
		const Anvil::Framebuffer &operator*() const;
		Anvil::Framebuffer *operator->();
		const Anvil::Framebuffer *operator->() const;
	  protected:
		VlkFramebuffer(IPrContext &context, const std::vector<std::shared_ptr<IImageView>> &attachments, uint32_t width, uint32_t height, uint32_t depth, uint32_t layers, std::unique_ptr<Anvil::Framebuffer, std::function<void(Anvil::Framebuffer *)>> fb);
		std::unique_ptr<Anvil::Framebuffer, std::function<void(Anvil::Framebuffer *)>> m_framebuffer = nullptr;
	};
};

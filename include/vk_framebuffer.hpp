// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_FRAMEBUFFER_HPP__
#define __PR_PROSPER_VK_FRAMEBUFFER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_framebuffer.hpp"
#include "debug/vk_debug_object.hpp"
#include <wrappers/framebuffer.h>

namespace prosper {
	class DLLPROSPER_VK VlkFramebuffer : public IFramebuffer, public VlkDebugObject {
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

#endif

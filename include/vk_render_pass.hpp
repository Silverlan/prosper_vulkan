// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_RENDER_PASS_HPP__
#define __PR_PROSPER_VK_RENDER_PASS_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_render_pass.hpp"
#include "debug/vk_debug_object.hpp"
#include <wrappers/render_pass.h>

namespace prosper {
	class DLLPROSPER_VK VlkRenderPass : public IRenderPass, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkRenderPass> Create(IPrContext &context, const util::RenderPassCreateInfo &createInfo, std::unique_ptr<Anvil::RenderPass, std::function<void(Anvil::RenderPass *)>> rp, const std::function<void(IRenderPass &)> &onDestroyedCallback = nullptr);
		virtual ~VlkRenderPass() override;
		Anvil::RenderPass &GetAnvilRenderPass() const;
		Anvil::RenderPass &operator*();
		const Anvil::RenderPass &operator*() const;
		Anvil::RenderPass *operator->();
		const Anvil::RenderPass *operator->() const;
		virtual void Bake() override;

		virtual const void *GetInternalHandle() const override { return m_renderPass ? m_renderPass->get_render_pass() : nullptr; }
	  protected:
		VlkRenderPass(IPrContext &context, const util::RenderPassCreateInfo &createInfo, std::unique_ptr<Anvil::RenderPass, std::function<void(Anvil::RenderPass *)>> rp);
		std::unique_ptr<Anvil::RenderPass, std::function<void(Anvil::RenderPass *)>> m_renderPass = nullptr;
	};
};

#endif

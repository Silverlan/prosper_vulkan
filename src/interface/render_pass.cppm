// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/render_pass.h>

export module pragma.prosper.vulkan:render_pass;

export import :debug.object;

export namespace prosper {
	class PR_EXPORT VlkRenderPass : public IRenderPass, public VlkDebugObject {
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

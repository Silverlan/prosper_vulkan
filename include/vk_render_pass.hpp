/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_RENDER_PASS_HPP__
#define __PR_PROSPER_VK_RENDER_PASS_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_render_pass.hpp"
#include <wrappers/render_pass.h>

namespace prosper
{
	class DLLPROSPER_VK VlkRenderPass
		: public IRenderPass
	{
	public:
		static std::shared_ptr<VlkRenderPass> Create(IPrContext &context,const util::RenderPassCreateInfo &createInfo,std::unique_ptr<Anvil::RenderPass,std::function<void(Anvil::RenderPass*)>> rp,const std::function<void(IRenderPass&)> &onDestroyedCallback=nullptr);
		virtual ~VlkRenderPass() override;
		Anvil::RenderPass &GetAnvilRenderPass() const;
		Anvil::RenderPass &operator*();
		const Anvil::RenderPass &operator*() const;
		Anvil::RenderPass *operator->();
		const Anvil::RenderPass *operator->() const;

		virtual const void *GetInternalHandle() const override {return GetAnvilRenderPass().get_render_pass();}
	protected:
		VlkRenderPass(IPrContext &context,const util::RenderPassCreateInfo &createInfo,std::unique_ptr<Anvil::RenderPass,std::function<void(Anvil::RenderPass*)>> rp);
		std::unique_ptr<Anvil::RenderPass,std::function<void(Anvil::RenderPass*)>> m_renderPass = nullptr;
	};
};

#endif

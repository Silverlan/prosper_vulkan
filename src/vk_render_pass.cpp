/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_render_pass.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include <wrappers/render_pass.h>

using namespace prosper;

std::shared_ptr<VlkRenderPass> VlkRenderPass::Create(IPrContext &context, const prosper::util::RenderPassCreateInfo &createInfo, Anvil::RenderPassUniquePtr imgView, const std::function<void(IRenderPass &)> &onDestroyedCallback)
{
	if(imgView == nullptr)
		return nullptr;
	if(onDestroyedCallback == nullptr)
		return std::shared_ptr<VlkRenderPass>(new VlkRenderPass(context, createInfo, std::move(imgView)));
	return std::shared_ptr<VlkRenderPass>(new VlkRenderPass(context, createInfo, std::move(imgView)), [onDestroyedCallback](VlkRenderPass *buf) {
		buf->OnRelease();
		onDestroyedCallback(*buf);
		delete buf;
	});
}

VlkRenderPass::VlkRenderPass(IPrContext &context, const prosper::util::RenderPassCreateInfo &createInfo, Anvil::RenderPassUniquePtr imgView) : IRenderPass {context, createInfo}, m_renderPass {std::move(imgView)}
{
	prosper::debug::register_debug_object(m_renderPass->get_render_pass(), *this, prosper::debug::ObjectType::RenderPass);
}
VlkRenderPass::~VlkRenderPass()
{
	VlkDebugObject::Clear(GetContext(), debug::ObjectType::RenderPass, GetInternalHandle());
	prosper::debug::deregister_debug_object(m_renderPass->get_render_pass());
}
void VlkRenderPass::Bake()
{
	GetAnvilRenderPass().get_render_pass();
	VlkDebugObject::Init(GetContext(), debug::ObjectType::RenderPass, GetInternalHandle());
}

Anvil::RenderPass &VlkRenderPass::GetAnvilRenderPass() const { return *m_renderPass; }
Anvil::RenderPass &VlkRenderPass::operator*() { return *m_renderPass; }
const Anvil::RenderPass &VlkRenderPass::operator*() const { return const_cast<VlkRenderPass *>(this)->operator*(); }
Anvil::RenderPass *VlkRenderPass::operator->() { return m_renderPass.get(); }
const Anvil::RenderPass *VlkRenderPass::operator->() const { return const_cast<VlkRenderPass *>(this)->operator->(); }

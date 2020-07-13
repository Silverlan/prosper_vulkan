/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_framebuffer.hpp"
#include "image/prosper_image_view.hpp"

using namespace prosper;

std::shared_ptr<VlkFramebuffer> VlkFramebuffer::Create(
	IPrContext &context,const std::vector<IImageView*> &attachments,
	uint32_t width,uint32_t height,uint32_t depth,uint32_t layers,
	Anvil::FramebufferUniquePtr fb,const std::function<void(IFramebuffer&)> &onDestroyedCallback
)
{
	if(fb == nullptr)
		return nullptr;
	std::vector<std::shared_ptr<IImageView>> ptrAttachments {};
	ptrAttachments.reserve(attachments.size());
	for(auto *att : attachments)
		ptrAttachments.push_back(att->shared_from_this());
	if(onDestroyedCallback == nullptr)
		return std::shared_ptr<VlkFramebuffer>(new VlkFramebuffer{context,ptrAttachments,width,height,depth,layers,std::move(fb)});
	return std::shared_ptr<VlkFramebuffer>(new VlkFramebuffer{context,ptrAttachments,width,height,depth,layers,std::move(fb)},[onDestroyedCallback](VlkFramebuffer *fb) {
		fb->OnRelease();
		onDestroyedCallback(*fb);
		delete fb;
		});
}
VlkFramebuffer::VlkFramebuffer(
	IPrContext &context,const std::vector<std::shared_ptr<IImageView>> &attachments,
	uint32_t width,uint32_t height,uint32_t depth,uint32_t layers,
	std::unique_ptr<Anvil::Framebuffer,std::function<void(Anvil::Framebuffer*)>> fb
)
	: IFramebuffer{context,attachments,width,height,depth,layers},m_framebuffer{std::move(fb)}
{
	//prosper::debug::register_debug_object(m_framebuffer->get_framebuffer(),this,prosper::debug::ObjectType::Framebuffer);
}
VlkFramebuffer::~VlkFramebuffer() {}
Anvil::Framebuffer &VlkFramebuffer::GetAnvilFramebuffer() const {return *m_framebuffer;}
Anvil::Framebuffer &VlkFramebuffer::operator*() {return *m_framebuffer;}
const Anvil::Framebuffer &VlkFramebuffer::operator*() const {return const_cast<VlkFramebuffer*>(this)->operator*();}
Anvil::Framebuffer *VlkFramebuffer::operator->() {return m_framebuffer.get();}
const Anvil::Framebuffer *VlkFramebuffer::operator->() const {return const_cast<VlkFramebuffer*>(this)->operator->();}

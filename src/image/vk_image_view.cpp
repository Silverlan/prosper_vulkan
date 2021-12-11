/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "image/prosper_image_view.hpp"
#include "image/vk_image_view.hpp"
#include "prosper_util.hpp"
#include "prosper_context.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include "vk_memory_tracker.hpp"
#include <misc/image_view_create_info.h>
#include <wrappers/image_view.h>

using namespace prosper;

std::shared_ptr<VlkImageView> VlkImageView::Create(IPrContext &context,IImage &img,const prosper::util::ImageViewCreateInfo &createInfo,ImageViewType type,ImageAspectFlags aspectFlags,Anvil::ImageViewUniquePtr imgView,const std::function<void(IImageView&)> &onDestroyedCallback)
{
	if(imgView == nullptr)
		return nullptr;
	if(onDestroyedCallback == nullptr)
		return std::shared_ptr<VlkImageView>(new VlkImageView{context,img,createInfo,type,aspectFlags,std::move(imgView)});
	return std::shared_ptr<VlkImageView>(new VlkImageView{context,img,createInfo,type,aspectFlags,std::move(imgView)},[onDestroyedCallback](IImageView *buf) {
		buf->OnRelease();
		onDestroyedCallback(*buf);
		delete buf;
	});
}

VlkImageView::VlkImageView(IPrContext &context,IImage &img,const prosper::util::ImageViewCreateInfo &createInfo,ImageViewType type,ImageAspectFlags aspectFlags,Anvil::ImageViewUniquePtr imgView)
	: IImageView{context,img,createInfo,type,aspectFlags},m_imageView(std::move(imgView))
{
	prosper::debug::register_debug_object(m_imageView->get_image_view(),*this,prosper::debug::ObjectType::ImageView);
}
VlkImageView::~VlkImageView()
{
	prosper::debug::deregister_debug_object(m_imageView->get_image_view());
}
void VlkImageView::Bake()
{
	GetAnvilImageView().get_image_view();
}
Anvil::ImageView &VlkImageView::GetAnvilImageView() const {return *m_imageView;}
Anvil::ImageView &VlkImageView::operator*() {return *m_imageView;}
const Anvil::ImageView &VlkImageView::operator*() const {return const_cast<VlkImageView*>(this)->operator*();}
Anvil::ImageView *VlkImageView::operator->() {return m_imageView.get();}
const Anvil::ImageView *VlkImageView::operator->() const {return const_cast<VlkImageView*>(this)->operator->();}

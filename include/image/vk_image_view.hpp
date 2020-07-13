/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_IMAGE_VIEW_HPP__
#define __PR_PROSPER_VK_IMAGE_VIEW_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "image/prosper_image_view.hpp"
#include <wrappers/image_view.h>

namespace prosper
{
	class DLLPROSPER_VK VlkImageView
		: public IImageView
	{
	public:
		static std::shared_ptr<VlkImageView> Create(IPrContext &context,IImage &img,const util::ImageViewCreateInfo &createInfo,ImageViewType type,ImageAspectFlags aspectFlags,std::unique_ptr<Anvil::ImageView,std::function<void(Anvil::ImageView*)>> imgView,const std::function<void(IImageView&)> &onDestroyedCallback=nullptr);
		virtual ~VlkImageView() override;
		Anvil::ImageView &GetAnvilImageView() const;
		Anvil::ImageView &operator*();
		const Anvil::ImageView &operator*() const;
		Anvil::ImageView *operator->();
		const Anvil::ImageView *operator->() const;
	protected:
		VlkImageView(IPrContext &context,IImage &img,const util::ImageViewCreateInfo &createInfo,ImageViewType type,ImageAspectFlags aspectFlags,std::unique_ptr<Anvil::ImageView,std::function<void(Anvil::ImageView*)>> imgView);
		std::unique_ptr<Anvil::ImageView,std::function<void(Anvil::ImageView*)>> m_imageView = nullptr;
	};
};

#endif

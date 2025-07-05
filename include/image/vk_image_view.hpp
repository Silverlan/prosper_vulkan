// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_IMAGE_VIEW_HPP__
#define __PR_PROSPER_VK_IMAGE_VIEW_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "image/prosper_image_view.hpp"
#include "debug/vk_debug_object.hpp"
#include <wrappers/image_view.h>

namespace prosper {
	class DLLPROSPER_VK VlkImageView : public IImageView, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkImageView> Create(IPrContext &context, IImage &img, const util::ImageViewCreateInfo &createInfo, ImageViewType type, ImageAspectFlags aspectFlags, std::unique_ptr<Anvil::ImageView, std::function<void(Anvil::ImageView *)>> imgView,
		  const std::function<void(IImageView &)> &onDestroyedCallback = nullptr);
		virtual ~VlkImageView() override;
		Anvil::ImageView &GetAnvilImageView() const;
		Anvil::ImageView &operator*();
		const Anvil::ImageView &operator*() const;
		Anvil::ImageView *operator->();
		const Anvil::ImageView *operator->() const;

		virtual void Bake() override;

		virtual const void *GetInternalHandle() const override { return m_imageView ? m_imageView->get_image_view() : nullptr; }
	  protected:
		VlkImageView(IPrContext &context, IImage &img, const util::ImageViewCreateInfo &createInfo, ImageViewType type, ImageAspectFlags aspectFlags, std::unique_ptr<Anvil::ImageView, std::function<void(Anvil::ImageView *)>> imgView);
		std::unique_ptr<Anvil::ImageView, std::function<void(Anvil::ImageView *)>> m_imageView = nullptr;
	};
};

#endif

// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"
#include <wrappers/image_view.h>

export module pragma.prosper.vulkan:image.view;

export import :debug.object;

export namespace prosper {
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

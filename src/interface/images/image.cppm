// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/image.h>

export module pragma.prosper.vulkan:image.image;

export import :debug.object;

export namespace prosper {
	class PR_EXPORT VlkImage : public IImage, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkImage> Create(IPrContext &context, std::unique_ptr<Anvil::Image, std::function<void(Anvil::Image *)>> img, const util::ImageCreateInfo &createInfo, bool isSwapchainImage, const std::function<void(VlkImage &)> &onDestroyedCallback = nullptr);
		virtual ~VlkImage() override;
		Anvil::Image &GetAnvilImage() const;
		Anvil::Image &operator*();
		const Anvil::Image &operator*() const;
		Anvil::Image *operator->();
		const Anvil::Image *operator->() const;

		virtual void Bake() override;

		virtual bool WriteImageData(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t layerIndex, uint32_t mipLevel, uint64_t size, const uint8_t *data) override;
		virtual DeviceSize GetAlignment() const override;
		virtual bool Map(DeviceSize offset, DeviceSize size, void **outPtr = nullptr) override;
		virtual bool Unmap() override;
		virtual std::optional<size_t> GetStorageSize() const override;
		virtual const void *GetInternalHandle() const override;
		virtual std::optional<util::SubresourceLayout> GetSubresourceLayout(uint32_t layerId = 0, uint32_t mipMapIdx = 0) override;
	  protected:
		VlkImage(IPrContext &context, std::unique_ptr<Anvil::Image, std::function<void(Anvil::Image *)>> img, const util::ImageCreateInfo &createInfo, bool isSwapchainImage);
		virtual bool DoSetMemoryBuffer(IBuffer &buffer) override;
		std::unique_ptr<Anvil::Image, std::function<void(Anvil::Image *)>> m_image = nullptr;
		bool m_swapchainImage = false;
	};
};

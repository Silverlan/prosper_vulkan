// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/sampler.h>

export module pragma.prosper.vulkan:image.sampler;

export import :debug.object;

export namespace prosper {
	class PR_EXPORT VlkSampler : public ISampler, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkSampler> Create(IPrContext &context, const util::SamplerCreateInfo &createInfo);
		virtual ~VlkSampler() override;

		Anvil::Sampler &GetAnvilSampler() const;
		Anvil::Sampler &operator*();
		const Anvil::Sampler &operator*() const;
		Anvil::Sampler *operator->();
		const Anvil::Sampler *operator->() const;

		virtual void Bake() override;

		virtual const void *GetInternalHandle() const override { return GetAnvilSampler().get_sampler(); }
	  protected:
		VlkSampler(IPrContext &context, const util::SamplerCreateInfo &samplerCreateInfo);
		virtual bool DoUpdate() override;
		std::unique_ptr<Anvil::Sampler, std::function<void(Anvil::Sampler *)>> m_sampler = nullptr;
	};
};

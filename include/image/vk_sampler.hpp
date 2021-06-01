/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_SAMPLER_HPP__
#define __PR_PROSPER_VK_SAMPLER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "image/prosper_sampler.hpp"
#include <wrappers/sampler.h>

namespace prosper
{
	class DLLPROSPER_VK VlkSampler
		: public ISampler
	{
	public:
		static std::shared_ptr<VlkSampler> Create(IPrContext &context,const util::SamplerCreateInfo &createInfo);
		virtual ~VlkSampler() override;

		Anvil::Sampler &GetAnvilSampler() const;
		Anvil::Sampler &operator*();
		const Anvil::Sampler &operator*() const;
		Anvil::Sampler *operator->();
		const Anvil::Sampler *operator->() const;

		virtual const void *GetInternalHandle() const override {return GetAnvilSampler().get_sampler();}
	protected:
		VlkSampler(IPrContext &context,const util::SamplerCreateInfo &samplerCreateInfo);
		virtual bool DoUpdate() override;
		std::unique_ptr<Anvil::Sampler,std::function<void(Anvil::Sampler*)>> m_sampler = nullptr;
	};
};

#endif

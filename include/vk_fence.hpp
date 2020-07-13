/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_FENCE_HPP__
#define __PR_PROSPER_VK_FENCE_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_fence.hpp"
#include <wrappers/fence.h>

namespace prosper
{
	class DLLPROSPER_VK VlkFence
		: public IFence
	{
	public:
		static std::shared_ptr<VlkFence> Create(IPrContext &context,bool createSignalled=false,const std::function<void(IFence&)> &onDestroyedCallback=nullptr);
		virtual ~VlkFence() override;
		Anvil::Fence &GetAnvilFence() const;
		Anvil::Fence &operator*();
		const Anvil::Fence &operator*() const;
		Anvil::Fence *operator->();
		const Anvil::Fence *operator->() const;

		virtual bool IsSet() const override;
		virtual bool Reset() const override;
	protected:
		VlkFence(IPrContext &context,std::unique_ptr<Anvil::Fence,std::function<void(Anvil::Fence*)>> fence);
		std::unique_ptr<Anvil::Fence,std::function<void(Anvil::Fence*)>> m_fence = nullptr;
	};
};

#endif

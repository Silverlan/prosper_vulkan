// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_FENCE_HPP__
#define __PR_PROSPER_VK_FENCE_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_fence.hpp"
#include "debug/vk_debug_object.hpp"
#include <wrappers/fence.h>

namespace prosper {
	class DLLPROSPER_VK VlkFence : public IFence, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkFence> Create(IPrContext &context, bool createSignalled = false, const std::function<void(IFence &)> &onDestroyedCallback = nullptr);
		virtual ~VlkFence() override;
		Anvil::Fence &GetAnvilFence() const;
		Anvil::Fence &operator*();
		const Anvil::Fence &operator*() const;
		Anvil::Fence *operator->();
		const Anvil::Fence *operator->() const;

		virtual const void *GetInternalHandle() const override { return GetAnvilFence().get_fence(); }
		virtual bool IsSet() const override;
		virtual bool Reset() const override;
	  protected:
		VlkFence(IPrContext &context, std::unique_ptr<Anvil::Fence, std::function<void(Anvil::Fence *)>> fence);
		std::unique_ptr<Anvil::Fence, std::function<void(Anvil::Fence *)>> m_fence = nullptr;
	};
};

#endif

// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"
#include <wrappers/fence.h>

export module pragma.prosper.vulkan:fence;

export import :debug.object;

export namespace prosper {
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

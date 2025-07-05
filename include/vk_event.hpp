// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_EVENT_HPP__
#define __PR_PROSPER_VK_EVENT_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_event.hpp"
#include <wrappers/event.h>

namespace prosper {
	class DLLPROSPER_VK VlkEvent : public IEvent {
	  public:
		static std::shared_ptr<VlkEvent> Create(IPrContext &context, const std::function<void(IEvent &)> &onDestroyedCallback = nullptr);
		virtual ~VlkEvent() override;
		Anvil::Event &GetAnvilEvent() const;
		Anvil::Event &operator*();
		const Anvil::Event &operator*() const;
		Anvil::Event *operator->();
		const Anvil::Event *operator->() const;

		virtual bool IsSet() const override;
	  protected:
		VlkEvent(IPrContext &context, std::unique_ptr<Anvil::Event, std::function<void(Anvil::Event *)>> ev);
		std::unique_ptr<Anvil::Event, std::function<void(Anvil::Event *)>> m_event = nullptr;
	};
};

#endif

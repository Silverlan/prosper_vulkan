// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_prosper_vulkan.h"
#include "vk_event.hpp"
#include "vk_context.hpp"
#include <wrappers/event.h>
#include <wrappers/device.h>
#include <misc/event_create_info.h>

using namespace prosper;

std::shared_ptr<VlkEvent> VlkEvent::Create(IPrContext &context, const std::function<void(IEvent &)> &onDestroyedCallback)
{
	auto ev = Anvil::Event::create(Anvil::EventCreateInfo::create(&static_cast<VlkContext &>(context).GetDevice()));
	if(onDestroyedCallback == nullptr)
		return std::shared_ptr<VlkEvent>(new VlkEvent(context, std::move(ev)));
	return std::shared_ptr<VlkEvent>(new VlkEvent(context, std::move(ev)), [onDestroyedCallback](VlkEvent *ev) {
		ev->OnRelease();
		onDestroyedCallback(*ev);
		delete ev;
	});
}

VlkEvent::VlkEvent(IPrContext &context, Anvil::EventUniquePtr ev) : IEvent {context}, m_event(std::move(ev))
{
	//prosper::debug::register_debug_object(m_framebuffer->get_framebuffer(),this,prosper::debug::ObjectType::Event);
}
VlkEvent::~VlkEvent() {}

bool VlkEvent::IsSet() const { return m_event->is_set(); }

Anvil::Event &VlkEvent::GetAnvilEvent() const { return *m_event; }
Anvil::Event &VlkEvent::operator*() { return *m_event; }
const Anvil::Event &VlkEvent::operator*() const { return const_cast<VlkEvent *>(this)->operator*(); }
Anvil::Event *VlkEvent::operator->() { return m_event.get(); }
const Anvil::Event *VlkEvent::operator->() const { return const_cast<VlkEvent *>(this)->operator->(); }

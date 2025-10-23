// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/fence.h>
#include <wrappers/device.h>
#include <misc/fence_create_info.h>

module pragma.prosper.vulkan;

import :fence;

using namespace prosper;

std::shared_ptr<VlkFence> VlkFence::Create(IPrContext &context, bool createSignalled, const std::function<void(IFence &)> &onDestroyedCallback)
{
	auto fence = Anvil::Fence::create(Anvil::FenceCreateInfo::create(&static_cast<VlkContext &>(context).GetDevice(), createSignalled));
	if(onDestroyedCallback == nullptr)
		return std::shared_ptr<VlkFence>(new VlkFence(context, std::move(fence)));
	return std::shared_ptr<VlkFence>(new VlkFence(context, std::move(fence)), [onDestroyedCallback](VlkFence *fence) {
		fence->OnRelease();
		onDestroyedCallback(*fence);
		delete fence;
	});
}

VlkFence::VlkFence(IPrContext &context, Anvil::FenceUniquePtr fence) : IFence {context}, m_fence(std::move(fence))
{
	if(GetContext().IsValidationEnabled())
		VlkDebugObject::Init(GetContext(), debug::ObjectType::Fence, GetInternalHandle());
	//prosper::debug::register_debug_object(m_framebuffer->get_framebuffer(),this,prosper::debug::ObjectType::Fence);
}
VlkFence::~VlkFence()
{
	if(GetContext().IsValidationEnabled())
		VlkDebugObject::Clear(GetContext(), debug::ObjectType::Fence, GetInternalHandle());
}
bool VlkFence::IsSet() const { return m_fence->is_set(); }
bool VlkFence::Reset() const { return m_fence->reset(); }

Anvil::Fence &VlkFence::GetAnvilFence() const { return *m_fence; }
Anvil::Fence &VlkFence::operator*() { return *m_fence; }
const Anvil::Fence &VlkFence::operator*() const { return const_cast<VlkFence *>(this)->operator*(); }
Anvil::Fence *VlkFence::operator->() { return m_fence.get(); }
const Anvil::Fence *VlkFence::operator->() const { return const_cast<VlkFence *>(this)->operator->(); }

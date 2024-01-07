/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "image/vk_sampler.hpp"
#include "vk_context.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include "debug/vk_debug.hpp"
#include "vk_memory_tracker.hpp"
#include <misc/sampler_create_info.h>
#include <wrappers/device.h>
#include <wrappers/sampler.h>

using namespace prosper;

std::shared_ptr<VlkSampler> VlkSampler::Create(IPrContext &context, const prosper::util::SamplerCreateInfo &createInfo)
{
	auto &dev = static_cast<VlkContext &>(context).GetDevice();
	auto sampler = std::shared_ptr<VlkSampler> {new VlkSampler {context, createInfo}, [](VlkSampler *smp) {
		                                            smp->OnRelease();
		                                            delete smp;
	                                            }};
	if(sampler->Update() == false)
		return nullptr;
	return sampler;
}

VlkSampler::VlkSampler(IPrContext &context, const prosper::util::SamplerCreateInfo &samplerCreateInfo) : ISampler {context, samplerCreateInfo} {}
VlkSampler::~VlkSampler()
{
	if(GetContext().IsValidationEnabled())
		VlkDebugObject::Clear(GetContext(), debug::ObjectType::Sampler, GetInternalHandle());
	if(m_sampler != nullptr)
		prosper::debug::deregister_debug_object(m_sampler->get_sampler());
}
void VlkSampler::Bake() { GetAnvilSampler().get_sampler(); }
bool VlkSampler::DoUpdate()
{
	auto &createInfo = m_createInfo;
	auto &dev = static_cast<VlkContext &>(GetContext()).GetDevice();
	auto maxDeviceAnisotropy = dev.get_physical_device_properties().core_vk1_0_properties_ptr->limits.max_sampler_anisotropy;
	auto anisotropy = createInfo.maxAnisotropy;
	if(anisotropy == std::numeric_limits<decltype(anisotropy)>::max() || anisotropy > maxDeviceAnisotropy)
		anisotropy = maxDeviceAnisotropy;
	auto newSampler = Anvil::Sampler::create(Anvil::SamplerCreateInfo::create(&dev, static_cast<Anvil::Filter>(createInfo.magFilter), static_cast<Anvil::Filter>(createInfo.minFilter), static_cast<Anvil::SamplerMipmapMode>(createInfo.mipmapMode),
	  static_cast<Anvil::SamplerAddressMode>(createInfo.addressModeU), static_cast<Anvil::SamplerAddressMode>(createInfo.addressModeV), static_cast<Anvil::SamplerAddressMode>(createInfo.addressModeW), createInfo.mipLodBias, anisotropy, createInfo.compareEnable,
	  static_cast<Anvil::CompareOp>(createInfo.compareOp), createInfo.minLod, createInfo.maxLod, static_cast<Anvil::BorderColor>(createInfo.borderColor), false //,createInfo.useUnnormalizedCoordinates
	  ));
	if(newSampler != nullptr) {
		if(m_sampler != nullptr) {
			prosper::debug::deregister_debug_object(m_sampler->get_sampler());
			if(GetContext().IsValidationEnabled())
				VlkDebugObject::Clear(GetContext(), debug::ObjectType::Sampler, GetInternalHandle());
		}
		m_sampler = std::move(newSampler);
		prosper::debug::register_debug_object(m_sampler->get_sampler(), *this, prosper::debug::ObjectType::Sampler);
		Init(GetContext(), debug::ObjectType::Sampler, GetInternalHandle());
		return true;
	}
	return false;
}
Anvil::Sampler &VlkSampler::GetAnvilSampler() const { return *m_sampler; }
Anvil::Sampler &VlkSampler::operator*() { return *m_sampler; }
const Anvil::Sampler &VlkSampler::operator*() const { return const_cast<VlkSampler *>(this)->operator*(); }
Anvil::Sampler *VlkSampler::operator->() { return m_sampler.get(); }
const Anvil::Sampler *VlkSampler::operator->() const { return const_cast<VlkSampler *>(this)->operator->(); }

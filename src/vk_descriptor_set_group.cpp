// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_prosper_vulkan.h"
#include "vk_descriptor_set_group.hpp"
#include "buffers/vk_buffer.hpp"
#include "image/vk_image_view.hpp"
#include "image/vk_sampler.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include <wrappers/descriptor_pool.h>

using namespace prosper;

std::shared_ptr<VlkDescriptorSetGroup> VlkDescriptorSetGroup::Create(IPrContext &context, const DescriptorSetCreateInfo &createInfo, Anvil::DescriptorSetGroupUniquePtr imgView, const std::function<void(IDescriptorSetGroup &)> &onDestroyedCallback)
{
	if(imgView == nullptr)
		return nullptr;
	if(onDestroyedCallback == nullptr)
		return std::shared_ptr<VlkDescriptorSetGroup>(new VlkDescriptorSetGroup(context, createInfo, std::move(imgView)));
	return std::shared_ptr<VlkDescriptorSetGroup>(new VlkDescriptorSetGroup(context, createInfo, std::move(imgView)), [onDestroyedCallback](VlkDescriptorSetGroup *buf) {
		buf->OnRelease();
		onDestroyedCallback(*buf);
		delete buf;
	});
}

VlkDescriptorSetGroup::VlkDescriptorSetGroup(IPrContext &context, const DescriptorSetCreateInfo &createInfo, Anvil::DescriptorSetGroupUniquePtr imgView) : IDescriptorSetGroup {context, createInfo}, m_descriptorSetGroup(std::move(imgView))
{
	auto numSets = m_descriptorSetGroup->get_n_descriptor_sets();
	if(prosper::debug::is_debug_mode_enabled()) {
		for(auto i = decltype(numSets) {0}; i < numSets; ++i)
			prosper::debug::register_debug_object(m_descriptorSetGroup->get_descriptor_set(i)->get_descriptor_set_vk(), *this, prosper::debug::ObjectType::DescriptorSet);
		auto *pool = m_descriptorSetGroup->get_descriptor_pool();
		if(pool) {
			auto &vkPool = pool->get_descriptor_pool();
			if(vkPool)
				prosper::debug::register_debug_object(vkPool, *this, prosper::debug::ObjectType::DescriptorSet);
		}
	}
	m_descriptorSets.resize(numSets);

	for(auto i = decltype(m_descriptorSets.size()) {0u}; i < m_descriptorSets.size(); ++i) {
		m_descriptorSets.at(i) = std::shared_ptr<VlkDescriptorSet> {new VlkDescriptorSet {*this, *m_descriptorSetGroup->get_descriptor_set(i)}, [](VlkDescriptorSet *ds) {
			                                                            // ds->OnRelease();
			                                                            delete ds;
		                                                            }};
	}
}
VlkDescriptorSetGroup::~VlkDescriptorSetGroup()
{
	if(prosper::debug::is_debug_mode_enabled()) {
		auto numSets = m_descriptorSetGroup->get_n_descriptor_sets();
		for(auto i = decltype(numSets) {0}; i < numSets; ++i)
			prosper::debug::deregister_debug_object(m_descriptorSetGroup->get_descriptor_set(i)->get_descriptor_set_vk(false));
		auto *pool = m_descriptorSetGroup->get_descriptor_pool();
		if(pool) {
			auto &vkPool = pool->get_descriptor_pool();
			if(vkPool)
				prosper::debug::deregister_debug_object(vkPool);
		}
	}
}

VlkDescriptorSet::VlkDescriptorSet(VlkDescriptorSetGroup &dsg, Anvil::DescriptorSet &ds) : IDescriptorSet {dsg}, m_descSet {ds}
{
	m_apiTypePtr = static_cast<VlkDescriptorSet *>(this);
	if(dsg.GetContext().IsValidationEnabled())
		VlkDebugObject::Init(dsg.GetContext(), debug::ObjectType::DescriptorSet, GetVkDescriptorSet());
}

VlkDescriptorSet::~VlkDescriptorSet()
{
	auto &dsg = GetDescriptorSetGroup();
	if(dsg.GetContext().IsValidationEnabled())
		VlkDebugObject::Clear(dsg.GetContext(), debug::ObjectType::DescriptorSet, m_descSet.get_descriptor_set_vk(false));
}

VkDescriptorSet VlkDescriptorSet::GetVkDescriptorSet() const { return m_descSet.get_descriptor_set_vk(); }

Anvil::DescriptorSet &VlkDescriptorSet::GetAnvilDescriptorSet() const { return m_descSet; }
Anvil::DescriptorSet &VlkDescriptorSet::operator*() { return m_descSet; }
const Anvil::DescriptorSet &VlkDescriptorSet::operator*() const { return const_cast<VlkDescriptorSet *>(this)->operator*(); }
Anvil::DescriptorSet *VlkDescriptorSet::operator->() { return &m_descSet; }
const Anvil::DescriptorSet *VlkDescriptorSet::operator->() const { return const_cast<VlkDescriptorSet *>(this)->operator->(); }

bool VlkDescriptorSet::Update()
{
	auto res = m_descSet.update();
	if(prosper::debug::is_debug_mode_enabled())
		prosper::debug::register_debug_object(GetAnvilDescriptorSet().get_descriptor_set_vk(), GetDescriptorSetGroup(), prosper::debug::ObjectType::DescriptorSet);
	return res;
}
bool VlkDescriptorSet::DoSetBindingStorageImage(prosper::Texture &texture, uint32_t bindingIdx, const std::optional<uint32_t> &layerId)
{
	auto *imgView = layerId.has_value() ? texture.GetImageView(*layerId) : texture.GetImageView();
	if(imgView == nullptr)
		return false;
	SetBinding(bindingIdx, std::make_unique<DescriptorSetBindingStorageImage>(*this, bindingIdx, texture));
	return GetAnvilDescriptorSet().set_binding_item(bindingIdx, Anvil::DescriptorSet::StorageImageBindingElement {Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL, &static_cast<VlkImageView *>(imgView)->GetAnvilImageView()});
}
bool VlkDescriptorSet::DoSetBindingTexture(prosper::Texture &texture, uint32_t bindingIdx, const std::optional<uint32_t> &layerId)
{
	auto *imgView = layerId.has_value() ? texture.GetImageView(*layerId) : texture.GetImageView();
	auto *sampler = texture.GetSampler();
	if(imgView == nullptr || sampler == nullptr)
		return false;
	SetBinding(bindingIdx, std::make_unique<DescriptorSetBindingTexture>(*this, bindingIdx, texture));
	return GetAnvilDescriptorSet().set_binding_item(bindingIdx, Anvil::DescriptorSet::CombinedImageSamplerBindingElement {Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL, &static_cast<VlkImageView *>(imgView)->GetAnvilImageView(), &static_cast<VlkSampler *>(sampler)->GetAnvilSampler()});
}
bool VlkDescriptorSet::DoSetBindingArrayTexture(prosper::Texture &texture, uint32_t bindingIdx, uint32_t arrayIndex, const std::optional<uint32_t> &layerId)
{
	std::vector<Anvil::DescriptorSet::CombinedImageSamplerBindingElement> bindingElements(1u,
	  Anvil::DescriptorSet::CombinedImageSamplerBindingElement {Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL, &static_cast<VlkImageView *>(layerId.has_value() ? texture.GetImageView(*layerId) : texture.GetImageView())->GetAnvilImageView(),
	    &static_cast<VlkSampler *>(texture.GetSampler())->GetAnvilSampler()});
	auto *binding = GetBinding(bindingIdx);
	if(binding == nullptr)
		binding = &SetBinding(bindingIdx, std::make_unique<DescriptorSetBindingArrayTexture>(*this, bindingIdx));
	if(binding && binding->GetType() == prosper::DescriptorSetBinding::Type::ArrayTexture)
		static_cast<prosper::DescriptorSetBindingArrayTexture *>(binding)->SetArrayBinding(arrayIndex, std::make_unique<DescriptorSetBindingTexture>(*this, bindingIdx, texture, layerId));
	return GetAnvilDescriptorSet().set_binding_array_items(bindingIdx, {arrayIndex, 1u}, bindingElements.data());
}
bool VlkDescriptorSet::DoSetBindingUniformBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size)
{
	return GetAnvilDescriptorSet().set_binding_item(bindingIdx, Anvil::DescriptorSet::UniformBufferBindingElement {&buffer.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(), buffer.GetStartOffset() + startOffset, size});
}
bool VlkDescriptorSet::DoSetBindingDynamicUniformBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size)
{
	return GetAnvilDescriptorSet().set_binding_item(bindingIdx, Anvil::DescriptorSet::DynamicUniformBufferBindingElement {&buffer.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(), buffer.GetStartOffset() + startOffset, size});
}
bool VlkDescriptorSet::DoSetBindingStorageBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size)
{
	return GetAnvilDescriptorSet().set_binding_item(bindingIdx, Anvil::DescriptorSet::StorageBufferBindingElement {&buffer.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(), buffer.GetStartOffset() + startOffset, size});
}
bool VlkDescriptorSet::DoSetBindingDynamicStorageBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size)
{
	return GetAnvilDescriptorSet().set_binding_item(bindingIdx, Anvil::DescriptorSet::DynamicStorageBufferBindingElement {&buffer.GetAPITypeRef<VlkBuffer>().GetBaseAnvilBuffer(), buffer.GetStartOffset() + startOffset, size});
}

Anvil::DescriptorSetGroup &VlkDescriptorSetGroup::GetAnvilDescriptorSetGroup() const { return *m_descriptorSetGroup; }
Anvil::DescriptorSetGroup &VlkDescriptorSetGroup::operator*() { return *m_descriptorSetGroup; }
const Anvil::DescriptorSetGroup &VlkDescriptorSetGroup::operator*() const { return const_cast<VlkDescriptorSetGroup *>(this)->operator*(); }
Anvil::DescriptorSetGroup *VlkDescriptorSetGroup::operator->() { return m_descriptorSetGroup.get(); }
const Anvil::DescriptorSetGroup *VlkDescriptorSetGroup::operator->() const { return const_cast<VlkDescriptorSetGroup *>(this)->operator->(); }

/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_DESCRIPTOR_SET_GROUP_HPP__
#define __PR_PROSPER_VK_DESCRIPTOR_SET_GROUP_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_descriptor_set_group.hpp"
#include <wrappers/descriptor_set_group.h>

namespace prosper
{
	class DLLPROSPER_VK VlkDescriptorSetGroup
		: public IDescriptorSetGroup
	{
	public:
		static std::shared_ptr<VlkDescriptorSetGroup> Create(IPrContext &context,const DescriptorSetCreateInfo &createInfo,std::unique_ptr<Anvil::DescriptorSetGroup,std::function<void(Anvil::DescriptorSetGroup*)>> dsg,const std::function<void(IDescriptorSetGroup&)> &onDestroyedCallback=nullptr);
		virtual ~VlkDescriptorSetGroup() override;

		Anvil::DescriptorSetGroup &GetAnvilDescriptorSetGroup() const;
		Anvil::DescriptorSetGroup &operator*();
		const Anvil::DescriptorSetGroup &operator*() const;
		Anvil::DescriptorSetGroup *operator->();
		const Anvil::DescriptorSetGroup *operator->() const;
	protected:
		VlkDescriptorSetGroup(IPrContext &context,const DescriptorSetCreateInfo &createInfo,std::unique_ptr<Anvil::DescriptorSetGroup,std::function<void(Anvil::DescriptorSetGroup*)>> dsg);
		std::unique_ptr<Anvil::DescriptorSetGroup,std::function<void(Anvil::DescriptorSetGroup*)>> m_descriptorSetGroup = nullptr;
	};

	class DLLPROSPER_VK VlkDescriptorSet
		: public IDescriptorSet
	{
	public:
		VlkDescriptorSet(VlkDescriptorSetGroup &dsg,Anvil::DescriptorSet &ds);

		Anvil::DescriptorSet &GetAnvilDescriptorSet() const;
		Anvil::DescriptorSet &operator*();
		const Anvil::DescriptorSet &operator*() const;
		Anvil::DescriptorSet *operator->();
		const Anvil::DescriptorSet *operator->() const;

		VkDescriptorSet GetVkDescriptorSet() const {return m_vkDescSet;};
		virtual bool Update() override;
	protected:
		virtual bool DoSetBindingStorageImage(prosper::Texture &texture,uint32_t bindingIdx,const std::optional<uint32_t> &layerId) override;
		virtual bool DoSetBindingTexture(prosper::Texture &texture,uint32_t bindingIdx,const std::optional<uint32_t> &layerId) override;
		virtual bool DoSetBindingArrayTexture(prosper::Texture &texture,uint32_t bindingIdx,uint32_t arrayIndex,const std::optional<uint32_t> &layerId) override;
		virtual bool DoSetBindingUniformBuffer(prosper::IBuffer &buffer,uint32_t bindingIdx,uint64_t startOffset,uint64_t size) override;
		virtual bool DoSetBindingDynamicUniformBuffer(prosper::IBuffer &buffer,uint32_t bindingIdx,uint64_t startOffset,uint64_t size) override;
		virtual bool DoSetBindingStorageBuffer(prosper::IBuffer &buffer,uint32_t bindingIdx,uint64_t startOffset,uint64_t size) override;
	private:
		Anvil::DescriptorSet &m_descSet;
		VkDescriptorSet m_vkDescSet;
	};
};

#endif

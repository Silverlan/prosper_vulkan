// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_DESCRIPTOR_SET_GROUP_HPP__
#define __PR_PROSPER_VK_DESCRIPTOR_SET_GROUP_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_descriptor_set_group.hpp"
#include "debug/vk_debug_object.hpp"
#include <wrappers/descriptor_set_group.h>

namespace prosper {
	class DLLPROSPER_VK VlkDescriptorSetGroup : public IDescriptorSetGroup {
	  public:
		static std::shared_ptr<VlkDescriptorSetGroup> Create(IPrContext &context, const DescriptorSetCreateInfo &createInfo, std::unique_ptr<Anvil::DescriptorSetGroup, std::function<void(Anvil::DescriptorSetGroup *)>> dsg,
		  const std::function<void(IDescriptorSetGroup &)> &onDestroyedCallback = nullptr);
		virtual ~VlkDescriptorSetGroup() override;

		Anvil::DescriptorSetGroup &GetAnvilDescriptorSetGroup() const;
		Anvil::DescriptorSetGroup &operator*();
		const Anvil::DescriptorSetGroup &operator*() const;
		Anvil::DescriptorSetGroup *operator->();
		const Anvil::DescriptorSetGroup *operator->() const;
	  protected:
		VlkDescriptorSetGroup(IPrContext &context, const DescriptorSetCreateInfo &createInfo, std::unique_ptr<Anvil::DescriptorSetGroup, std::function<void(Anvil::DescriptorSetGroup *)>> dsg);
		std::unique_ptr<Anvil::DescriptorSetGroup, std::function<void(Anvil::DescriptorSetGroup *)>> m_descriptorSetGroup = nullptr;
	};

	class DLLPROSPER_VK VlkDescriptorSet : public IDescriptorSet, public VlkDebugObject {
	  public:
		VlkDescriptorSet(VlkDescriptorSetGroup &dsg, Anvil::DescriptorSet &ds);
		virtual ~VlkDescriptorSet() override;

		Anvil::DescriptorSet &GetAnvilDescriptorSet() const;
		Anvil::DescriptorSet &operator*();
		const Anvil::DescriptorSet &operator*() const;
		Anvil::DescriptorSet *operator->();
		const Anvil::DescriptorSet *operator->() const;

		VkDescriptorSet GetVkDescriptorSet() const;
		virtual bool Update() override;
	  protected:
		virtual bool DoSetBindingStorageImage(prosper::Texture &texture, uint32_t bindingIdx, const std::optional<uint32_t> &layerId) override;
		virtual bool DoSetBindingTexture(prosper::Texture &texture, uint32_t bindingIdx, const std::optional<uint32_t> &layerId) override;
		virtual bool DoSetBindingArrayTexture(prosper::Texture &texture, uint32_t bindingIdx, uint32_t arrayIndex, const std::optional<uint32_t> &layerId) override;
		virtual bool DoSetBindingUniformBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size) override;
		virtual bool DoSetBindingDynamicUniformBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size) override;
		virtual bool DoSetBindingStorageBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size) override;
		virtual bool DoSetBindingDynamicStorageBuffer(prosper::IBuffer &buffer, uint32_t bindingIdx, uint64_t startOffset, uint64_t size) override;
	  private:
		Anvil::DescriptorSet &m_descSet;
	};
};

#endif

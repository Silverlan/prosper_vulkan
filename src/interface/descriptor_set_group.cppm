// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"
#include <wrappers/descriptor_set_group.h>
#include <optional>

export module pragma.prosper.vulkan:descriptor_set_group;

export import :debug.object;

export namespace prosper {
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

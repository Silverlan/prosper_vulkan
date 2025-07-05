// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_PROSPER_VK_BUFFER_HPP__
#define __PR_PROSPER_VK_BUFFER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_includes.hpp"
#include "prosper_context_object.hpp"
#include "buffers/prosper_buffer.hpp"
#include "debug/vk_debug_object.hpp"
#include "buffers/prosper_buffer_create_info.hpp"
#include <wrappers/buffer.h>
#include <mathutil/umath.h>
#include <memory>
#include <cinttypes>
#include <functional>

namespace Anvil {
	class Buffer;
};

namespace prosper {
	class VkDynamicResizableBuffer;
	class VkUniformResizableBuffer;
	class DLLPROSPER_VK VlkBuffer : virtual public IBuffer, public VlkDebugObject {
	  public:
		static std::shared_ptr<VlkBuffer> Create(IPrContext &context, Anvil::BufferUniquePtr buf, const util::BufferCreateInfo &bufCreateInfo, DeviceSize startOffset, DeviceSize size, const std::function<void(IBuffer &)> &onDestroyedCallback = nullptr);

		virtual ~VlkBuffer() override;

		std::shared_ptr<VlkBuffer> GetParent();
		const std::shared_ptr<VlkBuffer> GetParent() const;
		virtual std::shared_ptr<IBuffer> CreateSubBuffer(DeviceSize offset, DeviceSize size, const std::function<void(IBuffer &)> &onDestroyedCallback = nullptr) override;

		virtual void Bake() override;

		Anvil::Buffer &GetAnvilBuffer() const;
		Anvil::Buffer &GetBaseAnvilBuffer() const;
		Anvil::Buffer &operator*();
		const Anvil::Buffer &operator*() const;
		Anvil::Buffer *operator->();
		const Anvil::Buffer *operator->() const;

		virtual const void *GetInternalHandle() const override { return GetVkBuffer(); }
		VkBuffer GetVkBuffer() const { return m_vkBuffer; };
		virtual void Initialize() override;
	  protected:
		friend IDynamicResizableBuffer;
		friend VkDynamicResizableBuffer;
		friend IUniformResizableBuffer;
		friend VkUniformResizableBuffer;
		VlkBuffer(IPrContext &context, const util::BufferCreateInfo &bufCreateInfo, DeviceSize startOffset, DeviceSize size, std::unique_ptr<Anvil::Buffer, std::function<void(Anvil::Buffer *)>> buf);
		virtual void RecreateInternalSubBuffer(IBuffer &newParentBuffer) override;
		virtual bool DoWrite(Offset offset, Size size, const void *data) const override;
		virtual bool DoRead(Offset offset, Size size, void *data) const override;
		virtual bool DoMap(Offset offset, Size size, MapFlags mapFlags, void **optOutMappedPtr) const override;
		virtual bool DoUnmap() const override;

		std::unique_ptr<Anvil::Buffer, std::function<void(Anvil::Buffer *)>> m_buffer = nullptr;
		VkBuffer m_vkBuffer;
	  private:
		void SetBuffer(std::unique_ptr<Anvil::Buffer, std::function<void(Anvil::Buffer *)>> buf);
	};
};

#endif

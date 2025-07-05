// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __VK_MEMORY_TRACKER_HPP__
#define __VK_MEMORY_TRACKER_HPP__

#include "prosper_vulkan_definitions.hpp"
#include <mathutil/umath.h>
#include <vector>
#include <memory>

namespace Anvil {
	class MemoryBlock;
};

#pragma warning(push)
#pragma warning(disable : 4251)
namespace prosper {
	class IBuffer;
	class IPrContext;
	class IImage;
	class DLLPROSPER_VK MemoryTracker {
	  public:
		struct DLLPROSPER_VK Resource {
			enum class TypeFlags : uint8_t {
				None = 0u,
				ImageBit = 1u,
				BufferBit = ImageBit << 1u,
				UniformBufferBit = BufferBit << 1u,
				DynamicBufferBit = UniformBufferBit << 1u,
				StandAloneBufferBit = DynamicBufferBit << 1u,

				Any = std::numeric_limits<uint8_t>::max()
			};
			void *resource;
			TypeFlags typeFlags;
			Anvil::MemoryBlock *GetMemoryBlock(uint32_t i) const;
			uint32_t GetMemoryBlockCount() const;
		};
		static MemoryTracker &GetInstance();

		bool GetMemoryStats(prosper::IPrContext &context, uint32_t memType, uint64_t &allocatedSize, uint64_t &totalSize, Resource::TypeFlags typeFlags = Resource::TypeFlags::Any) const;
		void GetResources(uint32_t memType, std::vector<const Resource *> &outResources, Resource::TypeFlags typeFlags = Resource::TypeFlags::Any) const;
		const std::vector<Resource> &GetResources() const;

		void AddResource(IBuffer &buffer);
		void AddResource(IImage &buffer);
		void RemoveResource(IBuffer &buffer);
		void RemoveResource(IImage &buffer);
	  private:
		void RemoveResource(void *resource);
		MemoryTracker() = default;
		std::vector<Resource> m_resources = {};
		mutable std::mutex m_resourceMutex;
	};
};
REGISTER_BASIC_BITWISE_OPERATORS(prosper::MemoryTracker::Resource::TypeFlags)
#pragma warning(pop)

#endif

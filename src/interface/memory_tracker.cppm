// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "util_enum_flags.hpp"

#include <wrappers/memory_block.h>

export module pragma.prosper.vulkan:memory_tracker;

export import pragma.prosper;

#undef max

#pragma warning(push)
#pragma warning(disable : 4251)
export namespace prosper {
	class PR_EXPORT MemoryTracker {
	  public:
		struct PR_EXPORT Resource {
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
	using namespace umath::scoped_enum::bitwise;
};
export {
	REGISTER_ENUM_FLAGS(prosper::MemoryTracker::Resource::TypeFlags)
}
#pragma warning(pop)

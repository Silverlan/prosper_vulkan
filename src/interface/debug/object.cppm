// SPDX-FileCopyrightText: (c) 2024 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"
#include <limits>

export module pragma.prosper.vulkan:debug.object;

export import pragma.prosper;

import :debug.lookup_map;

export namespace prosper {
	class DLLPROSPER_VK VlkDebugObject {
	  public:
		VlkDebugObject() = default;
		size_t GetDebugIndex() const { return m_debugIndex; }
	  protected:
		void Init(IPrContext &context, debug::ObjectType type, const void *vkPtr);
		void Clear(IPrContext &context, debug::ObjectType type, const void *vkPtr);
	  private:
		size_t m_debugIndex = std::numeric_limits<size_t>::max();
	};
};

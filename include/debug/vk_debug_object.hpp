/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_DEBUG_OBJECT_HPP__
#define __PR_PROSPER_VK_DEBUG_OBJECT_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include <limits>

namespace prosper {
	class IPrContext;
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

#endif

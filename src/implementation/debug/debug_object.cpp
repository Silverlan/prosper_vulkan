// SPDX-FileCopyrightText: (c) 2024 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <sharedutils/magic_enum.hpp>
#include <ios>

module pragma.prosper.vulkan;

import :debug.object;

using namespace prosper;

static std::array<size_t, umath::to_integral(prosper::debug::ObjectType::Count)> g_objIndex {};
void VlkDebugObject::Init(IPrContext &context, debug::ObjectType type, const void *vkPtr)
{
	if(context.IsValidationEnabled()) {
		if(vkPtr) {
			auto &idx = g_objIndex[umath::to_integral(type)];
			std::stringstream ss;
			ss << "Created Vulkan object " << magic_enum::enum_name(type) << " 0x" << std::hex << reinterpret_cast<std::uintptr_t>(vkPtr) << " with global index " << std::dec << idx;
			context.Log(ss.str());
			m_debugIndex = idx;
			++idx;
		}
	}
}
void VlkDebugObject::Clear(IPrContext &context, debug::ObjectType type, const void *vkPtr)
{
	if(context.IsValidationEnabled()) {
		if(vkPtr && m_debugIndex != std::numeric_limits<size_t>::max()) {
			std::stringstream ss;
			ss << "Destroying Vulkan object " << magic_enum::enum_name(type) << " 0x" << std::hex << reinterpret_cast<std::uintptr_t>(vkPtr) << " with global index " << std::dec << m_debugIndex;
			m_debugIndex = std::numeric_limits<size_t>::max();
			context.Log(ss.str());
		}
	}
}

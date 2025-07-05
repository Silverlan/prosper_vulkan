// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_prosper_vulkan.h"
#include "vk_context.hpp"
#include "vk_swap_command_buffer.hpp"
#include <prosper_command_buffer.hpp>

using namespace prosper;

std::shared_ptr<prosper::ISwapCommandBufferGroup> prosper::VlkContext::CreateSwapCommandBufferGroup(Window &window, bool allowMt, const std::string &debugName)
{
	if(allowMt)
		return std::make_shared<MtSwapCommandBufferGroup>(window, debugName);
	return std::make_shared<StSwapCommandBufferGroup>(window, debugName);
}

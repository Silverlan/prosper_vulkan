/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_context.hpp"
#include "vk_swap_command_buffer.hpp"
#include <prosper_command_buffer.hpp>

using namespace prosper;
#pragma optimize("",off)
std::shared_ptr<prosper::ISwapCommandBufferGroup> prosper::VlkContext::CreateSwapCommandBufferGroup() {return std::make_shared<VlkSwapCommandBufferGroup>(*this);}
#pragma optimize("",on)

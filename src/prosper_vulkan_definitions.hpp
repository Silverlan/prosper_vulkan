// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PROSPER_VULKAN_DEFINITIONS_HPP__
#define __PROSPER_VULKAN_DEFINITIONS_HPP__

#include <prosper_definitions.hpp>

#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#else
#define VK_USE_PLATFORM_XCB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#ifdef SHPROSPER_VULKAN_STATIC
#define DLLPROSPER_VK
#elif SHPROSPER_VULKAN_DLL
#ifdef __linux__
#define DLLPROSPER_VK __attribute__((visibility("default")))
#else
#define DLLPROSPER_VK __declspec(dllexport)
#endif
#else
#ifdef __linux__
#define DLLPROSPER_VK
#else
#define DLLPROSPER_VK __declspec(dllimport)
#endif
#endif

#endif

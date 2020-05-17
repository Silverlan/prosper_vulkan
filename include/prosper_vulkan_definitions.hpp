/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PROSPER_VULKAN_DEFINITIONS_HPP__
#define __PROSPER_VULKAN_DEFINITIONS_HPP__

#include <prosper_definitions.hpp>

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

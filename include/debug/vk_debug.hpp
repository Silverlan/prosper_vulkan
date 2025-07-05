// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#ifndef __PR_VK_DEBUG_HPP__
#define __PR_VK_DEBUG_HPP__

#include "prosper_vulkan_definitions.hpp"
#include <sstream>

namespace Anvil {
	class Image;
};
namespace prosper {
	class IPrContext;
	class VlkImage;
};
namespace prosper::debug {
	DLLPROSPER_VK void dump_layers(IPrContext &context, std::stringstream &out);
	DLLPROSPER_VK void dump_extensions(IPrContext &context, std::stringstream &out);
	DLLPROSPER_VK void dump_limits(IPrContext &context, std::stringstream &out);
	DLLPROSPER_VK void dump_features(IPrContext &context, std::stringstream &out);
	DLLPROSPER_VK void dump_image_format_properties(IPrContext &context, std::stringstream &out, prosper::ImageCreateFlags createFlags = prosper::ImageCreateFlags {});
	DLLPROSPER_VK void dump_format_properties(IPrContext &context, std::stringstream &out);
	DLLPROSPER_VK void dump_layers(IPrContext &context, const std::string &fileName);
	DLLPROSPER_VK void dump_extensions(IPrContext &context, const std::string &fileName);
	DLLPROSPER_VK void dump_limits(IPrContext &context, const std::string &fileName);
	DLLPROSPER_VK void dump_features(IPrContext &context, const std::string &fileName);
	DLLPROSPER_VK void dump_image_format_properties(IPrContext &context, const std::string &fileName, prosper::ImageCreateFlags createFlags = prosper::ImageCreateFlags {});
	DLLPROSPER_VK void dump_format_properties(IPrContext &context, const std::string &fileName);

	DLLPROSPER_VK VlkImage *get_image_from_anvil_image(Anvil::Image &img);

};

#endif

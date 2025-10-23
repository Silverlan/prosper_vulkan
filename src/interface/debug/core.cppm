// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "prosper_vulkan_definitions.hpp"
#include <wrappers/image.h>
#include <sstream>

export module pragma.prosper.vulkan:debug.core;

export import pragma.prosper;

export {
	namespace prosper {
		class VlkImage;
	}
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
}

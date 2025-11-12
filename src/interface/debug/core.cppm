// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/image.h>

export module pragma.prosper.vulkan:debug.core;

export import pragma.prosper;

export {
	namespace prosper {
		class VlkImage;
	}
	namespace prosper::debug {
		PR_EXPORT void dump_layers(IPrContext &context, std::stringstream &out);
		PR_EXPORT void dump_extensions(IPrContext &context, std::stringstream &out);
		PR_EXPORT void dump_limits(IPrContext &context, std::stringstream &out);
		PR_EXPORT void dump_features(IPrContext &context, std::stringstream &out);
		PR_EXPORT void dump_image_format_properties(IPrContext &context, std::stringstream &out, prosper::ImageCreateFlags createFlags = prosper::ImageCreateFlags {});
		PR_EXPORT void dump_format_properties(IPrContext &context, std::stringstream &out);
		PR_EXPORT void dump_layers(IPrContext &context, const std::string &fileName);
		PR_EXPORT void dump_extensions(IPrContext &context, const std::string &fileName);
		PR_EXPORT void dump_limits(IPrContext &context, const std::string &fileName);
		PR_EXPORT void dump_features(IPrContext &context, const std::string &fileName);
		PR_EXPORT void dump_image_format_properties(IPrContext &context, const std::string &fileName, prosper::ImageCreateFlags createFlags = prosper::ImageCreateFlags {});
		PR_EXPORT void dump_format_properties(IPrContext &context, const std::string &fileName);

		PR_EXPORT VlkImage *get_image_from_anvil_image(Anvil::Image &img);

	};
}

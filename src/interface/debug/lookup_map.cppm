// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "vulkan_api.hpp"

export module pragma.prosper.vulkan:debug.lookup_map;

export import pragma.prosper;

#undef max

export {
	namespace prosper {
		class VlkImage;
		class VlkImageView;
		class VlkSampler;
		class VlkBuffer;
		class VlkCommandBuffer;
		class VlkRenderPass;
		class VlkFramebuffer;
		class VlkDescriptorSetGroup;
		namespace debug {
			enum class ObjectType : uint32_t {
				Image = 0u,
				ImageView,
				Sampler,
				Buffer,
				CommandBuffer,
				RenderPass,
				Framebuffer,
				DescriptorSet,
				Pipeline,
				Fence,
				Count,
			};
			struct ShaderPipelineInfo {
				Shader *shader = nullptr;
				uint32_t pipelineIdx = std::numeric_limits<uint32_t>::max();
			};
			PR_EXPORT void set_debug_mode_enabled(bool b);
			PR_EXPORT bool is_debug_mode_enabled();
			PR_EXPORT void register_debug_object(void *vkPtr, prosper::ContextObject &obj, ObjectType type);
			PR_EXPORT void register_debug_shader_pipeline(void *vkPtr, const ShaderPipelineInfo &pipelineInfo);
			PR_EXPORT void deregister_debug_object(void *vkPtr);

			PR_EXPORT void *get_object(void *vkObj, ObjectType &type, std::string *optOutBacktrace = nullptr);
			PR_EXPORT VlkImage *get_image(const vk::Image &vkImage);
			PR_EXPORT VlkImageView *get_image_view(const vk::ImageView &vkImageView);
			PR_EXPORT VlkSampler *get_sampler(const vk::Sampler &vkSampler);
			PR_EXPORT VlkBuffer *get_buffer(const vk::Buffer &vkBuffer);
			PR_EXPORT VlkCommandBuffer *get_command_buffer(const vk::CommandBuffer &vkBuffer);
			PR_EXPORT VlkRenderPass *get_render_pass(const vk::RenderPass &vkBuffer);
			PR_EXPORT VlkFramebuffer *get_framebuffer(const vk::Framebuffer &vkBuffer);
			PR_EXPORT VlkDescriptorSetGroup *get_descriptor_set_group(const vk::DescriptorSet &vkBuffer);
			PR_EXPORT ShaderPipelineInfo *get_shader_pipeline(const vk::Pipeline &vkPipeline);
		};
	};
}

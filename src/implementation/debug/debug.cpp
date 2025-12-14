// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include "vulkan_api.hpp"
#include <wrappers/device.h>
#include <wrappers/physical_device.h>

module pragma.prosper.vulkan;

import :debug.core;
import pragma.filesystem;

using namespace prosper;

void prosper::debug::dump_layers(IPrContext &context, std::stringstream &ss)
{
	uint32_t instanceLayerCount = 0;
	auto err = static_cast<vk::Result>(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
	if(err != vk::Result::eSuccess)
		return;
	std::vector<vk::LayerProperties> instanceLayers(instanceLayerCount);
	err = static_cast<vk::Result>(vkEnumerateInstanceLayerProperties(&instanceLayerCount, reinterpret_cast<VkLayerProperties *>(instanceLayers.data())));
	if(err != vk::Result::eSuccess)
		return;
	auto *dev = static_cast<VlkContext &>(context).GetDevice().get_physical_device()->get_physical_device();
	uint32_t deviceLayerCount = 0;
	err = static_cast<vk::Result>(vkEnumerateDeviceLayerProperties(dev, &deviceLayerCount, nullptr));
	if(err != vk::Result::eSuccess)
		return;
	std::vector<vk::LayerProperties> deviceLayers(deviceLayerCount);
	err = static_cast<vk::Result>(vkEnumerateDeviceLayerProperties(dev, &instanceLayerCount, reinterpret_cast<VkLayerProperties *>(deviceLayers.data())));
	if(err != vk::Result::eSuccess)
		return;

	ss << "Instance Layer Count: " << instanceLayers.size() << "\n\n";
	for(auto &prop : instanceLayers) {
		ss << prop.layerName << "\n";
		ss << "\tDescription: " << prop.description << "\n";
		ss << "\tImplementation Version: " << prop.implementationVersion << "\n";
		ss << "\tSpecification Version: " << prop.specVersion << "\n";
		ss << "\n";
	}
	ss << "\n\n";
	ss << "Device Layer Count: " << deviceLayers.size() << "\n\n";
	for(auto &prop : deviceLayers) {
		ss << prop.layerName << "\n";
		ss << "\tDescription: " << prop.description << "\n";
		ss << "\tImplementation Version: " << prop.implementationVersion << "\n";
		ss << "\tSpecification Version: " << prop.specVersion << "\n";
		ss << "\n";
	}
}
void prosper::debug::dump_extensions(IPrContext &context, std::stringstream &ss)
{
	uint32_t instanceExtensionCount = 0;
	auto err = static_cast<vk::Result>(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
	if(err != vk::Result::eSuccess)
		return;
	std::vector<vk::ExtensionProperties> instanceExtensions(instanceExtensionCount);
	err = vk::enumerateInstanceExtensionProperties("", &instanceExtensionCount, instanceExtensions.data());
	if(err != vk::Result::eSuccess)
		return;
	auto *dev = static_cast<VlkContext &>(context).GetDevice().get_physical_device()->get_physical_device();
	uint32_t deviceExtensionCount = 0;
	err = static_cast<vk::Result>(vkEnumerateDeviceExtensionProperties(dev, nullptr, &deviceExtensionCount, nullptr));
	if(err != vk::Result::eSuccess)
		return;
	std::vector<vk::ExtensionProperties> deviceExtensions(deviceExtensionCount);
	err = static_cast<vk::Result>(vkEnumerateDeviceExtensionProperties(dev, "", &deviceExtensionCount, reinterpret_cast<VkExtensionProperties *>(deviceExtensions.data())));
	if(err != vk::Result::eSuccess)
		return;

	ss << "Instance Extension Count: " << instanceExtensions.size() << "\n\n";
	for(auto &prop : instanceExtensions) {
		ss << prop.extensionName << "\n";
		ss << "\tSpecification Version: " << prop.specVersion << "\n";
		ss << "\n";
	}
	ss << "\n\n";
	ss << "Device Extension Count: " << deviceExtensions.size() << "\n\n";
	for(auto &prop : deviceExtensions) {
		ss << prop.extensionName << "\n";
		ss << "\tSpecification Version: " << prop.specVersion << "\n";
		ss << "\n";
	}
}
void prosper::debug::dump_limits(IPrContext &context, std::stringstream &ss)
{
	auto &dev = static_cast<VlkContext &>(context).GetDevice();
	auto &limits = dev.get_physical_device_properties().core_vk1_0_properties_ptr->limits;
	ss << "Buffer Image Granularity: " << limits.buffer_image_granularity << "\n";
	ss << "Discrete Queue Priorities: " << limits.discrete_queue_priorities << "\n";
	ss << "Framebuffer Color Sample Counts: " << static_cast<int32_t>(limits.framebuffer_color_sample_counts.get_vk()) << "\n";
	ss << "Framebuffer Depth Sample Counts: " << static_cast<int32_t>(limits.framebuffer_depth_sample_counts.get_vk()) << "\n";
	ss << "Framebuffer No Attachments Sample Counts: " << static_cast<int32_t>(limits.framebuffer_no_attachments_sample_counts.get_vk()) << "\n";
	ss << "Framebuffer Stencil Sample Counts: " << static_cast<int32_t>(limits.framebuffer_stencil_sample_counts.get_vk()) << "\n";
	ss << "Line Width Granularity: " << limits.line_width_granularity << "\n";
	ss << "Line Width Range: " << limits.line_width_range[0] << ", " << limits.line_width_range[1] << "\n";
	ss << "Max Bound Descriptor Sets: " << limits.max_bound_descriptor_sets << "\n";
	ss << "Max Clip Distances: " << limits.max_clip_distances << "\n";
	ss << "Max Color Attachments: " << limits.max_color_attachments << "\n";
	ss << "Max Combined Clip and Cull Distances: " << limits.max_combined_clip_and_cull_distances << "\n";
	ss << "Max Compute Shared Memory Size: " << limits.max_compute_shared_memory_size << "\n";
	ss << "Max Image Dimension 1D: " << limits.max_image_dimension_1D << "\n";
	ss << "Max Image Dimension 2D: " << limits.max_image_dimension_2D << "\n";
	ss << "Max Image Dimension 3D: " << limits.max_image_dimension_3D << "\n";
	ss << "Max Image Dimension Cube: " << limits.max_image_dimension_cube << "\n";
	ss << "Max Image Array Layers: " << limits.max_image_array_layers << "\n";
	ss << "Max Texel Buffer Elements: " << limits.max_texel_buffer_elements << "\n";
	ss << "Max Uniform Buffer Range: " << limits.max_uniform_buffer_range << "\n";
	ss << "Max Storage Buffer Range: " << limits.max_storage_buffer_range << "\n";
	ss << "Max Push Constants Size: " << limits.max_push_constants_size << "\n";
	ss << "Max Memory Allocation Count: " << limits.max_memory_allocation_count << "\n";
	ss << "Max Sampler Allocation Count: " << limits.max_sampler_allocation_count << "\n";
	ss << "Buffer Image Granularity: " << limits.buffer_image_granularity << "\n";
	ss << "Sparse Address Space Size: " << limits.sparse_address_space_size << "\n";
	ss << "Max Per Stage Descriptor Samples: " << limits.max_per_stage_descriptor_samplers << "\n";
	ss << "Max Per Stage Description Uniform Buffers: " << limits.max_per_stage_descriptor_uniform_buffers << "\n";
	ss << "Max Per Stage Descriptor Storage Buffers: " << limits.max_per_stage_descriptor_storage_buffers << "\n";
	ss << "Max Per Stage Descriptor Sampled Images: " << limits.max_per_stage_descriptor_sampled_images << "\n";
	ss << "Max Per Stage Descriptor Storage Images: " << limits.max_per_stage_descriptor_storage_images << "\n";
	ss << "Max Per Stage Descriptor Input Attachments: " << limits.max_per_stage_descriptor_input_attachments << "\n";
	ss << "Max Per Stage Resources: " << limits.max_per_stage_resources << "\n";
	ss << "Max Descriptor Set Samples: " << limits.max_descriptor_set_samplers << "\n";
	ss << "Max Descriptor Set Uniform Buffers: " << limits.max_descriptor_set_uniform_buffers << "\n";
	ss << "Max Descriptor Set Uniform Buffers Dynamic: " << limits.max_descriptor_set_uniform_buffers_dynamic << "\n";
	ss << "Max Descriptor Set Storage Buffers: " << limits.max_descriptor_set_storage_buffers << "\n";
	ss << "Max Descriptor Set Storage Buffers Dynamic: " << limits.max_descriptor_set_storage_buffers_dynamic << "\n";
	ss << "Max Descriptor Set Sampled Images: " << limits.max_descriptor_set_sampled_images << "\n";
	ss << "Max Descriptor Set Storage Images: " << limits.max_descriptor_set_storage_images << "\n";
	ss << "Max Descriptor Set Input Attachments: " << limits.max_descriptor_set_input_attachments << "\n";
	ss << "Max Vertex Input Attributes: " << limits.max_vertex_input_attributes << "\n";
	ss << "Max Vertex Input Bindings: " << limits.max_vertex_input_bindings << "\n";
	ss << "Max Vertex Input Attribute Offset: " << limits.max_vertex_input_attribute_offset << "\n";
	ss << "Max Vertex Input Binding Stride: " << limits.max_vertex_input_binding_stride << "\n";
	ss << "Max Vertex Output Components: " << limits.max_vertex_output_components << "\n";
	ss << "Max Tessellation Generation Level: " << limits.max_tessellation_generation_level << "\n";
	ss << "Max Tessellation Patch Size: " << limits.max_tessellation_patch_size << "\n";
	ss << "Max Tessellation Control Per Vertex Input Components: " << limits.max_tessellation_control_per_vertex_input_components << "\n";
	ss << "Max Tessellation Control Per Vertex Output Components: " << limits.max_tessellation_control_per_vertex_output_components << "\n";
	ss << "Max Tessellation Control Per Patch Output Components: " << limits.max_tessellation_control_per_patch_output_components << "\n";
	ss << "Max Tessellation Control Total Output Components: " << limits.max_tessellation_control_total_output_components << "\n";
	ss << "Max Tessellation Evaluation Input Components: " << limits.max_tessellation_evaluation_input_components << "\n";
	ss << "Max Tessellation Evaluation Output Components: " << limits.max_tessellation_evaluation_output_components << "\n";
	ss << "Max Geometry Shader Invocations: " << limits.max_geometry_shader_invocations << "\n";
	ss << "Max Geometry Input Components: " << limits.max_geometry_input_components << "\n";
	ss << "Max Geometry Output Components: " << limits.max_geometry_output_components << "\n";
	ss << "Max Geometry Output Vertices: " << limits.max_geometry_output_vertices << "\n";
	ss << "Max Geometry Total Output Components: " << limits.max_geometry_total_output_components << "\n";
	ss << "Max Fragment Input Components: " << limits.max_fragment_input_components << "\n";
	ss << "Max Fragment Output Attachments: " << limits.max_fragment_output_attachments << "\n";
	ss << "Max Fragment Dual Src Attachments: " << limits.max_fragment_dual_src_attachments << "\n";
	ss << "Max Fragment Combined Output Resources: " << limits.max_fragment_combined_output_resources << "\n";
	ss << "Max Compute Shared Memory Size: " << limits.max_compute_shared_memory_size << "\n";
	ss << "Max Compute Work Group Count: " << limits.max_compute_work_group_count[0] << ", " << limits.max_compute_work_group_count[1] << ", " << limits.max_compute_work_group_count[2] << "\n";
	ss << "Max Compute Work Group Invocations: " << limits.max_compute_work_group_invocations << "\n";
	ss << "Max Compute Work Group Size: " << limits.max_compute_work_group_size[0] << ", " << limits.max_compute_work_group_size[1] << ", " << limits.max_compute_work_group_size[2] << "\n";
	ss << "Sub Pixel Precision Bits: " << limits.sub_pixel_precision_bits << "\n";
	ss << "Sub Texel Precision Bits: " << limits.sub_texel_precision_bits << "\n";
	ss << "Mipmap Precision Bits: " << limits.mipmap_precision_bits << "\n";
	ss << "Max Draw Indexed Index Value: " << limits.max_draw_indexed_index_value << "\n";
	ss << "Max Draw Indirect Count: " << limits.max_draw_indirect_count << "\n";
	ss << "Max Sampler Lod Bias: " << limits.max_sampler_lod_bias << "\n";
	ss << "Max Sampler Anisotropy: " << limits.max_sampler_anisotropy << "\n";
	ss << "Max Viewports: " << limits.max_viewports << "\n";
	ss << "Max Viewport Dimensions: " << limits.max_viewport_dimensions[0] << ", " << limits.max_viewport_dimensions[1] << "\n";
	ss << "Viewport Bounds Range: " << limits.viewport_bounds_range[0] << ", " << limits.viewport_bounds_range[1] << "\n";
	ss << "Viewport Sub Pixel Bits: " << limits.viewport_sub_pixel_bits << "\n";
	ss << "Min Memory Map Alignment: " << limits.min_memory_map_alignment << "\n";
	ss << "Min Texel Buffer Offset Alignment: " << limits.min_texel_buffer_offset_alignment << "\n";
	ss << "Min Uniform Buffer Offset Alignment: " << limits.min_uniform_buffer_offset_alignment << "\n";
	ss << "Min Storage Buffer Offset Alignment: " << limits.min_storage_buffer_offset_alignment << "\n";
	ss << "Min Texel Offset: " << limits.min_texel_offset << "\n";
	ss << "Max Texel Offset: " << limits.max_texel_offset << "\n";
	ss << "Min Texel Gather Offset: " << limits.min_texel_gather_offset << "\n";
	ss << "Max Texel Gather Offset: " << limits.max_texel_gather_offset << "\n";
	ss << "Min Interpolation Offset: " << limits.min_interpolation_offset << "\n";
	ss << "Max Interpolation Offset: " << limits.max_interpolation_offset << "\n";
	ss << "Sub Pixel Interpolation Offset Bits: " << limits.sub_pixel_interpolation_offset_bits << "\n";
	ss << "Max Framebuffer Width: " << limits.max_framebuffer_width << "\n";
	ss << "Max Framebuffer Height: " << limits.max_framebuffer_height << "\n";
	ss << "Max Framebuffer Layers: " << limits.max_framebuffer_layers << "\n";
	ss << "Framebuffer Color Sample Counts: " << static_cast<int32_t>(limits.framebuffer_color_sample_counts.get_vk()) << "\n";
	ss << "Framebuffer Depth Sample Counts: " << static_cast<int32_t>(limits.framebuffer_depth_sample_counts.get_vk()) << "\n";
	ss << "Framebuffer Stencil Sample Counts: " << static_cast<int32_t>(limits.framebuffer_stencil_sample_counts.get_vk()) << "\n";
	ss << "Framebuffer No Attachments Sample Counts: " << static_cast<int32_t>(limits.framebuffer_no_attachments_sample_counts.get_vk()) << "\n";
	ss << "Max Color Attachments: " << limits.max_color_attachments << "\n";
	ss << "Sampled Image Color Sample Counts: " << static_cast<int32_t>(limits.sampled_image_color_sample_counts.get_vk()) << "\n";
	ss << "Sampled Image Integer Sample Counts: " << static_cast<int32_t>(limits.sampled_image_integer_sample_counts.get_vk()) << "\n";
	ss << "Sampled Image Depth Sample Counts: " << static_cast<int32_t>(limits.sampled_image_depth_sample_counts.get_vk()) << "\n";
	ss << "Sampled Image Stencil Sample Counts: " << static_cast<int32_t>(limits.sampled_image_stencil_sample_counts.get_vk()) << "\n";
	ss << "Storage Image Sample Counts: " << static_cast<int32_t>(limits.storage_image_sample_counts.get_vk()) << "\n";
	ss << "Max Sample Mask Words: " << limits.max_sample_mask_words << "\n";
	ss << "Timestamp Compute and Graphics: " << limits.timestamp_compute_and_graphics << "\n";
	ss << "Timestamp Period: " << limits.timestamp_period << "\n";
	ss << "Max Clip Distances: " << limits.max_clip_distances << "\n";
	ss << "Max Cull Distances: " << limits.max_cull_distances << "\n";
	ss << "Max Combined Clip and Cull Distances: " << limits.max_combined_clip_and_cull_distances << "\n";
	ss << "Discrete Queue Priorities: " << limits.discrete_queue_priorities << "\n";
	ss << "Point Size Range: " << limits.point_size_range[0] << ", " << limits.point_size_range[1] << "\n";
	ss << "Line Width Range: " << limits.line_width_range[0] << ", " << limits.line_width_range[1] << "\n";
	ss << "Point Size Granularity: " << limits.point_size_granularity << "\n";
	ss << "Line Width Granularity: " << limits.line_width_granularity << "\n";
	ss << "Strict Lines: " << limits.strict_lines << "\n";
	ss << "Standard Sample Locations: " << limits.standard_sample_locations << "\n";
	ss << "Optimal Buffer Copy Offset Alignment: " << limits.optimal_buffer_copy_offset_alignment << "\n";
	ss << "Optimal Buffer Copy Row Pitch Alignment: " << limits.optimal_buffer_copy_row_pitch_alignment << "\n";
	ss << "Non-coherent Atom Size: " << limits.non_coherent_atom_size;
}
void prosper::debug::dump_features(IPrContext &context, std::stringstream &ss)
{
	auto &dev = static_cast<VlkContext &>(context).GetDevice();
	auto &features = dev.get_physical_device_features();
	auto &devFeatures = *features.core_vk1_0_features_ptr;
	ss << "Robust Buffer Access: " << devFeatures.robust_buffer_access << "\n";
	ss << "Full Draw Index Uint32: " << devFeatures.full_draw_index_uint32 << "\n";
	ss << "Image Cube Array: " << devFeatures.image_cube_array << "\n";
	ss << "Independent Blend: " << devFeatures.independent_blend << "\n";
	ss << "Geometry Shader: " << devFeatures.geometry_shader << "\n";
	ss << "Tessellation Shader: " << devFeatures.tessellation_shader << "\n";
	ss << "Sample Rate Shading: " << devFeatures.sample_rate_shading << "\n";
	ss << "Dual Src Blend: " << devFeatures.dual_src_blend << "\n";
	ss << "Logic Op: " << devFeatures.logic_op << "\n";
	ss << "Multi Draw Indirect: " << devFeatures.multi_draw_indirect << "\n";
	ss << "Draw Indirect First Instance: " << devFeatures.draw_indirect_first_instance << "\n";
	ss << "Depth Clamp: " << devFeatures.depth_clamp << "\n";
	ss << "Depth Bias Clamp: " << devFeatures.depth_bias_clamp << "\n";
	ss << "Fill Mode Non Solid: " << devFeatures.fill_mode_non_solid << "\n";
	ss << "Depth Bounds: " << devFeatures.depth_bounds << "\n";
	ss << "Wide Lines: " << devFeatures.wide_lines << "\n";
	ss << "Large Points: " << devFeatures.large_points << "\n";
	ss << "Alpha To One: " << devFeatures.alpha_to_one << "\n";
	ss << "Multi Viewport: " << devFeatures.multi_viewport << "\n";
	ss << "Sampler Anisotropy: " << devFeatures.sampler_anisotropy << "\n";
	ss << "Texture Compression ETC2: " << devFeatures.texture_compression_ETC2 << "\n";
	ss << "Texture Compression ASTC_LDR: " << devFeatures.texture_compression_ASTC_LDR << "\n";
	ss << "Texture Compression BC: " << devFeatures.texture_compression_BC << "\n";
	ss << "Occlusion Query Precise: " << devFeatures.occlusion_query_precise << "\n";
	ss << "Pipeline Statistics Query: " << devFeatures.pipeline_statistics_query << "\n";
	ss << "Vertex Pipeline Stores and Atomics: " << devFeatures.vertex_pipeline_stores_and_atomics << "\n";
	ss << "Fragment Stores and Atomics: " << devFeatures.fragment_stores_and_atomics << "\n";
	ss << "Shader Tessellation and Geometry Point Size: " << devFeatures.shader_tessellation_and_geometry_point_size << "\n";
	ss << "Shader Image Gather Extended: " << devFeatures.shader_image_gather_extended << "\n";
	ss << "Shader Storage Image Extended Formats: " << devFeatures.shader_storage_image_extended_formats << "\n";
	ss << "Shader Storage Image Multisample: " << devFeatures.shader_storage_image_multisample << "\n";
	ss << "Shader Storage Image Read without Format: " << devFeatures.shader_storage_image_read_without_format << "\n";
	ss << "Shader Storage Image Write without Format: " << devFeatures.shader_storage_image_write_without_format << "\n";
	ss << "Shader Uniform Buffer Array Dynamic Indexing: " << devFeatures.shader_uniform_buffer_array_dynamic_indexing << "\n";
	ss << "Shader Sampled Image Array Dynamic Indexing: " << devFeatures.shader_sampled_image_array_dynamic_indexing << "\n";
	ss << "Shader Storage Buffer Array Dynamic Indexing: " << devFeatures.shader_storage_buffer_array_dynamic_indexing << "\n";
	ss << "Shader Storage Image Array Dynamic Indexing: " << devFeatures.shader_storage_image_array_dynamic_indexing << "\n";
	ss << "Shader Clip Distance: " << devFeatures.shader_clip_distance << "\n";
	ss << "Shader Cull Distance: " << devFeatures.shader_cull_distance << "\n";
	ss << "Shader Float64: " << devFeatures.shader_float64 << "\n";
	ss << "Shader Int64: " << devFeatures.shader_int64 << "\n";
	ss << "Shader Int16: " << devFeatures.shader_int16 << "\n";
	ss << "Shader Resource Residency: " << devFeatures.shader_resource_residency << "\n";
	ss << "Shader Resource Min Lod: " << devFeatures.shader_resource_min_lod << "\n";
	ss << "Sparse Binding: " << devFeatures.sparse_binding << "\n";
	ss << "Sparse Residency Buffer: " << devFeatures.sparse_residency_buffer << "\n";
	ss << "Sparse Residency Image 2D: " << devFeatures.sparse_residency_image_2D << "\n";
	ss << "Sparse Residency Image 3D: " << devFeatures.sparse_residency_image_3D << "\n";
	ss << "Sparse Residency Image 2 Samples: " << devFeatures.sparse_residency_2_samples << "\n";
	ss << "Sparse Residency Image 4 Samples: " << devFeatures.sparse_residency_4_samples << "\n";
	ss << "Sparse Residency Image 8 Samples: " << devFeatures.sparse_residency_8_samples << "\n";
	ss << "Sparse Residency Image 16 Samples: " << devFeatures.sparse_residency_16_samples << "\n";
	ss << "Sparse Residency Aliased: " << devFeatures.sparse_residency_aliased << "\n";
	ss << "Variable Multisample Rate: " << devFeatures.variable_multisample_rate << "\n";
	ss << "Inherited Queries: " << devFeatures.inherited_queries;
}

void prosper::debug::dump_image_format_properties(IPrContext &context, std::stringstream &ss, prosper::ImageCreateFlags createFlags)
{
	struct FormatPropertyInfo {
		Anvil::ImageFormatProperties formatProperties;
		Anvil::ImageUsageFlags usageFlags = {};
	};
	ss << "Create Flags: ";
	if(createFlags == prosper::ImageCreateFlags::None)
		ss << "-";
	else
		ss << prosper::util::to_string(createFlags);
	for(auto format = Anvil::Format::UNKNOWN; format != static_cast<decltype(format)>(pragma::math::to_integral(Anvil::Format::ASTC_12x12_SRGB_BLOCK) + 1); format = static_cast<decltype(format)>(pragma::math::to_integral(format) + 1)) {
		ss << "\n" << prosper::util::to_string(static_cast<prosper::Format>(format));
		for(auto type = Anvil::ImageType::_1D; type != static_cast<decltype(type)>(pragma::math::to_integral(Anvil::ImageType::_3D) + 1); type = static_cast<decltype(type)>(pragma::math::to_integral(type) + 1)) {
			ss << "\n\t" << prosper::util::to_string(static_cast<prosper::ImageType>(type));
			for(auto tiling = Anvil::ImageTiling::OPTIMAL; tiling != static_cast<decltype(tiling)>(pragma::math::to_integral(Anvil::ImageTiling::LINEAR) + 1); tiling = static_cast<decltype(tiling)>(pragma::math::to_integral(tiling) + 1)) {
				std::vector<FormatPropertyInfo> propertyList {};
				ss << "\n\t\t" << prosper::util::to_string(static_cast<prosper::ImageTiling>(tiling));
				for(auto usage = Anvil::ImageUsageFlagBits::TRANSFER_SRC_BIT; usage != static_cast<decltype(usage)>(pragma::math::to_integral(Anvil::ImageUsageFlagBits::INPUT_ATTACHMENT_BIT) >> 1); usage = static_cast<decltype(usage)>(pragma::math::to_integral(usage) << 1)) {
					try {
						Anvil::ImageFormatProperties properties;
						Anvil::ImageFormatPropertiesQuery query {format, type, tiling, usage, static_cast<Anvil::ImageCreateFlagBits>(createFlags)};
						static_cast<VlkContext &>(context).GetDevice().get_physical_device_image_format_properties(query, &properties);
						FormatPropertyInfo *found = nullptr;
						for(auto &info : propertyList) {
							auto &propsOther = info.formatProperties;
							auto &ext = propsOther.max_extent;
							if(propsOther.n_max_array_layers == properties.n_max_array_layers && ext.depth == properties.max_extent.depth && ext.height == properties.max_extent.height && ext.width == properties.max_extent.width && propsOther.n_max_mip_levels == properties.n_max_mip_levels
							  && propsOther.max_resource_size == properties.max_resource_size && propsOther.sample_counts == properties.sample_counts) {
								found = &info;
								break;
							}
						}
						if(found == nullptr) {
							propertyList.push_back(FormatPropertyInfo {});
							found = &propertyList.back();
							found->formatProperties = properties;
						}
						found->usageFlags |= usage;
					}
					catch(const std::system_error &err) {
						ss << "\n\t\t\t" << prosper::util::to_string(static_cast<prosper::ImageUsageFlags>(usage));
						if(static_cast<vk::Result>(err.code().value()) == vk::Result::eErrorFormatNotSupported) {
							ss << "\n\t\t\t\tUNSUPPORTED";
						}
						else {
							ss << "\n\t\t\t\tUnexpected Error: " << err.what();
						}
					}
				}
				for(auto &info : propertyList) {
					ss << "\n\t\t\t" << prosper::util::to_string(static_cast<prosper::ImageUsageFlags>(info.usageFlags.get_vk()));
					auto &properties = info.formatProperties;

					const auto t = "\t\t\t\t";
					ss << "\n" << t << "Max Array Layers: " << properties.n_max_array_layers;
					ss << "\n" << t << "Max Extent:";
					ss << "\n" << t << "\tDepth: " << properties.max_extent.depth;
					ss << "\n" << t << "\tHeight: " << properties.max_extent.height;
					ss << "\n" << t << "\tWidth: " << properties.max_extent.width;
					ss << "\n" << t << "Max Mip Levels: " << properties.n_max_mip_levels;
					ss << "\n" << t << "Max Resource Size: " << properties.max_resource_size;
					ss << "\n" << t << "Sample Counts: ";
					auto values = pragma::math::get_power_of_2_values(static_cast<uint64_t>(static_cast<uint32_t>(properties.sample_counts.get_vk())));
					if(values.empty())
						ss << "-";
					else {
						auto numValues = values.size();
						for(auto i = decltype(numValues) {0}; i < numValues; ++i) {
							ss << vk::to_string(static_cast<vk::SampleCountFlagBits>(values[i]));
							if(i < numValues - 1)
								ss << " | ";
						}
					}
				}
			}
		}
	}
}
void prosper::debug::dump_format_properties(IPrContext &context, std::stringstream &ss)
{
	for(auto i = Anvil::Format::UNKNOWN; i < Anvil::Format::ASTC_12x12_SRGB_BLOCK; i = static_cast<Anvil::Format>(static_cast<uint32_t>(i) + 1)) {
		auto props = static_cast<VlkContext &>(context).GetDevice().get_physical_device_format_properties(i);
		ss << prosper::util::to_string(static_cast<prosper::Format>(i)) << ":\n";
		std::map<std::string, const Anvil::FormatFeatureFlags &> formatFeatures = {{"Buffer", props.buffer_capabilities}, {"Linear Tiling", props.linear_tiling_capabilities}, {"Optimal Tiling", props.optimal_tiling_capabilities}};
		for(auto it = formatFeatures.begin(); it != formatFeatures.end(); ++it) {
			ss << "\t" << it->first << ": ";
			auto &features = it->second;
			auto bFirst = true;
			for(auto feature = Anvil::FormatFeatureFlagBits::SAMPLED_IMAGE_BIT; feature < Anvil::FormatFeatureFlagBits::SAMPLED_IMAGE_FILTER_LINEAR_BIT; feature = static_cast<Anvil::FormatFeatureFlagBits>(static_cast<uint32_t>(feature) * 2)) {
				if((features & feature) != Anvil::FormatFeatureFlagBits(0)) {
					if(bFirst == false)
						ss << " | ";
					else
						bFirst = false;
					ss << vk::to_string(static_cast<vk::FormatFeatureFlags>(Anvil::FormatFeatureFlags {feature}.get_vk()));
				}
			}
			ss << "\n";
		}
	}
}

void prosper::debug::dump_layers(IPrContext &context, const std::string &fileName)
{
	auto f = pragma::fs::open_file<pragma::fs::VFilePtrReal>(fileName, pragma::fs::FileMode::Write);
	if(f == nullptr)
		return;
	std::stringstream ss;
	dump_layers(context, ss);
	f->WriteString(ss.str());
	f = nullptr;
}
void prosper::debug::dump_extensions(IPrContext &context, const std::string &fileName)
{
	auto f = pragma::fs::open_file<pragma::fs::VFilePtrReal>(fileName, pragma::fs::FileMode::Write);
	if(f == nullptr)
		return;
	std::stringstream ss;
	dump_extensions(context, ss);
	f->WriteString(ss.str());
	f = nullptr;
}

void prosper::debug::dump_limits(IPrContext &context, const std::string &fileName)
{
	auto f = pragma::fs::open_file<pragma::fs::VFilePtrReal>(fileName, pragma::fs::FileMode::Write);
	if(f == nullptr)
		return;
	std::stringstream ss;
	dump_limits(context, ss);
	f->WriteString(ss.str());
	f = nullptr;
}

void prosper::debug::dump_features(IPrContext &context, const std::string &fileName)
{
	auto f = pragma::fs::open_file<pragma::fs::VFilePtrReal>(fileName, pragma::fs::FileMode::Write);
	if(f == nullptr)
		return;
	std::stringstream ss;
	dump_features(context, ss);
	f->WriteString(ss.str());
	f = nullptr;
}

void prosper::debug::dump_image_format_properties(IPrContext &context, const std::string &fileName, prosper::ImageCreateFlags createFlags)
{
	auto f = pragma::fs::open_file<pragma::fs::VFilePtrReal>(fileName, pragma::fs::FileMode::Write);
	if(f == nullptr)
		return;
	std::stringstream ss;
	dump_image_format_properties(context, ss, createFlags);
	f->WriteString(ss.str());
	f = nullptr;
}

void prosper::debug::dump_format_properties(IPrContext &context, const std::string &fileName)
{
	auto f = pragma::fs::open_file<pragma::fs::VFilePtrReal>(fileName, pragma::fs::FileMode::Write);
	if(f == nullptr)
		return;
	std::stringstream ss;
	dump_format_properties(context, ss);
	f->WriteString(ss.str());
	f = nullptr;
}

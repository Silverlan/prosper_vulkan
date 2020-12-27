/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_context.hpp"
#include <misc/types.h>
#include "vk_fence.hpp"
#include "vk_command_buffer.hpp"
#include "vk_render_pass.hpp"
#include "buffers/vk_buffer.hpp"
#include "buffers/vk_render_buffer.hpp"
#include "vk_descriptor_set_group.hpp"
#include "vk_pipeline_cache.hpp"
#include "vk_event.hpp"
#include "vk_framebuffer.hpp"
#include "shader/prosper_shader.hpp"
#include "shader/prosper_pipeline_create_info.hpp"
#include "image/vk_image.hpp"
#include "image/vk_image_view.hpp"
#include "image/vk_sampler.hpp"
#include "buffers/vk_uniform_resizable_buffer.hpp"
#include "buffers/vk_dynamic_resizable_buffer.hpp"
#include "queries/prosper_query_pool.hpp"
#include "queries/prosper_pipeline_statistics_query.hpp"
#include "queries/prosper_timestamp_query.hpp"
#include "queries/vk_query_pool.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include "vk_util.hpp"
#include <prosper_glsl.hpp>
#include <util_image_buffer.hpp>
#include <misc/buffer_create_info.h>
#include <misc/fence_create_info.h>
#include <misc/semaphore_create_info.h>
#include <misc/framebuffer_create_info.h>
#include <misc/swapchain_create_info.h>
#include <misc/rendering_surface_create_info.h>
#include <misc/compute_pipeline_create_info.h>
#include <misc/graphics_pipeline_create_info.h>
#include <misc/render_pass_create_info.h>
#include <misc/image_create_info.h>
#include <misc/window_factory.h>
#include <misc/window.h>
#include <wrappers/fence.h>
#include <wrappers/device.h>
#include <wrappers/queue.h>
#include <wrappers/instance.h>
#include <wrappers/swapchain.h>
#include <wrappers/render_pass.h>
#include <wrappers/command_buffer.h>
#include <wrappers/command_pool.h>
#include <wrappers/framebuffer.h>
#include <wrappers/semaphore.h>
#include <wrappers/descriptor_set_group.h>
#include <wrappers/graphics_pipeline_manager.h>
#include <wrappers/compute_pipeline_manager.h>
#include <wrappers/query_pool.h>
#include <wrappers/shader_module.h>
#include <misc/image_view_create_info.h>
#include <iglfw/glfw_window.h>

#include <string>
#include <cmath>
#include <iglfw/glfw_window.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3native.h>

#ifdef __linux__
#define ENABLE_GLFW_ANVIL_COMPATIBILITY
#endif
#ifdef ENABLE_GLFW_ANVIL_COMPATIBILITY
#include <xcb/xcb.h>
#include <X11/Xlib-xcb.h>
#endif

/* Sanity checks */
#if defined(_WIN32)
#if !defined(ANVIL_INCLUDE_WIN3264_WINDOW_SYSTEM_SUPPORT) && !defined(ENABLE_OFFSCREEN_RENDERING)
#error Anvil has not been built with Win32/64 window system support. The application can only be built in offscreen rendering mode.
#endif
#else
#if !defined(ANVIL_INCLUDE_XCB_WINDOW_SYSTEM_SUPPORT) && !defined(ENABLE_OFFSCREEN_RENDERING)
#error Anvil has not been built with XCB window system support. The application can only be built in offscreen rendering mode.
#endif
#endif

using namespace prosper;

static_assert(sizeof(prosper::util::BufferCopy) == sizeof(Anvil::BufferCopy));
static_assert(sizeof(prosper::Extent2D) == sizeof(vk::Extent2D));

#pragma optimize("",off)
VlkShaderStageProgram::VlkShaderStageProgram(std::vector<unsigned int> &&spirvBlob)
	: m_spirvBlob{std::move(spirvBlob)}
{}

const std::vector<unsigned int> &VlkShaderStageProgram::GetSPIRVBlob() const {return m_spirvBlob;}

/////////////

decltype(VlkContext::s_devToContext) VlkContext::s_devToContext = {};

VlkContext::VlkContext(const std::string &appName,bool bEnableValidation)
	: IPrContext{appName,bEnableValidation}
{}

void VlkContext::Flush() {}

prosper::Result VlkContext::WaitForFence(const IFence &fence,uint64_t timeout) const
{
	auto vkFence = static_cast<const VlkFence&>(fence).GetAnvilFence().get_fence();
	return static_cast<prosper::Result>(vkWaitForFences(m_devicePtr->get_device_vk(),1,&vkFence,true,timeout));
}

prosper::Result VlkContext::WaitForFences(const std::vector<IFence*> &fences,bool waitAll,uint64_t timeout) const
{
	std::vector<VkFence> vkFences {};
	vkFences.reserve(fences.size());
	for(auto &fence : fences)
		vkFences.push_back(static_cast<VlkFence&>(*fence).GetAnvilFence().get_fence());
	return static_cast<prosper::Result>(vkWaitForFences(m_devicePtr->get_device_vk(),vkFences.size(),vkFences.data(),waitAll,timeout));
}

bool VlkContext::WaitForCurrentSwapchainCommandBuffer(std::string &outErrMsg)
{
	auto waitResult = static_cast<prosper::Result>(vkWaitForFences(m_devicePtr->get_device_vk(),1,m_cmdFences[m_n_swapchain_image]->get_fence_ptr(),true,std::numeric_limits<uint64_t>::max()));
	if(waitResult == prosper::Result::Success)
	{
		if(m_cmdFences[m_n_swapchain_image]->reset() == false)
		{
			outErrMsg = "Unable to reset swapchain fence!";
			return false;
		}
	}
	else
	{
		outErrMsg = "An error has occurred when waiting for swapchain fence: " +prosper::util::to_string(waitResult);
		return false;
	}
	return true;
}

#define ENABLE_DEBUG_TIMESTAMPS 0
#if ENABLE_DEBUG_TIMESTAMPS == 1
static long double get_debug_delta_time(std::chrono::high_resolution_clock::time_point t) {return ((std::chrono::high_resolution_clock::now() -t).count() /1'000'000.0);}
static bool g_dbgPrintTs = false;
#endif
void VlkContext::DrawFrame(const std::function<void(const std::shared_ptr<prosper::IPrimaryCommandBuffer>&,uint32_t)> &drawFrame)
{
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Timing start..."<<std::endl;
	auto t0 = std::chrono::high_resolution_clock::now();
#endif
	Anvil::Semaphore *curr_frame_signal_semaphore_ptr;
	Anvil::Semaphore *curr_frame_wait_semaphore_ptr;
	static uint32_t n_frames_rendered = 0;
	auto *present_queue_ptr = m_devicePtr->get_universal_queue(0);
	Anvil::PipelineStageFlags wait_stage_mask = Anvil::PipelineStageFlagBits::ALL_COMMANDS_BIT;

	/* Determine the signal + wait semaphores to use for drawing this frame */
	m_lastSemaporeUsed = (m_lastSemaporeUsed +1) %m_numSwapchainImages;

	curr_frame_signal_semaphore_ptr = m_frameSignalSemaphores[m_lastSemaporeUsed].get();
	curr_frame_wait_semaphore_ptr = m_frameWaitSemaphores[m_lastSemaporeUsed].get();
	
#if ENABLE_DEBUG_TIMESTAMPS == 1
	auto tAcquireImage = std::chrono::high_resolution_clock::now();
#endif
	/* Determine the semaphore which the swapchain image */
	auto errCode = m_swapchainPtr->acquire_image(curr_frame_wait_semaphore_ptr,&m_n_swapchain_image);
	if(errCode == Anvil::SwapchainOperationErrorCode::OUT_OF_DATE)
	{
		InitSwapchain();
		return;
	}
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Acquire image time: "<<get_debug_delta_time(tAcquireImage)<<"ms"<<std::endl;
	//g_dbgPrintTs = false;
#endif
	
#if ENABLE_DEBUG_TIMESTAMPS == 1
	auto tWaitCurrentCmdBuf = std::chrono::high_resolution_clock::now();
#endif
	auto success = (errCode == Anvil::SwapchainOperationErrorCode::SUCCESS);
	std::string errMsg;
	if(success)
		success = WaitForCurrentSwapchainCommandBuffer(errMsg);
	else
		errMsg = "Unable to acquire next swapchain image: " +std::to_string(umath::to_integral(errCode));
	if(errMsg.empty() == false)
		throw std::runtime_error(errMsg);
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Wait for current command buffer time: "<<get_debug_delta_time(tWaitCurrentCmdBuf)<<"ms"<<std::endl;
	//g_dbgPrintTs = false;
#endif
	
#if ENABLE_DEBUG_TIMESTAMPS == 1
	auto tClearResources = std::chrono::high_resolution_clock::now();
#endif
	ClearKeepAliveResources();
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Resource clear time: "<<get_debug_delta_time(tClearResources)<<"ms"<<std::endl;
	//g_dbgPrintTs = false;
#endif

	//auto &keepAliveResources = m_keepAliveResources.at(m_n_swapchain_image);
	//auto numKeepAliveResources = keepAliveResources.size(); // We can clear the resources from the previous render pass of this swapchain after we've waited for the semaphore (i.e. after the frame rendering is complete)
	
#if ENABLE_DEBUG_TIMESTAMPS == 1
	auto tBufUpdate = std::chrono::high_resolution_clock::now();
#endif
	auto &cmd_buffer_ptr = m_commandBuffers.at(m_n_swapchain_image);
	/* Start recording commands */
	auto &primCmd = static_cast<prosper::VlkPrimaryCommandBuffer&>(*cmd_buffer_ptr);
	static_cast<Anvil::PrimaryCommandBuffer&>(primCmd.GetAnvilCommandBuffer()).start_recording(false,true);
	primCmd.SetRecording(true);
	umath::set_flag(m_stateFlags,StateFlags::IsRecording);
	umath::set_flag(m_stateFlags,StateFlags::Idle,false);
	while(m_scheduledBufferUpdates.empty() == false)
	{
		auto &f = m_scheduledBufferUpdates.front();
		f(*cmd_buffer_ptr);
		m_scheduledBufferUpdates.pop();
	}
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Buffer update time: "<<get_debug_delta_time(tBufUpdate)<<"ms"<<std::endl;
#endif
	drawFrame(GetDrawCommandBuffer(),m_n_swapchain_image);
	/* Close the recording process */
	umath::set_flag(m_stateFlags,StateFlags::IsRecording,false);
	primCmd.SetRecording(false);
	static_cast<Anvil::PrimaryCommandBuffer&>(primCmd.GetAnvilCommandBuffer()).stop_recording();

	/* Submit work chunk and present */
	auto *signalSemaphore = curr_frame_signal_semaphore_ptr;
	auto *waitSemaphore = curr_frame_wait_semaphore_ptr;
#if ENABLE_DEBUG_TIMESTAMPS == 1
	auto tSubmit = std::chrono::high_resolution_clock::now();
#endif
	m_devicePtr->get_universal_queue(0)->submit(Anvil::SubmitInfo::create(
		&static_cast<prosper::VlkPrimaryCommandBuffer&>(*m_commandBuffers[m_n_swapchain_image]).GetAnvilCommandBuffer(),
		1, /* n_semaphores_to_signal */
		&signalSemaphore,
		1, /* n_semaphores_to_wait_on */
		&waitSemaphore,
		&wait_stage_mask,
		false, /* should_block  */
		m_cmdFences.at(m_n_swapchain_image).get()
	)); /* opt_fence_ptr */
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Submit time: "<<get_debug_delta_time(tSubmit)<<"ms"<<std::endl;
#endif

		//ClearKeepAliveResources(numKeepAliveResources);
	
#if ENABLE_DEBUG_TIMESTAMPS == 1
	auto tPresent = std::chrono::high_resolution_clock::now();
#endif
	auto bPresentSuccess = present_queue_ptr->present(
		m_swapchainPtr.get(),
		m_n_swapchain_image,
		1, /* n_wait_semaphores */
		&signalSemaphore,
		&errCode
	);
	
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Present time: "<<get_debug_delta_time(tPresent)<<"ms"<<std::endl;
#endif
	if(errCode != Anvil::SwapchainOperationErrorCode::SUCCESS || bPresentSuccess == false)
	{
		if(errCode == Anvil::SwapchainOperationErrorCode::OUT_OF_DATE)
		{
			InitSwapchain();
			return;
		}
		else
			; // Terminal error?
	}
	//m_glfwWindow->SwapBuffers();
#if ENABLE_DEBUG_TIMESTAMPS == 1
	if(g_dbgPrintTs)
		std::cout<<"Frame time: "<<get_debug_delta_time(t0)<<"ms"<<std::endl;
	//g_dbgPrintTs = false;
#endif
}

bool VlkContext::Submit(ICommandBuffer &cmdBuf,bool shouldBlock,IFence *optFence)
{
	auto &dev = GetDevice();
	return dev.get_universal_queue(0)->submit(Anvil::SubmitInfo::create(
		&cmdBuf.GetAPITypeRef<VlkCommandBuffer>().GetAnvilCommandBuffer(),0u,nullptr,
		0u,nullptr,nullptr,shouldBlock,optFence ? &dynamic_cast<VlkFence*>(optFence)->GetAnvilFence() : nullptr
	));
}

bool VlkContext::GetSurfaceCapabilities(Anvil::SurfaceCapabilities &caps) const
{
	return const_cast<VlkContext*>(this)->GetDevice().get_physical_device_surface_capabilities(m_renderingSurfacePtr.get(),&caps);
}

void VlkContext::ReleaseSwapchain()
{
	m_renderPass.reset();
	m_swapchainPtr.reset();
	m_renderingSurfacePtr.reset();
	for(auto &fbo : m_fbos)
		fbo.reset();

	m_commandBuffers.clear();
	m_cmdFences.clear();
	m_fbos.clear();
	m_frameSignalSemaphores.clear();
	m_frameWaitSemaphores.clear();
}

void VlkContext::ReloadWindow()
{
	WaitIdle();
	ReleaseSwapchain();
	InitWindow();
	ReloadSwapchain();
}

const Anvil::Instance &VlkContext::GetAnvilInstance() const {return const_cast<VlkContext*>(this)->GetAnvilInstance();}
Anvil::Instance &VlkContext::GetAnvilInstance() {return *m_instancePtr;}

std::shared_ptr<VlkContext> VlkContext::Create(const std::string &appName,bool bEnableValidation)
{
	return std::shared_ptr<VlkContext>{new VlkContext{appName,bEnableValidation}};
}

IPrContext &VlkContext::GetContext(Anvil::BaseDevice &dev)
{
	auto it = s_devToContext.find(&dev);
	if(it == s_devToContext.end())
		throw std::runtime_error("Invalid context for device!");
	return *it->second;
}

void VlkContext::Release()
{
	if(m_devicePtr == nullptr)
		return;
	ReleaseSwapchain();

	IPrContext::Release();

	auto &dev = GetDevice();
	auto it = s_devToContext.find(&dev);
	if(it != s_devToContext.end())
		s_devToContext.erase(it);

	m_devicePtr.reset(); // All Vulkan resources related to the device have to be cleared at this point!
	vkDestroySurfaceKHR(m_instancePtr->get_instance_vk(),m_surface,nullptr);
	m_instancePtr.reset();
	m_windowPtr.reset();
}

Anvil::SGPUDevice &VlkContext::GetDevice() {return *m_pGpuDevice;}
const Anvil::SGPUDevice &VlkContext::GetDevice() const {return const_cast<VlkContext*>(this)->GetDevice();}
const std::shared_ptr<Anvil::RenderPass> &VlkContext::GetMainRenderPass() const {return m_renderPass;}
Anvil::SubPassID VlkContext::GetMainSubPassID() const {return m_mainSubPass;}
std::shared_ptr<Anvil::Swapchain> VlkContext::GetSwapchain() {return m_swapchainPtr;}

void VlkContext::DoWaitIdle()
{
	auto &dev = GetDevice();
	dev.wait_idle();
}

void VlkContext::DoFlushCommandBuffer(ICommandBuffer &cmd)
{
	if(cmd.IsPrimary() == false)
		return;
	auto &pcmd = static_cast<prosper::VlkPrimaryCommandBuffer&>(cmd.GetAPITypeRef<prosper::VlkCommandBuffer>());
	auto bSuccess = static_cast<Anvil::PrimaryCommandBuffer&>(pcmd.GetAnvilCommandBuffer()).stop_recording();
	auto &dev = GetDevice();
	dev.get_universal_queue(0)->submit(Anvil::SubmitInfo::create(
		&pcmd.GetAnvilCommandBuffer(),0u,nullptr,
		0u,nullptr,nullptr,true
	));
}


bool VlkContext::IsImageFormatSupported(prosper::Format format,prosper::ImageUsageFlags usageFlags,prosper::ImageType type,prosper::ImageTiling tiling) const
{
	return m_devicePtr->get_physical_device_image_format_properties(
		Anvil::ImageFormatPropertiesQuery{
			static_cast<Anvil::Format>(format),static_cast<Anvil::ImageType>(type),static_cast<Anvil::ImageTiling>(tiling),
			static_cast<Anvil::ImageUsageFlagBits>(usageFlags),{}
		}
	);
}


uint32_t VlkContext::GetUniversalQueueFamilyIndex() const
{
	auto &dev = GetDevice();
	auto *queuePtr = dev.get_universal_queue(0);
	return queuePtr->get_queue_family_index();
}

prosper::util::Limits VlkContext::GetPhysicalDeviceLimits() const
{
	auto &vkLimits = GetDevice().get_physical_device_properties().core_vk1_0_properties_ptr->limits;
	util::Limits limits {};
	limits.maxSamplerAnisotropy = vkLimits.max_sampler_anisotropy;
	limits.maxStorageBufferRange = vkLimits.max_storage_buffer_range;
	limits.maxImageArrayLayers = vkLimits.max_image_array_layers;

	Anvil::SurfaceCapabilities surfCapabilities {};
	if(GetSurfaceCapabilities(surfCapabilities))
		limits.maxSurfaceImageCount = surfCapabilities.max_image_count;
	return limits;
}

bool VlkContext::ClearPipeline(bool graphicsShader,PipelineID pipelineId)
{
	auto &dev = static_cast<VlkContext&>(*this).GetDevice();
	if(graphicsShader)
		return dev.get_graphics_pipeline_manager()->delete_pipeline(pipelineId);
	return dev.get_compute_pipeline_manager()->delete_pipeline(pipelineId);
}

std::optional<prosper::util::PhysicalDeviceImageFormatProperties> VlkContext::GetPhysicalDeviceImageFormatProperties(const ImageFormatPropertiesQuery &query)
{
	auto &dev = GetDevice();
	Anvil::ImageFormatProperties imgFormatProperties {};
	if(dev.get_physical_device_image_format_properties(
		Anvil::ImageFormatPropertiesQuery{
			static_cast<Anvil::Format>(query.format),static_cast<Anvil::ImageType>(query.imageType),static_cast<Anvil::ImageTiling>(query.tiling),
			static_cast<Anvil::ImageUsageFlagBits>(query.usageFlags),
		{}
		},&imgFormatProperties
	) == false
		)
		return {};
	util::PhysicalDeviceImageFormatProperties imageFormatProperties {};
	imageFormatProperties.sampleCount = static_cast<SampleCountFlags>(imgFormatProperties.sample_counts.get_vk());
	return imageFormatProperties;
}

const std::string PIPELINE_CACHE_PATH = "cache/shader.cache";
bool VlkContext::SavePipelineCache()
{
	auto *pPipelineCache = m_devicePtr->get_pipeline_cache();
	if(pPipelineCache == nullptr)
		return false;
	return PipelineCache::Save(*pPipelineCache,PIPELINE_CACHE_PATH);
}

std::shared_ptr<prosper::IPrimaryCommandBuffer> VlkContext::AllocatePrimaryLevelCommandBuffer(prosper::QueueFamilyType queueFamilyType,uint32_t &universalQueueFamilyIndex)
{
	if(GetUniversalQueueFamilyIndex(queueFamilyType,universalQueueFamilyIndex) == false)
	{
		if(queueFamilyType != prosper::QueueFamilyType::Universal)
			return AllocatePrimaryLevelCommandBuffer(prosper::QueueFamilyType::Universal,universalQueueFamilyIndex);
		return nullptr;
	}
	return prosper::VlkPrimaryCommandBuffer::Create(*this,GetDevice().get_command_pool_for_queue_family_index(universalQueueFamilyIndex)->alloc_primary_level_command_buffer(),queueFamilyType);
}
std::shared_ptr<prosper::ISecondaryCommandBuffer> VlkContext::AllocateSecondaryLevelCommandBuffer(prosper::QueueFamilyType queueFamilyType,uint32_t &universalQueueFamilyIndex)
{
	if(GetUniversalQueueFamilyIndex(queueFamilyType,universalQueueFamilyIndex) == false)
	{
		if(queueFamilyType != prosper::QueueFamilyType::Universal)
			return AllocateSecondaryLevelCommandBuffer(prosper::QueueFamilyType::Universal,universalQueueFamilyIndex);
		return nullptr;
	}
	return std::dynamic_pointer_cast<ISecondaryCommandBuffer>(prosper::VlkSecondaryCommandBuffer::Create(
		*this,GetDevice().get_command_pool_for_queue_family_index(universalQueueFamilyIndex)->alloc_secondary_level_command_buffer(),
		queueFamilyType
	));
}

void VlkContext::ReloadSwapchain()
{
	// Reload swapchain related objects
	WaitIdle();
	ReleaseSwapchain();
	InitSwapchain();
	InitFrameBuffers();
	InitCommandBuffers();
	InitSemaphores();
	InitMainRenderPass();
	InitTemporaryBuffer();
	OnSwapchainInitialized();

	if(m_shaderManager != nullptr)
	{
		auto &shaderManager = *m_shaderManager;
		for(auto &pshader : shaderManager.GetShaders())
		{
			if(pshader->IsGraphicsShader() == false)
				continue;
			auto &shader = static_cast<prosper::ShaderGraphics&>(*pshader);
			auto numPipelines = shader.GetPipelineCount();
			auto bHasStaticViewportOrScissor = false;
			for(auto i=decltype(numPipelines){0};i<numPipelines;++i)
			{
				auto *baseInfo = shader.GetPipelineCreateInfo(i);
				if(baseInfo == nullptr)
					continue;
				auto &info = static_cast<const prosper::GraphicsPipelineCreateInfo&>(*baseInfo);
				if(info.GetScissorBoxesCount() == 0u && info.GetViewportCount() == 0u)
					continue;
				bHasStaticViewportOrScissor = true;
				break;
			}
			if(bHasStaticViewportOrScissor == false)
				continue;
			shader.ReloadPipelines();
		}
	}
}

void VlkContext::InitCommandBuffers()
{
	VkImageSubresourceRange subresource_range;
	auto *universal_queue_ptr = m_devicePtr->get_universal_queue(0);

	subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseArrayLayer = 0;
	subresource_range.baseMipLevel = 0;
	subresource_range.layerCount = 1;
	subresource_range.levelCount = 1;

	/* Set up rendering command buffers. We need one per swap-chain image. */
	uint32_t        n_universal_queue_family_indices = 0;
	const uint32_t* universal_queue_family_indices   = nullptr;

	m_devicePtr->get_queue_family_indices_for_queue_family_type(
		Anvil::QueueFamilyType::UNIVERSAL,
		&n_universal_queue_family_indices,
		&universal_queue_family_indices
	);

	/* Set up rendering command buffers. We need one per swap-chain image. */
	for(auto n_current_swapchain_image=0u;n_current_swapchain_image < m_commandBuffers.size();++n_current_swapchain_image)
	{
		auto cmd_buffer_ptr = prosper::VlkPrimaryCommandBuffer::Create(*this,m_devicePtr->get_command_pool_for_queue_family_index(universal_queue_family_indices[0])->alloc_primary_level_command_buffer(),prosper::QueueFamilyType::Universal);
		cmd_buffer_ptr->SetDebugName("swapchain_cmd" +std::to_string(n_current_swapchain_image));
		//m_devicePtr->get_command_pool(Anvil::QUEUE_FAMILY_TYPE_UNIVERSAL)->alloc_primary_level_command_buffer();

		m_commandBuffers[n_current_swapchain_image] = cmd_buffer_ptr;
		m_cmdFences[n_current_swapchain_image] = Anvil::Fence::create(Anvil::FenceCreateInfo::create(m_devicePtr.get(),true));
	}
}

void VlkContext::InitFrameBuffers()
{
	bool result;
	for(uint32_t n_swapchain_image=0;n_swapchain_image<m_numSwapchainImages;++n_swapchain_image)
	{
		auto createinfo = Anvil::FramebufferCreateInfo::create(
			m_devicePtr.get(),
			GetWindowWidth(),
			GetWindowHeight(),
			1
		);
		result = createinfo->add_attachment(m_swapchainPtr->get_image_view(n_swapchain_image),nullptr /* out_opt_attachment_id_ptrs */);
		anvil_assert(result);
		m_fbos[n_swapchain_image] = Anvil::Framebuffer::create(std::move(createinfo));

		m_fbos[n_swapchain_image]->set_name_formatted("Framebuffer used to render to swapchain image [%d]",n_swapchain_image);
	}
}

void VlkContext::InitSemaphores()
{
	for(auto n_semaphore=0u;n_semaphore < m_numSwapchainImages;++n_semaphore)
	{
		auto new_signal_semaphore_ptr = Anvil::Semaphore::create(Anvil::SemaphoreCreateInfo::create(m_devicePtr.get()));
		auto new_wait_semaphore_ptr = Anvil::Semaphore::create(Anvil::SemaphoreCreateInfo::create(m_devicePtr.get()));

		new_signal_semaphore_ptr->set_name_formatted("Signal semaphore [%d]",n_semaphore);
		new_wait_semaphore_ptr->set_name_formatted("Wait semaphore [%d]",n_semaphore);

		m_frameSignalSemaphores.push_back(std::move(new_signal_semaphore_ptr));
		m_frameWaitSemaphores.push_back(std::move(new_wait_semaphore_ptr));
	}
}

void VlkContext::InitSwapchain()
{
	m_renderingSurfacePtr = Anvil::RenderingSurface::create(Anvil::RenderingSurfaceCreateInfo::create(m_instancePtr.get(),m_devicePtr.get(),m_windowPtr.get()));
	if(m_renderingSurfacePtr->get_width() == 0 || m_renderingSurfacePtr->get_height() == 0)
		return; // Minimized?

	m_n_swapchain_image = 0;
	m_renderingSurfacePtr->set_name("Main rendering surface");
	SetPresentMode(GetPresentMode()); // Update present mode to make sure it's supported by out surface

	auto presentMode = GetPresentMode();
	switch(presentMode)
	{
	case prosper::PresentModeKHR::Mailbox:
		m_numSwapchainImages = 3u;
		break;
	case prosper::PresentModeKHR::Fifo:
		m_numSwapchainImages = 2u;
		break;
	default:
		m_numSwapchainImages = 1u;
	}

	m_swapchainImages.clear();
	m_cmdFences.clear();
	m_commandBuffers.clear();
	m_fbos.clear();

	m_commandBuffers.resize(m_numSwapchainImages);
	m_cmdFences.resize(m_numSwapchainImages);
	m_fbos.resize(m_numSwapchainImages);

	auto createInfo = Anvil::SwapchainCreateInfo::create(
		m_pGpuDevice,
		m_renderingSurfacePtr.get(),m_windowPtr.get(),
		Anvil::Format::B8G8R8A8_UNORM,Anvil::ColorSpaceKHR::SRGB_NONLINEAR_KHR,static_cast<Anvil::PresentModeKHR>(presentMode),
		Anvil::ImageUsageFlagBits::COLOR_ATTACHMENT_BIT | Anvil::ImageUsageFlagBits::TRANSFER_DST_BIT,m_numSwapchainImages
	);
	createInfo->set_mt_safety(Anvil::MTSafety::ENABLED);

	auto recreateSwapchain = (m_swapchainPtr != nullptr);
	if(recreateSwapchain)
		createInfo->set_old_swapchain(m_swapchainPtr.get());
	m_swapchainPtr = Anvil::Swapchain::create(std::move(createInfo));

	/*
	m_swapchainPtr = m_pGpuDevice->create_swapchain(
	m_renderingSurfacePtr.get(),m_windowPtr.get(),
	Anvil::Format::B8G8R8A8_UNORM,Anvil::ColorSpaceKHR::SRGB_NONLINEAR_KHR,static_cast<Anvil::PresentModeKHR>(presentMode),
	Anvil::ImageUsageFlagBits::COLOR_ATTACHMENT_BIT | Anvil::ImageUsageFlagBits::TRANSFER_DST_BIT,m_numSwapchainImages
	);*/

	auto nSwapchainImages = m_swapchainPtr->get_n_images();
	m_swapchainImages.resize(nSwapchainImages);
	for(auto i=decltype(nSwapchainImages){0u};i<nSwapchainImages;++i)
	{
		auto &img = *m_swapchainPtr->get_image(i);
		auto *anvCreateInfo = img.get_create_info_ptr();
		prosper::util::ImageCreateInfo createInfo {};
		createInfo.format = static_cast<prosper::Format>(anvCreateInfo->get_format());
		createInfo.width = anvCreateInfo->get_base_mip_width();
		createInfo.height = anvCreateInfo->get_base_mip_height();
		createInfo.layers = anvCreateInfo->get_n_layers();
		createInfo.tiling = static_cast<prosper::ImageTiling>(anvCreateInfo->get_tiling());
		createInfo.samples = static_cast<prosper::SampleCountFlags>(anvCreateInfo->get_sample_count());
		createInfo.usage = static_cast<prosper::ImageUsageFlags>(anvCreateInfo->get_usage_flags().get_vk());
		createInfo.type = static_cast<prosper::ImageType>(anvCreateInfo->get_type());
		createInfo.postCreateLayout = static_cast<prosper::ImageLayout>(anvCreateInfo->get_post_create_image_layout());
		createInfo.queueFamilyMask = static_cast<prosper::QueueFamilyFlags>(anvCreateInfo->get_queue_families().get_vk());
		createInfo.memoryFeatures = prosper::MemoryFeatureFlags::DeviceLocal;
		m_swapchainImages.at(i) = VlkImage::Create(*this,std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>>{&img,[](Anvil::Image *img) {
			// Don't delete, image will be destroyed by Anvil
		}},createInfo,true);
	}

	m_swapchainPtr->set_name("Main swapchain");

	/* Cache the queue we are going to use for presentation */
	const std::vector<uint32_t>* present_queue_fams_ptr = nullptr;

	if(!m_renderingSurfacePtr->get_queue_families_with_present_support(m_physicalDevicePtr,&present_queue_fams_ptr))
		anvil_assert_fail();

	m_presentQueuePtr = m_devicePtr->get_queue_for_queue_family_index(present_queue_fams_ptr->at(0),0);

	if(recreateSwapchain)
	{
		InitFrameBuffers();
		InitCommandBuffers();
	}
	m_keepAliveResources.clear();
	m_keepAliveResources.resize(m_numSwapchainImages);
}

void VlkContext::InitWindow()
{
#ifdef _WIN32
	const Anvil::WindowPlatform platform = Anvil::WINDOW_PLATFORM_SYSTEM;
#else
	const Anvil::WindowPlatform platform = Anvil::WINDOW_PLATFORM_XCB;
#endif
	auto oldSize = (m_glfwWindow != nullptr) ? m_glfwWindow->GetSize() : Vector2i();
	auto &appName = GetAppName();
	/* Create a window */
	//m_windowPtr = Anvil::WindowFactory::create_window(platform,appName,width,height,true,std::bind(&Context::DrawFrame,this));

	// TODO: Clean this up
	try
	{
		m_glfwWindow = nullptr;
		GLFW::poll_events();
		m_glfwWindow = GLFW::Window::Create(*m_windowCreationInfo); // TODO: Release
#ifdef _WIN32
		auto hWindow = glfwGetWin32Window(const_cast<GLFWwindow*>(m_glfwWindow->GetGLFWWindow()));
#else
#ifdef ENABLE_GLFW_ANVIL_COMPATIBILITY
																	// This is a workaround since GLFW does not expose any functions
																	// for retrieving XCB connection.
		auto hWindow = glfwGetX11Window(const_cast<GLFWwindow*>(m_glfwWindow->GetGLFWWindow()));
		auto *pConnection =  XGetXCBConnection(glfwGetX11Display());
#else
#error "Unable to retrieve xcb connection: GLFW does not expose required functions."
		auto hWindow = glfwGetX11Window(const_cast<GLFWwindow*>(m_glfwWindow->GetGLFWWindow()));
#endif
#endif
		const char *errDesc;
		auto err = glfwGetError(&errDesc);
		if(err != GLFW_NO_ERROR)
		{
			std::cout<<"Error retrieving GLFW window handle: "<<errDesc<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
			exit(EXIT_FAILURE);
		}

		// GLFW does not guarantee to actually use the size which was specified,
		// in some cases it may change, so we have to retrieve it again here
		auto actualWindowSize = m_glfwWindow->GetSize();
		m_windowCreationInfo->width = actualWindowSize.x;
		m_windowCreationInfo->height = actualWindowSize.y;

		m_windowPtr = Anvil::WindowFactory::create_window(platform,hWindow
#ifdef ENABLE_GLFW_ANVIL_COMPATIBILITY
			,pConnection
#endif
		);

		err = glfwCreateWindowSurface(
			m_instancePtr->get_instance_vk(),
			const_cast<GLFWwindow*>(m_glfwWindow->GetGLFWWindow()),
			nullptr,
			reinterpret_cast<VkSurfaceKHR*>(&m_surface)
		);
		if(err != GLFW_NO_ERROR)
		{
			glfwGetError(&errDesc);
			std::cout<<"Error creating GLFW window surface: "<<errDesc<<std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(5));
			exit(EXIT_FAILURE);
		}

		m_glfwWindow->SetResizeCallback([](GLFW::Window &window,Vector2i size) {
			std::cout<<"Resizing..."<<std::endl; // TODO
			});

		//glfwDestroyWindow

		//m_window = window.GetHandle();
		//auto *allocatorCallbacks = GetAllocatorCallbacks();
		//glfwCreateWindowSurface(s_info.instance,const_cast<GLFWwindow*>(window.GetGLFWWindow()),reinterpret_cast<const VkAllocationCallbacks*>(allocatorCallbacks),reinterpret_cast<VkSurfaceKHR*>(&m_surface));

		//m_window->SetKeyCallback(std::bind(&RenderState::KeyCallback,this,std::placeholders::_1,std::placeholders::_2,std::placeholders::_3,std::placeholders::_4,std::placeholders::_5));
		//m_window->SetRefereshCallback(std::bind(&RenderState::RefreshCallback,this,std::placeholders::_1));
		//m_window->SetResizeCallback(std::bind(&RenderState::ResizeCallback,this,std::placeholders::_1,std::placeholders::_2));
		//m_context->AttachWindow(*m_window.get(),features);
	}
	catch(const std::exception &e)
	{

	}
	//catch(Vulkan::Exception &e)
	//{
	//	throw e;
	//}
	//catch(std::exception &e)
	//{
	//	throw Vulkan::Exception{e.what(),vk::Result::eErrorInitializationFailed};
	//}
	OnWindowInitialized();
	if(m_glfwWindow != nullptr && umath::is_flag_set(m_stateFlags,StateFlags::Initialized) == true)
	{
		auto newSize = m_glfwWindow->GetSize();
		if(newSize != oldSize)
			OnResolutionChanged(newSize.x,newSize.y);
	}
}

void VlkContext::InitVulkan(const CreateInfo &createInfo)
{
	auto &appName = GetAppName();
	/* Create a Vulkan instance */
	m_instancePtr = Anvil::Instance::create(Anvil::InstanceCreateInfo::create(
		appName, /* app_name */
		appName, /* engine_name */
		(umath::is_flag_set(m_stateFlags,StateFlags::ValidationEnabled) == true) ? [this](
			Anvil::DebugMessageSeverityFlags severityFlags,
			const char *message
			) -> VkBool32 {
				if(m_callbacks.validationCallback)
					m_callbacks.validationCallback(static_cast<prosper::DebugMessageSeverityFlags>(severityFlags.get_vk()),message);
				return true;
		} :  Anvil::DebugCallbackFunction(),
			false
			)); /* in_mt_safe */

	if(umath::is_flag_set(m_stateFlags,StateFlags::ValidationEnabled) == true)
		prosper::debug::set_debug_mode_enabled(true);

	if(createInfo.device.has_value())
	{
		auto numDevices = m_instancePtr->get_n_physical_devices();
		for(auto i=decltype(numDevices){0u};i<numDevices;++i)
		{
			auto *pDevice = m_instancePtr->get_physical_device(i);
			auto *props = (pDevice != nullptr) ? pDevice->get_device_properties().core_vk1_0_properties_ptr : nullptr;
			if(props == nullptr || static_cast<Vendor>(props->vendor_id) != createInfo.device->vendorId || props->device_id != createInfo.device->deviceId)
				continue;
			m_physicalDevicePtr = m_instancePtr->get_physical_device(i);
		}
	}
	if(m_physicalDevicePtr == nullptr)
		m_physicalDevicePtr = m_instancePtr->get_physical_device(0);

	m_shaderManager = std::make_unique<ShaderManager>(*this);

	/* Create a Vulkan device */
	Anvil::DeviceExtensionConfiguration devExtConfig {};

	// Note: These are required for VR on Nvidia GPUs!
	devExtConfig.extension_status["VK_NV_dedicated_allocation"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["VK_NV_external_memory"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["VK_NV_external_memory_win32"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["VK_NV_win32_keyed_mutex"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;

	auto devCreateInfo = Anvil::DeviceCreateInfo::create_sgpu(
		m_physicalDevicePtr,
		true, /* in_enable_shader_module_cache */
		devExtConfig,
		std::vector<std::string>(),
		Anvil::CommandPoolCreateFlagBits::CREATE_RESET_COMMAND_BUFFER_BIT,
		false /* in_mt_safe */
	);
	// devCreateInfo->set_pipeline_cache_ptr() // TODO
	m_devicePtr = Anvil::SGPUDevice::create(
		std::move(devCreateInfo)
	);
	/*[](Anvil::BaseDevice &dev) -> Anvil::PipelineCacheUniquePtr {
	prosper::PipelineCache::LoadError loadErr {};
	auto pipelineCache = PipelineCache::Load(dev,PIPELINE_CACHE_PATH,loadErr);
	if(pipelineCache == nullptr)
	pipelineCache = PipelineCache::Create(dev);
	if(pipelineCache == nullptr)
	throw std::runtime_error("Unable to create pipeline cache!");
	return pipelineCache;
	}*/

	m_pGpuDevice = static_cast<Anvil::SGPUDevice*>(m_devicePtr.get());
	s_devToContext[m_devicePtr.get()] = this;
}

Vendor VlkContext::GetPhysicalDeviceVendor() const
{
	return static_cast<Vendor>(const_cast<VlkContext*>(this)->GetDevice().get_physical_device_properties().core_vk1_0_properties_ptr->vendor_id);
}

MemoryRequirements VlkContext::GetMemoryRequirements(IImage &img)
{
	static_assert(sizeof(MemoryRequirements) == sizeof(VkMemoryRequirements));
	MemoryRequirements req {};
	vkGetImageMemoryRequirements(GetDevice().get_device_vk(),static_cast<VlkImage&>(img).GetAnvilImage().get_image(),reinterpret_cast<VkMemoryRequirements*>(&req));
	return req;
}

std::unique_ptr<prosper::ShaderModule> VlkContext::CreateShaderModuleFromStageData(
	const std::shared_ptr<ShaderStageProgram> &shaderStageProgram,
	prosper::ShaderStage stage,
	const std::string &entryPointName
)
{
	return std::make_unique<prosper::ShaderModule>(
		shaderStageProgram,
		(stage == prosper::ShaderStage::Compute) ? entryPointName : "",
		(stage == prosper::ShaderStage::Fragment) ? entryPointName : "",
		(stage == prosper::ShaderStage::Geometry) ? entryPointName : "",
		(stage == prosper::ShaderStage::TessellationControl) ? entryPointName : "",
		(stage == prosper::ShaderStage::TessellationEvaluation) ? entryPointName : "",
		(stage == prosper::ShaderStage::Vertex) ? entryPointName : ""
	);
}
uint64_t VlkContext::ClampDeviceMemorySize(uint64_t size,float percentageOfGPUMemory,MemoryFeatureFlags featureFlags) const
{
	auto r = FindCompatibleMemoryType(featureFlags);
	if(r.first == nullptr || r.first->heap_ptr == nullptr)
		throw std::runtime_error("Incompatible memory feature flags");
	auto maxMem = std::floorl(r.first->heap_ptr->size *static_cast<long double>(percentageOfGPUMemory));
	return umath::min(size,static_cast<uint64_t>(maxMem));
}
DeviceSize VlkContext::CalcBufferAlignment(BufferUsageFlags usageFlags)
{
	auto &dev = GetDevice();
	auto alignment = 0u;
	if((usageFlags &BufferUsageFlags::UniformBufferBit) != BufferUsageFlags::None)
		alignment = dev.get_physical_device_properties().core_vk1_0_properties_ptr->limits.min_uniform_buffer_offset_alignment;
	if((usageFlags &BufferUsageFlags::StorageBufferBit) != BufferUsageFlags::None)
	{
		alignment = umath::get_least_common_multiple(
			static_cast<vk::DeviceSize>(alignment),
			dev.get_physical_device_properties().core_vk1_0_properties_ptr->limits.min_storage_buffer_offset_alignment
		);
	}
	return alignment;
}
void VlkContext::GetGLSLDefinitions(glsl::Definitions &outDef) const
{
	outDef.layoutId = "set = setIndex,binding = bindingIndex";
	outDef.layoutPushConstants = "push_constant";
	outDef.vertexIndex = "gl_VertexIndex";
	outDef.instanceIndex = "gl_InstanceIndex";
}

void VlkContext::DoKeepResourceAliveUntilPresentationComplete(const std::shared_ptr<void> &resource)
{
	if(m_cmdFences[m_n_swapchain_image]->is_set())
		return;
	m_keepAliveResources.at(m_n_swapchain_image).push_back(resource);
}

bool VlkContext::IsPresentationModeSupported(prosper::PresentModeKHR presentMode) const
{
	if(m_renderingSurfacePtr == nullptr)
		return true;
	auto res = false;
	m_renderingSurfacePtr->supports_presentation_mode(m_physicalDevicePtr,static_cast<Anvil::PresentModeKHR>(presentMode),&res);
	return res;
}

static Anvil::QueueFamilyFlags queue_family_flags_to_anvil_queue_family(prosper::QueueFamilyFlags flags)
{
	Anvil::QueueFamilyFlags queueFamilies {};
	if((flags &prosper::QueueFamilyFlags::GraphicsBit) != prosper::QueueFamilyFlags::None)
		queueFamilies = queueFamilies | Anvil::QueueFamilyFlagBits::GRAPHICS_BIT;
	if((flags &prosper::QueueFamilyFlags::ComputeBit) != prosper::QueueFamilyFlags::None)
		queueFamilies = queueFamilies | Anvil::QueueFamilyFlagBits::COMPUTE_BIT;
	if((flags &prosper::QueueFamilyFlags::DMABit) != prosper::QueueFamilyFlags::None)
		queueFamilies = queueFamilies | Anvil::QueueFamilyFlagBits::DMA_BIT;
	return queueFamilies;
}

static Anvil::MemoryFeatureFlags memory_feature_flags_to_anvil_flags(prosper::MemoryFeatureFlags flags)
{
	auto memoryFeatureFlags = Anvil::MemoryFeatureFlags{};
	if((flags &prosper::MemoryFeatureFlags::DeviceLocal) != prosper::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::DEVICE_LOCAL_BIT;
	if((flags &prosper::MemoryFeatureFlags::HostCached) != prosper::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::HOST_CACHED_BIT;
	if((flags &prosper::MemoryFeatureFlags::HostCoherent) != prosper::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::HOST_COHERENT_BIT;
	if((flags &prosper::MemoryFeatureFlags::LazilyAllocated) != prosper::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::LAZILY_ALLOCATED_BIT;
	if((flags &prosper::MemoryFeatureFlags::HostAccessable) != prosper::MemoryFeatureFlags::None)
		memoryFeatureFlags |= Anvil::MemoryFeatureFlagBits::MAPPABLE_BIT;
	return memoryFeatureFlags;
}

static void find_compatible_memory_feature_flags(prosper::IPrContext &context,prosper::MemoryFeatureFlags &featureFlags)
{
	auto r = static_cast<prosper::VlkContext&>(context).FindCompatibleMemoryType(featureFlags);
	if(r.first == nullptr)
		throw std::runtime_error("Incompatible memory feature flags");
	featureFlags = r.second;
}

std::shared_ptr<prosper::IUniformResizableBuffer> prosper::VlkContext::DoCreateUniformResizableBuffer(
	const util::BufferCreateInfo &createInfo,uint64_t bufferInstanceSize,
	uint64_t maxTotalSize,const void *data,
	prosper::DeviceSize bufferBaseSize,uint32_t alignment
)
{
	auto buf = CreateBuffer(createInfo,data);
	if(buf == nullptr)
		return nullptr;
	auto r = std::shared_ptr<VkUniformResizableBuffer>(new VkUniformResizableBuffer{*this,*buf,bufferInstanceSize,bufferBaseSize,maxTotalSize,alignment});
	r->Initialize();
	return r;
}
std::shared_ptr<prosper::IDynamicResizableBuffer> prosper::VlkContext::CreateDynamicResizableBuffer(
	util::BufferCreateInfo createInfo,
	uint64_t maxTotalSize,float clampSizeToAvailableGPUMemoryPercentage,const void *data
)
{
	createInfo.size = ClampDeviceMemorySize(createInfo.size,clampSizeToAvailableGPUMemoryPercentage,createInfo.memoryFeatures);
	maxTotalSize = ClampDeviceMemorySize(maxTotalSize,clampSizeToAvailableGPUMemoryPercentage,createInfo.memoryFeatures);
	auto buf = CreateBuffer(createInfo,data);
	if(buf == nullptr)
		return nullptr;
	auto r = std::shared_ptr<VkDynamicResizableBuffer>(new VkDynamicResizableBuffer{*this,*buf,createInfo,maxTotalSize});
	r->Initialize();
	return r;
}

std::shared_ptr<prosper::IBuffer> VlkContext::CreateBuffer(const prosper::util::BufferCreateInfo &createInfo,const void *data)
{
	if(createInfo.size == 0ull)
		return nullptr;
	auto sharingMode = Anvil::SharingMode::EXCLUSIVE;
	if((createInfo.flags &prosper::util::BufferCreateInfo::Flags::ConcurrentSharing) != prosper::util::BufferCreateInfo::Flags::None)
		sharingMode = Anvil::SharingMode::CONCURRENT;

	auto &dev = static_cast<VlkContext&>(*this).GetDevice();
	if((createInfo.flags &prosper::util::BufferCreateInfo::Flags::Sparse) != prosper::util::BufferCreateInfo::Flags::None)
	{
		Anvil::BufferCreateFlags createFlags {Anvil::BufferCreateFlagBits::NONE};
		if((createInfo.flags &prosper::util::BufferCreateInfo::Flags::SparseAliasedResidency) != prosper::util::BufferCreateInfo::Flags::None)
			createFlags |= Anvil::BufferCreateFlagBits::SPARSE_ALIASED_BIT | Anvil::BufferCreateFlagBits::SPARSE_RESIDENCY_BIT;

		return VlkBuffer::Create(*this,Anvil::Buffer::create(
			Anvil::BufferCreateInfo::create_no_alloc(
				&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
				sharingMode,createFlags,static_cast<Anvil::BufferUsageFlagBits>(createInfo.usageFlags)
			)
		),createInfo,0ull,createInfo.size);
	}
	Anvil::BufferCreateFlags createFlags {Anvil::BufferCreateFlagBits::NONE};
	if((createInfo.flags &prosper::util::BufferCreateInfo::Flags::DontAllocateMemory) == prosper::util::BufferCreateInfo::Flags::None)
	{
		auto memoryFeatures = createInfo.memoryFeatures;
		find_compatible_memory_feature_flags(*this,memoryFeatures);
		auto bufferCreateInfo = Anvil::BufferCreateInfo::create_alloc(
			&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
			sharingMode,createFlags,static_cast<Anvil::BufferUsageFlagBits>(createInfo.usageFlags),memory_feature_flags_to_anvil_flags(memoryFeatures)
		);
		bufferCreateInfo->set_client_data(data);
		auto r = VlkBuffer::Create(*this,Anvil::Buffer::create(
			std::move(bufferCreateInfo)
		),createInfo,0ull,createInfo.size);
		return r;
	}
	return VlkBuffer::Create(*this,Anvil::Buffer::create(
		Anvil::BufferCreateInfo::create_no_alloc(
			&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
			sharingMode,createFlags,static_cast<Anvil::BufferUsageFlagBits>(createInfo.usageFlags)
		)
	),createInfo,0ull,createInfo.size);
}

std::shared_ptr<prosper::IImage> create_image(prosper::IPrContext &context,const prosper::util::ImageCreateInfo &pCreateInfo,const std::vector<Anvil::MipmapRawData> *data)
{
	auto createInfo = pCreateInfo;

	constexpr auto flags = MemoryFeatureFlags::HostCached | MemoryFeatureFlags::DeviceLocal;
	if((createInfo.memoryFeatures &flags) == flags)
	{
		context.ValidationCallback(DebugMessageSeverityFlags::ErrorBit,"Attempted to create image with both HostCached and DeviceLocal flags, which is poorly supported. Removing DeviceLocal flag...");
		umath::remove_flag(createInfo.memoryFeatures,MemoryFeatureFlags::DeviceLocal);
	}
	auto &layers = createInfo.layers;
	auto imageCreateFlags = Anvil::ImageCreateFlags{};
	if((createInfo.flags &prosper::util::ImageCreateInfo::Flags::Cubemap) != prosper::util::ImageCreateInfo::Flags::None)
	{
		imageCreateFlags |= Anvil::ImageCreateFlagBits::CUBE_COMPATIBLE_BIT;
		layers = 6u;
	}

	auto &postCreateLayout = createInfo.postCreateLayout;
	if(postCreateLayout == prosper::ImageLayout::ColorAttachmentOptimal && prosper::util::is_depth_format(createInfo.format))
		postCreateLayout = prosper::ImageLayout::DepthStencilAttachmentOptimal;

	auto memoryFeatures = createInfo.memoryFeatures;
	find_compatible_memory_feature_flags(static_cast<prosper::VlkContext&>(context),memoryFeatures);
	auto memoryFeatureFlags = memory_feature_flags_to_anvil_flags(memoryFeatures);
	auto queueFamilies = queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask);
	auto sharingMode = Anvil::SharingMode::EXCLUSIVE;
	if((createInfo.flags &prosper::util::ImageCreateInfo::Flags::ConcurrentSharing) != prosper::util::ImageCreateInfo::Flags::None)
		sharingMode = Anvil::SharingMode::CONCURRENT;

	auto useDiscreteMemory = umath::is_flag_set(createInfo.flags,prosper::util::ImageCreateInfo::Flags::AllocateDiscreteMemory);
	if(
		useDiscreteMemory == false &&
		((createInfo.memoryFeatures &prosper::MemoryFeatureFlags::HostAccessable) != prosper::MemoryFeatureFlags::None ||
		(createInfo.memoryFeatures &prosper::MemoryFeatureFlags::DeviceLocal) == prosper::MemoryFeatureFlags::None)
		)
		useDiscreteMemory = true; // Pre-allocated memory currently only supported for device local memory

	auto sparse = (createInfo.flags &prosper::util::ImageCreateInfo::Flags::Sparse) != prosper::util::ImageCreateInfo::Flags::None;
	auto dontAllocateMemory = umath::is_flag_set(createInfo.flags,prosper::util::ImageCreateInfo::Flags::DontAllocateMemory);

	auto bUseFullMipmapChain = (createInfo.flags &prosper::util::ImageCreateInfo::Flags::FullMipmapChain) != prosper::util::ImageCreateInfo::Flags::None;
	if(useDiscreteMemory == false || sparse || dontAllocateMemory)
	{
		if((createInfo.flags &prosper::util::ImageCreateInfo::Flags::SparseAliasedResidency) != prosper::util::ImageCreateInfo::Flags::None)
			imageCreateFlags |= Anvil::ImageCreateFlagBits::SPARSE_ALIASED_BIT | Anvil::ImageCreateFlagBits::SPARSE_RESIDENCY_BIT;
		;
		auto img = prosper::VlkImage::Create(context,Anvil::Image::create(
			Anvil::ImageCreateInfo::create_no_alloc(
				&static_cast<prosper::VlkContext&>(context).GetDevice(),static_cast<Anvil::ImageType>(createInfo.type),static_cast<Anvil::Format>(createInfo.format),
				static_cast<Anvil::ImageTiling>(createInfo.tiling),static_cast<Anvil::ImageUsageFlagBits>(createInfo.usage),
				createInfo.width,createInfo.height,1u,layers,
				static_cast<Anvil::SampleCountFlagBits>(createInfo.samples),queueFamilies,
				sharingMode,bUseFullMipmapChain,imageCreateFlags,
				static_cast<Anvil::ImageLayout>(postCreateLayout),data
			)
		),createInfo,false);
		if(sparse == false && dontAllocateMemory == false)
			context.AllocateDeviceImageBuffer(*img);
		return img;
	}

	auto result = prosper::VlkImage::Create(context,Anvil::Image::create(
		Anvil::ImageCreateInfo::create_alloc(
			&static_cast<prosper::VlkContext&>(context).GetDevice(),static_cast<Anvil::ImageType>(createInfo.type),static_cast<Anvil::Format>(createInfo.format),
			static_cast<Anvil::ImageTiling>(createInfo.tiling),static_cast<Anvil::ImageUsageFlagBits>(createInfo.usage),
			createInfo.width,createInfo.height,1u,layers,
			static_cast<Anvil::SampleCountFlagBits>(createInfo.samples),queueFamilies,
			sharingMode,bUseFullMipmapChain,memoryFeatureFlags,imageCreateFlags,
			static_cast<Anvil::ImageLayout>(postCreateLayout),data
		)
	),createInfo,false);
	return result;
}

std::shared_ptr<prosper::IImage> prosper::VlkContext::CreateImage(const util::ImageCreateInfo &createInfo,const ImageData &imgData)
{
	auto byteSize = util::get_byte_size(createInfo.format);
	std::vector<Anvil::MipmapRawData> anvMipmapData {};
	for(auto &layerData : imgData)
	{
		for(auto iMipmap=decltype(layerData.size()){0u};iMipmap<layerData.size();++iMipmap)
		{
			auto *mipmapData = layerData.at(iMipmap);
			auto wMipmap = util::calculate_mipmap_size(createInfo.width,iMipmap);
			auto hMipmap = util::calculate_mipmap_size(createInfo.height,iMipmap);
			anvMipmapData.emplace_back(Anvil::MipmapRawData::create_2D_from_uchar_ptr(
				Anvil::ImageAspectFlagBits::COLOR_BIT,0u,
				mipmapData,wMipmap *hMipmap *byteSize,wMipmap *byteSize
			));
		}
	}
	return static_cast<VlkContext*>(this)->CreateImage(createInfo,anvMipmapData);
}

std::pair<const Anvil::MemoryType*,prosper::MemoryFeatureFlags> VlkContext::FindCompatibleMemoryType(MemoryFeatureFlags featureFlags) const
{
	auto r = std::pair<const Anvil::MemoryType*,prosper::MemoryFeatureFlags>{nullptr,featureFlags};
	if(umath::to_integral(featureFlags) == 0u)
		return r;
	prosper::MemoryFeatureFlags requiredFeatures {};

	// Convert usage to requiredFlags and preferredFlags.
	switch(featureFlags)
	{
	case prosper::MemoryFeatureFlags::CPUToGPU:
	case prosper::MemoryFeatureFlags::GPUToCPU:
		requiredFeatures |= prosper::MemoryFeatureFlags::HostAccessable;
	}

	auto anvFlags = memory_feature_flags_to_anvil_flags(featureFlags);
	auto anvRequiredFlags = memory_feature_flags_to_anvil_flags(requiredFeatures);
	auto &dev = GetDevice();
	auto &memProps = dev.get_physical_device_memory_properties();
	for(auto &type : memProps.types)
	{
		if((type.features &anvFlags) == anvFlags)
		{
			// Found preferred flags
			r.first = &type;
			return r;
		}
		if((type.features &anvRequiredFlags) != Anvil::MemoryFeatureFlagBits::NONE)
			r.first = &type; // Found required flags
	}
	r.second = requiredFeatures;
	return r;
}

Anvil::PipelineLayout *VlkContext::GetPipelineLayout(bool graphicsShader,Anvil::PipelineID pipelineId)
{
	auto &dev = GetDevice();
	if(graphicsShader)
		return dev.get_graphics_pipeline_manager()->get_pipeline_layout(pipelineId);
	return dev.get_compute_pipeline_manager()->get_pipeline_layout(pipelineId);
}

void *VlkContext::GetInternalDevice() const
{
	return GetDevice().get_device_vk();
}
void *VlkContext::GetInternalPhysicalDevice() const
{
	return GetDevice().get_physical_device()->get_physical_device();
}
void *VlkContext::GetInternalInstance() const
{
	return GetDevice().get_parent_instance()->get_instance_vk();
}
void *VlkContext::GetInternalUniversalQueue() const
{
	return GetDevice().get_universal_queue(0u)->get_queue();
}
bool VlkContext::IsDeviceExtensionEnabled(const std::string &ext) const
{
	return GetDevice().is_extension_enabled(ext);
}
bool VlkContext::IsInstanceExtensionEnabled(const std::string &ext) const
{
	return GetAnvilInstance().is_instance_extension_enabled(ext);
}

void VlkContext::SubmitCommandBuffer(prosper::ICommandBuffer &cmd,prosper::QueueFamilyType queueFamilyType,bool shouldBlock,prosper::IFence *fence)
{
	switch(queueFamilyType)
	{
	case prosper::QueueFamilyType::Universal:
		m_devicePtr->get_universal_queue(0u)->submit(Anvil::SubmitInfo::create(
			&cmd.GetAPITypeRef<VlkCommandBuffer>().GetAnvilCommandBuffer(),0u,nullptr,0u,nullptr,nullptr,shouldBlock,fence ? &static_cast<VlkFence*>(fence)->GetAnvilFence() : nullptr
		));
		break;
	case prosper::QueueFamilyType::Compute:
		m_devicePtr->get_compute_queue(0u)->submit(Anvil::SubmitInfo::create(
			&cmd.GetAPITypeRef<VlkCommandBuffer>().GetAnvilCommandBuffer(),0u,nullptr,0u,nullptr,nullptr,shouldBlock,fence ? &static_cast<VlkFence*>(fence)->GetAnvilFence() : nullptr
		));
		break;
	default:
		throw std::invalid_argument("No device queue exists for queue family " +std::to_string(umath::to_integral(queueFamilyType)) +"!");
	}
}

bool VlkContext::GetUniversalQueueFamilyIndex(prosper::QueueFamilyType queueFamilyType,uint32_t &queueFamilyIndex) const
{
	auto n_universal_queue_family_indices = 0u;
	const uint32_t *universal_queue_family_indices = nullptr;
	if(m_devicePtr->get_queue_family_indices_for_queue_family_type(
		static_cast<Anvil::QueueFamilyType>(queueFamilyType),
		&n_universal_queue_family_indices,
		&universal_queue_family_indices
	) == false || n_universal_queue_family_indices == 0u)
		return false;
	queueFamilyIndex = universal_queue_family_indices[0];
	return true;
}

void VlkContext::InitAPI(const CreateInfo &createInfo)
{
	InitVulkan(createInfo);
	InitWindow();
	ReloadSwapchain();
}

void VlkContext::InitMainRenderPass()
{
	Anvil::RenderPassAttachmentID render_pass_color_attachment_id;
	prosper::Rect2D scissors[4];

	GetScissorViewportInfo(scissors,nullptr); /* viewports */

	std::unique_ptr<Anvil::RenderPassCreateInfo> render_pass_info_ptr(new Anvil::RenderPassCreateInfo(m_devicePtr.get()));

	render_pass_info_ptr->add_color_attachment(m_swapchainPtr->get_create_info_ptr()->get_format(),
		Anvil::SampleCountFlagBits::_1_BIT,
		Anvil::AttachmentLoadOp::CLEAR,
		Anvil::AttachmentStoreOp::STORE,
		Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
		Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
		false, /* may_alias */
		&render_pass_color_attachment_id
	);
	render_pass_info_ptr->add_subpass(&m_mainSubPass);
	render_pass_info_ptr->add_subpass_color_attachment(
		m_mainSubPass,
		Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
		render_pass_color_attachment_id,
		0, /* in_location */
		nullptr
	); /* in_opt_attachment_resolve_id_ptr */

	m_renderPass = Anvil::RenderPass::create(std::move(render_pass_info_ptr),m_swapchainPtr.get()); // TODO: Deprecated? Remove main render pass entirely

	m_renderPass->set_name("Main renderpass");
}

static std::unique_ptr<Anvil::DescriptorSetCreateInfo> to_anv_descriptor_set_create_info(prosper::DescriptorSetCreateInfo &descSetInfo)
{
	auto dsInfo = Anvil::DescriptorSetCreateInfo::create();
	auto numBindings = descSetInfo.GetBindingCount();
	for(auto i=decltype(numBindings){0u};i<numBindings;++i)
	{
		uint32_t bindingIndex;
		prosper::DescriptorType descType;
		uint32_t descArraySize;
		prosper::ShaderStageFlags stageFlags;
		bool immutableSamplersEnabled;
		prosper::DescriptorBindingFlags flags;
		auto result = descSetInfo.GetBindingPropertiesByIndexNumber(
			i,&bindingIndex,&descType,&descArraySize,
			&stageFlags,&immutableSamplersEnabled,
			&flags
		);
		assert(result && !immutableSamplersEnabled);
		dsInfo->add_binding(
			bindingIndex,static_cast<Anvil::DescriptorType>(descType),descArraySize,static_cast<Anvil::ShaderStageFlagBits>(stageFlags),
			static_cast<Anvil::DescriptorBindingFlagBits>(flags),nullptr
		);
	}
	return dsInfo;
}

static Anvil::ShaderModuleUniquePtr to_anv_shader_module(Anvil::BaseDevice &dev,const prosper::ShaderModule &module)
{
	auto *shaderStageProgram = static_cast<const prosper::VlkShaderStageProgram*>(module.GetShaderStageProgram());
	if(shaderStageProgram == nullptr)
		return nullptr;
	auto &spirvBlob = shaderStageProgram->GetSPIRVBlob();
	return Anvil::ShaderModule::create_from_spirv_blob(
		&dev,
		spirvBlob.data(),spirvBlob.size(),
		module.GetCSEntrypointName(),
		module.GetFSEntrypointName(),
		module.GetGSEntrypointName(),
		module.GetTCEntrypointName(),
		module.GetTEEntrypointName(),
		module.GetVSEntrypointName()
	);
}

static Anvil::ShaderModuleStageEntryPoint to_anv_entrypoint(Anvil::BaseDevice &dev,prosper::ShaderModuleStageEntryPoint &entrypoint)
{
	auto module = to_anv_shader_module(dev,*entrypoint.shader_module_ptr);
	return Anvil::ShaderModuleStageEntryPoint {entrypoint.name,std::move(module),static_cast<Anvil::ShaderStage>(entrypoint.stage)};
}

static void init_base_pipeline_create_info(const prosper::BasePipelineCreateInfo &pipelineCreateInfo,Anvil::BasePipelineCreateInfo &anvPipelineCreateInfo,bool computePipeline)
{
	anvPipelineCreateInfo.set_name(pipelineCreateInfo.GetName());
	for(auto i=decltype(umath::to_integral(prosper::ShaderStage::Count)){0u};i<umath::to_integral(prosper::ShaderStage::Count);++i)
	{
		auto stage = static_cast<prosper::ShaderStage>(i);
		const std::vector<prosper::SpecializationConstant> *specializationConstants;
		const uint8_t *dataBuffer;
		auto success = pipelineCreateInfo.GetSpecializationConstants(stage,&specializationConstants,&dataBuffer);
		assert(success);
		if(success)
		{
			if(computePipeline == false || stage == prosper::ShaderStage::Compute)
			{
				for(auto j=decltype(specializationConstants->size()){0u};j<specializationConstants->size();++j)
				{
					auto &specializationConstant = specializationConstants->at(j);
					if(computePipeline == false)
					{
						static_cast<Anvil::GraphicsPipelineCreateInfo&>(anvPipelineCreateInfo).add_specialization_constant(
							static_cast<Anvil::ShaderStage>(stage),specializationConstant.constantId,specializationConstant.numBytes,dataBuffer +specializationConstant.startOffset
						);
						continue;
					}
					static_cast<Anvil::ComputePipelineCreateInfo&>(anvPipelineCreateInfo).add_specialization_constant(
						specializationConstant.constantId,specializationConstant.numBytes,dataBuffer +specializationConstant.startOffset
					);
				}
			}
		}
	}

	auto &pushConstantRanges = pipelineCreateInfo.GetPushConstantRanges();
	for(auto &pushConstantRange : pushConstantRanges)
		anvPipelineCreateInfo.attach_push_constant_range(pushConstantRange.offset,pushConstantRange.size,static_cast<Anvil::ShaderStageFlagBits>(pushConstantRange.stages));

	auto *dsInfos = pipelineCreateInfo.GetDsCreateInfoItems();
	std::vector<const Anvil::DescriptorSetCreateInfo*> anvDsInfos;
	std::vector<std::unique_ptr<Anvil::DescriptorSetCreateInfo>> anvDsInfosOwned;
	anvDsInfos.reserve(dsInfos->size());
	anvDsInfos.reserve(anvDsInfosOwned.size());
	for(auto &dsCreateInfo : *dsInfos)
	{
		anvDsInfosOwned.push_back(to_anv_descriptor_set_create_info(*dsCreateInfo));
		anvDsInfos.push_back(anvDsInfosOwned.back().get());
	}
	anvPipelineCreateInfo.set_descriptor_set_create_info(&anvDsInfos);
}

std::shared_ptr<prosper::IDescriptorSetGroup> prosper::VlkContext::CreateDescriptorSetGroup(DescriptorSetCreateInfo &descSetInfo)
{
	return static_cast<VlkContext*>(this)->CreateDescriptorSetGroup(descSetInfo,to_anv_descriptor_set_create_info(descSetInfo));
}

std::shared_ptr<prosper::ShaderStageProgram> prosper::VlkContext::CompileShader(prosper::ShaderStage stage,const std::string &shaderPath,std::string &outInfoLog,std::string &outDebugInfoLog,bool reload)
{
	auto shaderLocation = prosper::Shader::GetRootShaderLocation();
	if(shaderLocation.empty() == false)
		shaderLocation += '\\';
	std::vector<unsigned int> spirvBlob {};
	auto success = prosper::glsl_to_spv(*this,stage,shaderLocation +shaderPath,spirvBlob,&outInfoLog,&outDebugInfoLog,reload);
	if(success == false)
		return nullptr;
	return std::make_shared<VlkShaderStageProgram>(std::move(spirvBlob));
}

std::optional<prosper::PipelineID> VlkContext::AddPipeline(
	prosper::Shader &shader,PipelineID shaderPipelineId,const prosper::ComputePipelineCreateInfo &createInfo,
	prosper::ShaderStageData &stage,PipelineID basePipelineId
)
{
	Anvil::PipelineCreateFlags createFlags = Anvil::PipelineCreateFlagBits::ALLOW_DERIVATIVES_BIT;
	auto bIsDerivative = basePipelineId != std::numeric_limits<Anvil::PipelineID>::max();
	if(bIsDerivative)
		createFlags = createFlags | Anvil::PipelineCreateFlagBits::DERIVATIVE_BIT;
	auto &dev = static_cast<VlkContext&>(*this).GetDevice();
	auto computePipelineInfo = Anvil::ComputePipelineCreateInfo::create(
		createFlags,
		to_anv_entrypoint(dev,*stage.entryPoint),
		bIsDerivative ? &basePipelineId : nullptr
	);
	if(computePipelineInfo == nullptr)
		return {};
	init_base_pipeline_create_info(createInfo,*computePipelineInfo,true);
	auto *computePipelineManager = dev.get_compute_pipeline_manager();
	Anvil::PipelineID pipelineId;
	auto r = computePipelineManager->add_pipeline(std::move(computePipelineInfo),&pipelineId);
	if(r == false)
		return {};
	computePipelineManager->bake();
	return pipelineId;
}

std::optional<prosper::PipelineID> prosper::VlkContext::AddPipeline(
	prosper::Shader &shader,PipelineID shaderPipelineId,
	const prosper::GraphicsPipelineCreateInfo &createInfo,IRenderPass &rp,
	prosper::ShaderStageData *shaderStageFs,
	prosper::ShaderStageData *shaderStageVs,
	prosper::ShaderStageData *shaderStageGs,
	prosper::ShaderStageData *shaderStageTc,
	prosper::ShaderStageData *shaderStageTe,
	SubPassID subPassId,
	PipelineID basePipelineId
)
{
	auto &dev = static_cast<VlkContext&>(*this).GetDevice();
	Anvil::PipelineCreateFlags createFlags = Anvil::PipelineCreateFlagBits::ALLOW_DERIVATIVES_BIT;
	auto bIsDerivative = basePipelineId != std::numeric_limits<Anvil::PipelineID>::max();
	if(bIsDerivative)
		createFlags = createFlags | Anvil::PipelineCreateFlagBits::DERIVATIVE_BIT;
	auto gfxPipelineInfo = Anvil::GraphicsPipelineCreateInfo::create(
		createFlags,
		&static_cast<VlkRenderPass&>(rp).GetAnvilRenderPass(),
		subPassId,
		shaderStageFs ? to_anv_entrypoint(dev,*shaderStageFs->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageGs ? to_anv_entrypoint(dev,*shaderStageGs->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageTc ? to_anv_entrypoint(dev,*shaderStageTc->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageTe ? to_anv_entrypoint(dev,*shaderStageTe->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageVs ? to_anv_entrypoint(dev,*shaderStageVs->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		nullptr,bIsDerivative ? &basePipelineId : nullptr
	);
	if(gfxPipelineInfo == nullptr)
		return {};
	gfxPipelineInfo->toggle_depth_writes(createInfo.AreDepthWritesEnabled());
	gfxPipelineInfo->toggle_alpha_to_coverage(createInfo.IsAlphaToCoverageEnabled());
	gfxPipelineInfo->toggle_alpha_to_one(createInfo.IsAlphaToOneEnabled());
	gfxPipelineInfo->toggle_depth_clamp(createInfo.IsDepthClampEnabled());
	gfxPipelineInfo->toggle_depth_clip(createInfo.IsDepthClipEnabled());
	gfxPipelineInfo->toggle_primitive_restart(createInfo.IsPrimitiveRestartEnabled());
	gfxPipelineInfo->toggle_rasterizer_discard(createInfo.IsRasterizerDiscardEnabled());
	gfxPipelineInfo->toggle_sample_mask(createInfo.IsSampleMaskEnabled());

	const float *blendConstants;
	uint32_t numBlendAttachments;
	createInfo.GetBlendingProperties(&blendConstants,&numBlendAttachments);
	gfxPipelineInfo->set_blending_properties(blendConstants);

	for(auto attId=decltype(numBlendAttachments){0u};attId<numBlendAttachments;++attId)
	{
		bool blendingEnabled;
		BlendOp blendOpColor;
		BlendOp blendOpAlpha;
		BlendFactor srcColorBlendFactor;
		BlendFactor dstColorBlendFactor;
		BlendFactor srcAlphaBlendFactor;
		BlendFactor dstAlphaBlendFactor;
		ColorComponentFlags channelWriteMask;
		if(createInfo.GetColorBlendAttachmentProperties(
			attId,&blendingEnabled,&blendOpColor,&blendOpAlpha,
			&srcColorBlendFactor,&dstColorBlendFactor,&srcAlphaBlendFactor,&dstAlphaBlendFactor,
			&channelWriteMask
		))
		{
			gfxPipelineInfo->set_color_blend_attachment_properties(
				attId,blendingEnabled,
				static_cast<Anvil::BlendOp>(blendOpColor),static_cast<Anvil::BlendOp>(blendOpAlpha),
				static_cast<Anvil::BlendFactor>(srcColorBlendFactor),static_cast<Anvil::BlendFactor>(dstColorBlendFactor),
				static_cast<Anvil::BlendFactor>(srcAlphaBlendFactor),static_cast<Anvil::BlendFactor>(dstAlphaBlendFactor),
				static_cast<Anvil::ColorComponentFlagBits>(channelWriteMask)
			);
		}
	}

	bool isDepthBiasStateEnabled;
	float depthBiasConstantFactor;
	float depthBiasClamp;
	float depthBiasSlopeFactor;
	createInfo.GetDepthBiasState(
		&isDepthBiasStateEnabled,
		&depthBiasConstantFactor,&depthBiasClamp,&depthBiasSlopeFactor
	);
	gfxPipelineInfo->toggle_depth_bias(isDepthBiasStateEnabled,depthBiasConstantFactor,depthBiasClamp,depthBiasSlopeFactor);

	bool isDepthBoundsStateEnabled;
	float minDepthBounds;
	float maxDepthBounds;
	createInfo.GetDepthBoundsState(&isDepthBoundsStateEnabled,&minDepthBounds,&maxDepthBounds);
	gfxPipelineInfo->toggle_depth_bounds_test(isDepthBoundsStateEnabled,minDepthBounds,maxDepthBounds);

	bool isDepthTestEnabled;
	CompareOp depthCompareOp;
	createInfo.GetDepthTestState(&isDepthTestEnabled,&depthCompareOp);
	gfxPipelineInfo->toggle_depth_test(isDepthTestEnabled,static_cast<Anvil::CompareOp>(depthCompareOp));

	const DynamicState *dynamicStates;
	uint32_t numDynamicStates;
	createInfo.GetEnabledDynamicStates(&dynamicStates,&numDynamicStates);
	static_assert(sizeof(prosper::DynamicState) == sizeof(Anvil::DynamicState));
	gfxPipelineInfo->toggle_dynamic_states(true,reinterpret_cast<const Anvil::DynamicState*>(dynamicStates),numDynamicStates);

	uint32_t numScissors;
	uint32_t numViewports;
	uint32_t numVertexBindings;
	const IRenderPass *renderPass;
	createInfo.GetGraphicsPipelineProperties(
		&numScissors,&numViewports,&numVertexBindings,
		&renderPass,&subPassId
	);
	for(auto i=decltype(numScissors){0u};i<numScissors;++i)
	{
		int32_t x,y;
		uint32_t w,h;
		if(createInfo.GetScissorBoxProperties(i,&x,&y,&w,&h))
			gfxPipelineInfo->set_scissor_box_properties(i,x,y,w,h);
	}
	for(auto i=decltype(numViewports){0u};i<numViewports;++i)
	{
		float x,y;
		float w,h;
		float minDepth;
		float maxDepth;
		if(createInfo.GetViewportProperties(i,&x,&y,&w,&h,&minDepth,&maxDepth))
			gfxPipelineInfo->set_viewport_properties(i,x,y,w,h,minDepth,maxDepth);
	}
	for(auto i=decltype(numVertexBindings){0u};i<numVertexBindings;++i)
	{
		uint32_t bindingIndex;
		uint32_t stride;
		prosper::VertexInputRate rate;
		uint32_t numAttributes;
		const prosper::VertexInputAttribute *attributes;
		uint32_t divisor;
		if(createInfo.GetVertexBindingProperties(i,&bindingIndex,&stride,&rate,&numAttributes,&attributes,&divisor) == false)
			continue;
		static_assert(sizeof(prosper::VertexInputAttribute) == sizeof(Anvil::VertexInputAttribute));
		gfxPipelineInfo->add_vertex_binding(i,static_cast<Anvil::VertexInputRate>(rate),stride,numAttributes,reinterpret_cast<const Anvil::VertexInputAttribute*>(attributes),divisor);
	}

	bool isLogicOpEnabled;
	LogicOp logicOp;
	createInfo.GetLogicOpState(
		&isLogicOpEnabled,&logicOp
	);
	gfxPipelineInfo->toggle_logic_op(isLogicOpEnabled,static_cast<Anvil::LogicOp>(logicOp));

	bool isSampleShadingEnabled;
	float minSampleShading;
	createInfo.GetSampleShadingState(&isSampleShadingEnabled,&minSampleShading);
	gfxPipelineInfo->toggle_sample_shading(isSampleShadingEnabled);

	SampleCountFlags sampleCount;
	const SampleMask *sampleMask;
	createInfo.GetMultisamplingProperties(&sampleCount,&sampleMask);
	gfxPipelineInfo->set_multisampling_properties(static_cast<Anvil::SampleCountFlagBits>(sampleCount),minSampleShading,*sampleMask);

	auto numDynamicScissors = createInfo.GetDynamicScissorBoxesCount();
	gfxPipelineInfo->set_n_dynamic_scissor_boxes(numDynamicScissors);

	auto numDynamicViewports = createInfo.GetDynamicViewportsCount();
	gfxPipelineInfo->set_n_dynamic_viewports(numDynamicViewports);

	auto primitiveTopology = createInfo.GetPrimitiveTopology();
	gfxPipelineInfo->set_primitive_topology(static_cast<Anvil::PrimitiveTopology>(primitiveTopology));

	PolygonMode polygonMode;
	CullModeFlags cullMode;
	FrontFace frontFace;
	float lineWidth;
	createInfo.GetRasterizationProperties(&polygonMode,&cullMode,&frontFace,&lineWidth);
	gfxPipelineInfo->set_rasterization_properties(
		static_cast<Anvil::PolygonMode>(polygonMode),static_cast<Anvil::CullModeFlagBits>(cullMode),
		static_cast<Anvil::FrontFace>(frontFace),lineWidth
	);

	bool isStencilTestEnabled;
	StencilOp frontStencilFailOp;
	StencilOp frontStencilPassOp;
	StencilOp frontStencilDepthFailOp;
	CompareOp frontStencilCompareOp;
	uint32_t frontStencilCompareMask;
	uint32_t frontStencilWriteMask;
	uint32_t frontStencilReference;
	StencilOp backStencilFailOp;
	StencilOp backStencilPassOp;
	StencilOp backStencilDepthFailOp;
	CompareOp backStencilCompareOp;
	uint32_t backStencilCompareMask;
	uint32_t backStencilWriteMask;
	uint32_t backStencilReference;
	createInfo.GetStencilTestProperties(
		&isStencilTestEnabled,
		&frontStencilFailOp,
		&frontStencilPassOp,
		&frontStencilDepthFailOp,
		&frontStencilCompareOp,
		&frontStencilCompareMask,
		&frontStencilWriteMask,
		&frontStencilReference,
		&backStencilFailOp,
		&backStencilPassOp,
		&backStencilDepthFailOp,
		&backStencilCompareOp,
		&backStencilCompareMask,
		&backStencilWriteMask,
		&backStencilReference
	);
	gfxPipelineInfo->set_stencil_test_properties(
		true,
		static_cast<Anvil::StencilOp>(frontStencilFailOp),static_cast<Anvil::StencilOp>(frontStencilPassOp),static_cast<Anvil::StencilOp>(frontStencilDepthFailOp),static_cast<Anvil::CompareOp>(frontStencilCompareOp),
		frontStencilCompareMask,frontStencilWriteMask,frontStencilReference
	);
	gfxPipelineInfo->set_stencil_test_properties(
		false,
		static_cast<Anvil::StencilOp>(backStencilFailOp),static_cast<Anvil::StencilOp>(backStencilPassOp),static_cast<Anvil::StencilOp>(backStencilDepthFailOp),static_cast<Anvil::CompareOp>(backStencilCompareOp),
		backStencilCompareMask,backStencilWriteMask,backStencilReference
	);
	init_base_pipeline_create_info(createInfo,*gfxPipelineInfo,false);

	auto *gfxPipelineManager = dev.get_graphics_pipeline_manager();
	Anvil::PipelineID pipelineId;
	auto r = gfxPipelineManager->add_pipeline(std::move(gfxPipelineInfo),&pipelineId);
	if(r == false)
		return {};
	return pipelineId;
}

std::shared_ptr<prosper::IImage> create_image(prosper::IPrContext &context,const prosper::util::ImageCreateInfo &pImgCreateInfo,const std::vector<std::shared_ptr<uimg::ImageBuffer>> &imgBuffers,bool cubemap)
{
	if(imgBuffers.empty() || imgBuffers.size() != (cubemap ? 6 : 1))
		return nullptr;
	auto &img0 = imgBuffers.at(0);
	auto format = img0->GetFormat();
	auto w = img0->GetWidth();
	auto h = img0->GetHeight();
	for(auto &img : imgBuffers)
	{
		if(img->GetFormat() != format || img->GetWidth() != w || img->GetHeight() != h)
			return nullptr;
	}
	auto imgCreateInfo = pImgCreateInfo;
	std::optional<uimg::ImageBuffer::Format> conversionFormatRequired = {};
	auto &imgBuf = imgBuffers.front();
	switch(imgBuf->GetFormat())
	{
	case uimg::ImageBuffer::Format::RGB8:
		conversionFormatRequired = uimg::ImageBuffer::Format::RGBA8;
		imgCreateInfo.format = prosper::Format::R8G8B8A8_UNorm;
		break;
	case uimg::ImageBuffer::Format::RGB16:
		conversionFormatRequired = uimg::ImageBuffer::Format::RGBA16;
		imgCreateInfo.format = prosper::Format::R16G16B16A16_SFloat;
		break;
	}
	
	static_assert(umath::to_integral(uimg::ImageBuffer::Format::Count) == 7);
	if(conversionFormatRequired.has_value())
	{
		std::vector<std::shared_ptr<uimg::ImageBuffer>> converted {};
		converted.reserve(imgBuffers.size());
		for(auto &img : imgBuffers)
		{
			auto copy = img->Copy(*conversionFormatRequired);
			converted.push_back(copy);
		}
		return create_image(context,imgCreateInfo,converted,cubemap);
	}

	auto byteSize = prosper::util::get_byte_size(imgCreateInfo.format);
	std::vector<Anvil::MipmapRawData> layers {};
	layers.reserve(imgBuffers.size());
	for(auto iLayer=decltype(imgBuffers.size()){0u};iLayer<imgBuffers.size();++iLayer)
	{
		auto &imgBuf = imgBuffers.at(iLayer);
		if(cubemap)
		{
			layers.push_back(Anvil::MipmapRawData::create_cube_map_from_uchar_ptr(
				Anvil::ImageAspectFlagBits::COLOR_BIT,iLayer,0,
				static_cast<uint8_t*>(imgBuf->GetData()),imgCreateInfo.width *imgCreateInfo.height *byteSize,imgCreateInfo.width *byteSize
			));
		}
		else
		{
			layers.push_back(Anvil::MipmapRawData::create_2D_from_uchar_ptr(
				Anvil::ImageAspectFlagBits::COLOR_BIT,0u,
				static_cast<uint8_t*>(imgBuf->GetData()),imgCreateInfo.width *imgCreateInfo.height *byteSize,imgCreateInfo.width *byteSize
			));
		}
	}
	return static_cast<VlkContext&>(context).CreateImage(imgCreateInfo,layers);
}
extern std::shared_ptr<prosper::IImage> create_image(prosper::IPrContext &context,const prosper::util::ImageCreateInfo &createInfo,const std::vector<Anvil::MipmapRawData> *data);
std::shared_ptr<prosper::IImage> prosper::VlkContext::CreateImage(const util::ImageCreateInfo &createInfo,const std::vector<Anvil::MipmapRawData> &data)
{
	return ::create_image(*this,createInfo,&data);
}
std::shared_ptr<prosper::IRenderPass> prosper::VlkContext::CreateRenderPass(const prosper::util::RenderPassCreateInfo &renderPassInfo,std::unique_ptr<Anvil::RenderPassCreateInfo> anvRenderPassInfo)
{
	return prosper::VlkRenderPass::Create(*this,renderPassInfo,Anvil::RenderPass::create(std::move(anvRenderPassInfo),GetSwapchain().get()));
}
static void init_default_dsg_bindings(Anvil::BaseDevice &dev,Anvil::DescriptorSetGroup &dsg)
{
	// Initialize image sampler bindings with dummy texture
	auto &context = prosper::VlkContext::GetContext(dev);
	auto &dummyTex = context.GetDummyTexture();
	auto &dummyBuf = context.GetDummyBuffer();
	auto numSets = dsg.get_n_descriptor_sets();
	for(auto i=decltype(numSets){0};i<numSets;++i)
	{
		auto *descSet = dsg.get_descriptor_set(i);
		auto *descSetLayout = dsg.get_descriptor_set_layout(i);
		auto &info = *descSetLayout->get_create_info();
		auto numBindings = info.get_n_bindings();
		auto bindingIndex = 0u;
		for(auto j=decltype(numBindings){0};j<numBindings;++j)
		{
			Anvil::DescriptorType descType;
			uint32_t arraySize;
			info.get_binding_properties_by_index_number(j,nullptr,&descType,&arraySize,nullptr,nullptr);
			switch(descType)
			{
			case Anvil::DescriptorType::COMBINED_IMAGE_SAMPLER:
			{
				std::vector<Anvil::DescriptorSet::CombinedImageSamplerBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::CombinedImageSamplerBindingElement{
					Anvil::ImageLayout::SHADER_READ_ONLY_OPTIMAL,
					&static_cast<prosper::VlkImageView&>(*dummyTex->GetImageView()).GetAnvilImageView(),&static_cast<prosper::VlkSampler&>(*dummyTex->GetSampler()).GetAnvilSampler()
					});
				descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
				break;
			}
			case Anvil::DescriptorType::UNIFORM_BUFFER:
			{
				std::vector<Anvil::DescriptorSet::UniformBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::UniformBufferBindingElement{
					&dummyBuf->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
				descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
				break;
			}
			case Anvil::DescriptorType::UNIFORM_BUFFER_DYNAMIC:
			{
				std::vector<Anvil::DescriptorSet::DynamicUniformBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::DynamicUniformBufferBindingElement{
					&dummyBuf->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
				descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
				break;
			}
			case Anvil::DescriptorType::STORAGE_BUFFER:
			{
				std::vector<Anvil::DescriptorSet::StorageBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::StorageBufferBindingElement{
					&dummyBuf->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
				descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
				break;
			}
			case Anvil::DescriptorType::STORAGE_BUFFER_DYNAMIC:
			{
				std::vector<Anvil::DescriptorSet::DynamicStorageBufferBindingElement> bindingElements(arraySize,Anvil::DescriptorSet::DynamicStorageBufferBindingElement{
					&dummyBuf->GetAPITypeRef<VlkBuffer>().GetAnvilBuffer(),0ull,dummyBuf->GetSize()
					});
				descSet->set_binding_array_items(bindingIndex,{0u,arraySize},bindingElements.data());
				break;
			}
			}
			//bindingIndex += arraySize;
			++bindingIndex;
		}
	}
}
std::shared_ptr<prosper::IDescriptorSetGroup> prosper::VlkContext::CreateDescriptorSetGroup(const DescriptorSetCreateInfo &descSetCreateInfo,std::unique_ptr<Anvil::DescriptorSetCreateInfo> descSetInfo)
{
	std::vector<std::unique_ptr<Anvil::DescriptorSetCreateInfo>> descSetInfos = {};
	descSetInfos.push_back(std::move(descSetInfo));
	auto dsg = Anvil::DescriptorSetGroup::create(&static_cast<VlkContext&>(*this).GetDevice(),descSetInfos,Anvil::DescriptorPoolCreateFlagBits::FREE_DESCRIPTOR_SET_BIT);
	init_default_dsg_bindings(static_cast<VlkContext&>(*this).GetDevice(),*dsg);
	return prosper::VlkDescriptorSetGroup::Create(*this,descSetCreateInfo,std::move(dsg));
}

std::shared_ptr<IQueryPool> VlkContext::CreateQueryPool(QueryType queryType,uint32_t maxConcurrentQueries)
{
	if(queryType == QueryType::PipelineStatistics)
		throw std::logic_error("Cannot create pipeline statistics query pool using this overload!");
	auto pool = Anvil::QueryPool::create_non_ps_query_pool(&static_cast<VlkContext&>(*this).GetDevice(),static_cast<VkQueryType>(queryType),maxConcurrentQueries);
	if(pool == nullptr)
		return nullptr;
	return std::shared_ptr<IQueryPool>(new VlkQueryPool(*this,std::move(pool),queryType));
}
std::shared_ptr<IQueryPool> VlkContext::CreateQueryPool(QueryPipelineStatisticFlags statsFlags,uint32_t maxConcurrentQueries)
{
	auto pool = Anvil::QueryPool::create_ps_query_pool(&static_cast<VlkContext&>(*this).GetDevice(),static_cast<Anvil::QueryPipelineStatisticFlagBits>(statsFlags),maxConcurrentQueries);
	if(pool == nullptr)
		return nullptr;
	return std::shared_ptr<IQueryPool>(new VlkQueryPool(*this,std::move(pool),QueryType::PipelineStatistics));
}

bool VlkContext::QueryResult(const TimestampQuery &query,std::chrono::nanoseconds &outTimestampValue) const
{
	uint64_t result;
	if(QueryResult(query,result) == false)
		return false;
	auto ns = result *static_cast<double>(const_cast<VlkContext&>(*this).GetDevice().get_physical_device_properties().core_vk1_0_properties_ptr->limits.timestamp_period);
	outTimestampValue = static_cast<std::chrono::nanoseconds>(static_cast<uint64_t>(ns));
	return true;
}

template<class T,typename TBaseType>
	bool VlkContext::QueryResult(const Query &query,T &outResult,QueryResultFlags resultFlags) const
{
	auto *pool = static_cast<VlkQueryPool*>(query.GetPool());
	if(pool == nullptr)
		return false;
#pragma pack(push,1)
	struct ResultData
	{
		T data;
		TBaseType availability;
	};
#pragma pack(pop)
	auto bAllQueryResultsRetrieved = false;
	ResultData resultData;
	auto bSuccess = pool->GetAnvilQueryPool().get_query_pool_results(query.GetQueryId(),1u,static_cast<Anvil::QueryResultFlagBits>(resultFlags | prosper::QueryResultFlags::WithAvailabilityBit),reinterpret_cast<TBaseType*>(&resultData),&bAllQueryResultsRetrieved);
	if(bSuccess == false || bAllQueryResultsRetrieved == false || resultData.availability == 0)
		return false;
	outResult = resultData.data;
	return true;
}
bool VlkContext::QueryResult(const Query &query,uint32_t &r) const {return QueryResult<uint32_t>(query,r,prosper::QueryResultFlags{});}
bool VlkContext::QueryResult(const Query &query,uint64_t &r) const {return QueryResult<uint64_t>(query,r,prosper::QueryResultFlags::e64Bit);}
bool VlkContext::QueryResult(const PipelineStatisticsQuery &query,PipelineStatistics &outStatistics) const
{
	return QueryResult<PipelineStatistics,uint64_t>(query,outStatistics,prosper::QueryResultFlags::e64Bit);
}
std::shared_ptr<prosper::ISampler> prosper::VlkContext::CreateSampler(const util::SamplerCreateInfo &createInfo)
{
	return VlkSampler::Create(*this,createInfo);
}

std::shared_ptr<prosper::IEvent> prosper::VlkContext::CreateEvent()
{
	return VlkEvent::Create(*this);
}
std::shared_ptr<prosper::IFence> prosper::VlkContext::CreateFence(bool createSignalled)
{
	return VlkFence::Create(*this,createSignalled,nullptr);
}
std::shared_ptr<prosper::IImageView> prosper::VlkContext::DoCreateImageView(
	const prosper::util::ImageViewCreateInfo &createInfo,prosper::IImage &img,Format format,ImageViewType type,prosper::ImageAspectFlags aspectMask,uint32_t numLayers
)
{
	switch(type)
	{
	case ImageViewType::e2D:
		return VlkImageView::Create(*this,img,createInfo,type,aspectMask,Anvil::ImageView::create(
			Anvil::ImageViewCreateInfo::create_2D(
				&static_cast<VlkContext&>(*this).GetDevice(),&static_cast<VlkImage&>(img).GetAnvilImage(),createInfo.baseLayer.has_value() ? *createInfo.baseLayer : 0u,createInfo.baseMipmap,umath::min(createInfo.baseMipmap +createInfo.mipmapLevels,img.GetMipmapCount()) -createInfo.baseMipmap,
				static_cast<Anvil::ImageAspectFlagBits>(aspectMask),static_cast<Anvil::Format>(format),
				static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleRed),static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleGreen),
				static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleBlue),static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleAlpha)
			)
		));
	case ImageViewType::Cube:
		return VlkImageView::Create(*this,img,createInfo,type,aspectMask,Anvil::ImageView::create(
			Anvil::ImageViewCreateInfo::create_cube_map(
				&static_cast<VlkContext&>(*this).GetDevice(),&static_cast<VlkImage&>(img).GetAnvilImage(),createInfo.baseLayer.has_value() ? *createInfo.baseLayer : 0u,createInfo.baseMipmap,umath::min(createInfo.baseMipmap +createInfo.mipmapLevels,img.GetMipmapCount()) -createInfo.baseMipmap,
				static_cast<Anvil::ImageAspectFlagBits>(aspectMask),static_cast<Anvil::Format>(format),
				static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleRed),static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleGreen),
				static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleBlue),static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleAlpha)
			)
		));
	case ImageViewType::e2DArray:
		return VlkImageView::Create(*this,img,createInfo,type,aspectMask,Anvil::ImageView::create(
			Anvil::ImageViewCreateInfo::create_2D_array(
				&static_cast<VlkContext&>(*this).GetDevice(),&static_cast<VlkImage&>(img).GetAnvilImage(),createInfo.baseLayer.has_value() ? *createInfo.baseLayer : 0u,numLayers,createInfo.baseMipmap,umath::min(createInfo.baseMipmap +createInfo.mipmapLevels,img.GetMipmapCount()) -createInfo.baseMipmap,
				static_cast<Anvil::ImageAspectFlagBits>(aspectMask),static_cast<Anvil::Format>(format),
				static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleRed),static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleGreen),
				static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleBlue),static_cast<Anvil::ComponentSwizzle>(createInfo.swizzleAlpha)
			)
		));
	default:
		throw std::invalid_argument("Image view type " +std::to_string(umath::to_integral(type)) +" is currently unsupported!");
	}
}
std::shared_ptr<prosper::IRenderPass> prosper::VlkContext::CreateRenderPass(const util::RenderPassCreateInfo &renderPassInfo)
{
	if(renderPassInfo.attachments.empty())
		throw std::logic_error("Attempted to create render pass with 0 attachments, this is not allowed!");
	auto rpInfo = std::make_unique<Anvil::RenderPassCreateInfo>(&static_cast<VlkContext&>(*this).GetDevice());
	std::vector<Anvil::RenderPassAttachmentID> attachmentIds;
	attachmentIds.reserve(renderPassInfo.attachments.size());
	auto depthStencilAttId = std::numeric_limits<Anvil::RenderPassAttachmentID>::max();
	for(auto &attInfo : renderPassInfo.attachments)
	{
		Anvil::RenderPassAttachmentID attId;
		if(util::is_depth_format(static_cast<prosper::Format>(attInfo.format)) == false)
		{
			rpInfo->add_color_attachment(
				static_cast<Anvil::Format>(attInfo.format),static_cast<Anvil::SampleCountFlagBits>(attInfo.sampleCount),
				static_cast<Anvil::AttachmentLoadOp>(attInfo.loadOp),static_cast<Anvil::AttachmentStoreOp>(attInfo.storeOp),
				static_cast<Anvil::ImageLayout>(attInfo.initialLayout),static_cast<Anvil::ImageLayout>(attInfo.finalLayout),
				true,&attId
			);
		}
		else
		{
			if(depthStencilAttId != std::numeric_limits<Anvil::RenderPassAttachmentID>::max())
				throw std::logic_error("Attempted to add more than one depth stencil attachment to render pass, this is not allowed!");
			rpInfo->add_depth_stencil_attachment(
				static_cast<Anvil::Format>(attInfo.format),static_cast<Anvil::SampleCountFlagBits>(attInfo.sampleCount),
				static_cast<Anvil::AttachmentLoadOp>(attInfo.loadOp),static_cast<Anvil::AttachmentStoreOp>(attInfo.storeOp),
				Anvil::AttachmentLoadOp::DONT_CARE,Anvil::AttachmentStoreOp::DONT_CARE,
				static_cast<Anvil::ImageLayout>(attInfo.initialLayout),static_cast<Anvil::ImageLayout>(attInfo.finalLayout),
				true,&attId
			);
			depthStencilAttId = attId;
		}
		attachmentIds.push_back(attId);
	}
	if(renderPassInfo.subPasses.empty() == false)
	{
		for(auto &subPassInfo : renderPassInfo.subPasses)
		{
			Anvil::SubPassID subPassId;
			rpInfo->add_subpass(&subPassId);
			auto location = 0u;
			for(auto attId : subPassInfo.colorAttachments)
				rpInfo->add_subpass_color_attachment(subPassId,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,attId,location++,nullptr);
			if(subPassInfo.useDepthStencilAttachment)
			{
				if(depthStencilAttId == std::numeric_limits<Anvil::RenderPassAttachmentID>::max())
					throw std::logic_error("Attempted to add depth stencil attachment sub-pass, but no depth stencil attachment has been specified!");
				rpInfo->add_subpass_depth_stencil_attachment(subPassId,Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,depthStencilAttId);
			}
			for(auto &dependency : subPassInfo.dependencies)
			{
				rpInfo->add_subpass_to_subpass_dependency(
					dependency.destinationSubPassId,dependency.sourceSubPassId,
					static_cast<Anvil::PipelineStageFlagBits>(dependency.destinationStageMask),static_cast<Anvil::PipelineStageFlagBits>(dependency.sourceStageMask),
					static_cast<Anvil::AccessFlagBits>(dependency.destinationAccessMask),static_cast<Anvil::AccessFlagBits>(dependency.sourceAccessMask),
					Anvil::DependencyFlagBits::NONE
				);
			}
		}
	}
	else
	{
		Anvil::SubPassID subPassId;
		rpInfo->add_subpass(&subPassId);

		if(depthStencilAttId != std::numeric_limits<Anvil::RenderPassAttachmentID>::max())
			rpInfo->add_subpass_depth_stencil_attachment(subPassId,Anvil::ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,depthStencilAttId);

		auto attId = 0u;
		auto location = 0u;
		for(auto &attInfo : renderPassInfo.attachments)
		{
			if(util::is_depth_format(static_cast<prosper::Format>(attInfo.format)) == false)
				rpInfo->add_subpass_color_attachment(subPassId,Anvil::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,attId,location++,nullptr);
			++attId;
		}
	}
	return static_cast<VlkContext*>(this)->CreateRenderPass(renderPassInfo,std::move(rpInfo));
}
std::shared_ptr<prosper::IFramebuffer> prosper::VlkContext::CreateFramebuffer(uint32_t width,uint32_t height,uint32_t layers,const std::vector<prosper::IImageView*> &attachments)
{
	auto createInfo = Anvil::FramebufferCreateInfo::create(
		&static_cast<VlkContext&>(*this).GetDevice(),width,height,layers
	);
	uint32_t depth = 1u;
	for(auto *att : attachments)
		createInfo->add_attachment(&static_cast<prosper::VlkImageView*>(att)->GetAnvilImageView(),nullptr);
	return prosper::VlkFramebuffer::Create(*this,attachments,width,height,depth,layers,Anvil::Framebuffer::create(
		std::move(createInfo)
	));
}
std::shared_ptr<prosper::IRenderBuffer> prosper::VlkContext::CreateRenderBuffer(
	const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo,const std::vector<prosper::IBuffer*> &buffers,
	const std::vector<prosper::DeviceSize> &offsets,const std::optional<IndexBufferInfo> &indexBufferInfo
)
{
	return VlkRenderBuffer::Create(*this,pipelineCreateInfo,buffers,offsets,indexBufferInfo);
}
uint32_t prosper::VlkContext::GetLastAcquiredSwapchainImageIndex() const {return const_cast<VlkContext*>(this)->GetSwapchain()->get_last_acquired_image_index();}
#pragma optimize("",on)

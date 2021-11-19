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
#include "vk_window.hpp"
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
#include <misc/memory_allocator.h>
#include <misc/memalloc_backends/backend_vma.h>
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
#include <wrappers/pipeline_layout.h>
#include <wrappers/descriptor_set_group.h>
#include <wrappers/graphics_pipeline_manager.h>
#include <wrappers/compute_pipeline_manager.h>
#include <wrappers/query_pool.h>
#include <wrappers/shader_module.h>
#include <misc/image_view_create_info.h>
#include <iglfw/glfw_window.h>
#include <sharedutils/util.h>
#include <sharedutils/magic_enum.hpp>

#include <string>
#include <cmath>

#define ENABLE_ANVIL_THREAD_SAFETY true

using namespace prosper;

static_assert(sizeof(prosper::util::BufferCopy) == sizeof(Anvil::BufferCopy));
static_assert(sizeof(prosper::Extent2D) == sizeof(vk::Extent2D));

#pragma optimize("",off)
VlkShaderStageProgram::VlkShaderStageProgram(std::vector<unsigned int> &&spirvBlob)
	: m_spirvBlob{std::move(spirvBlob)}
{}

const std::vector<unsigned int> &VlkShaderStageProgram::GetSPIRVBlob() const {return m_spirvBlob;}

/////////////

void VkRaytracingFunctions::Initialize(VkDevice dev)
{
	vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(dev,"vkCreateAccelerationStructureKHR");
	vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(dev,"vkDestroyAccelerationStructureKHR");
}
bool VkRaytracingFunctions::IsValid() const
{
	return vkCreateAccelerationStructureKHR && vkDestroyAccelerationStructureKHR;
}

/////////////

std::unique_ptr<VlkShaderPipelineLayout> VlkShaderPipelineLayout::Create(const Shader &shader,uint32_t pipelineIdx)
{
	PipelineID pipelineId;
	if(shader.GetPipelineId(pipelineId,pipelineIdx) == false)
		return nullptr;
	auto &vkContext = static_cast<VlkContext&>(shader.GetContext());
	auto *anvLayout = vkContext.GetPipelineLayout(shader.IsGraphicsShader(),pipelineId);
	if(anvLayout == nullptr)
		return nullptr;
	auto vkLayout = anvLayout->get_pipeline_layout();
	auto res = std::unique_ptr<VlkShaderPipelineLayout>{new VlkShaderPipelineLayout{}};
	res->m_pipelineLayout = vkLayout;
	res->m_pipelineBindPoint = static_cast<VkPipelineBindPoint>(shader.GetPipelineBindPoint());
	return res;
}

/////////////

decltype(VlkContext::s_devToContext) VlkContext::s_devToContext = {};

VlkContext::VlkContext(const std::string &appName,bool bEnableValidation)
	: IPrContext{appName,bEnableValidation}
{}

void VlkContext::OnClose()
{
	m_dummyTexture = nullptr;
	m_dummyCubemapTexture = nullptr;

	m_memAllocator = nullptr;
	IPrContext::OnClose();
}

VlkContext::~VlkContext()
{
	m_renderPass = nullptr;
	m_devicePtr = nullptr;
	m_instancePtr = nullptr;
}

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
	for(auto &wpWindow : m_windows)
	{
		auto window = wpWindow.lock();
		if(!window || window->IsValid() == false)
			continue;
		auto result = static_cast<VlkWindow&>(*window).WaitForFence(outErrMsg);
		if(result == false)
			return false;
	}
	return true;
}

void VlkContext::DrawFrame(const std::function<void(const std::shared_ptr<prosper::IPrimaryCommandBuffer>&,uint32_t)> &drawFrame)
{
	auto errCode = Anvil::SwapchainOperationErrorCode::SUCCESS;
	uint64_t validWindows = 0;
	std::string errMsg;
	for(auto it=m_windows.begin();it!=m_windows.end();)
	{
		auto window = it->lock();
		if(!window || window->IsValid() == false)
		{
			it = m_windows.erase(it);
			continue;
		}
		auto idx = it -m_windows.begin();
		if(idx >= 64)
		{
			++it;
			window->SetState(prosper::Window::State::Inactive);
			continue;
		}
		errCode = static_cast<VlkWindow&>(*window).AcquireImage();
		if(errCode != Anvil::SwapchainOperationErrorCode::SUCCESS)
		{
			++it;
			window->SetState(prosper::Window::State::Inactive);
			continue;
		}

		auto result = static_cast<VlkWindow&>(*window).WaitForFence(errMsg);
		window->SetState(result ? prosper::Window::State::Active : prosper::Window::State::Inactive);
		if(result)
			validWindows |= (1<<idx);

		++it;
	}
	
	ClearKeepAliveResources();

	//auto &keepAliveResources = m_keepAliveResources.at(m_n_swapchain_image);
	//auto numKeepAliveResources = keepAliveResources.size(); // We can clear the resources from the previous render pass of this swapchain after we've waited for the semaphore (i.e. after the frame rendering is complete)
	
	auto swapchainImgIdx = GetLastAcquiredSwapchainImageIndex();
	if(swapchainImgIdx == UINT32_MAX)
		return;
	auto &cmd_buffer_ptr = m_commandBuffers.at(swapchainImgIdx);
	/* Start recording commands */
	auto &primCmd = static_cast<prosper::VlkPrimaryCommandBuffer&>(*cmd_buffer_ptr);
	static_cast<Anvil::PrimaryCommandBuffer&>(primCmd.GetAnvilCommandBuffer()).start_recording(true,false);
	primCmd.SetRecording(true);
	umath::set_flag(m_stateFlags,StateFlags::IsRecording);
	umath::set_flag(m_stateFlags,StateFlags::Idle,false);
	while(m_scheduledBufferUpdates.empty() == false)
	{
		auto &f = m_scheduledBufferUpdates.front();
		f(*cmd_buffer_ptr);
		m_scheduledBufferUpdates.pop();
	}
	drawFrame(GetDrawCommandBuffer(),swapchainImgIdx);
	/* Close the recording process */
	umath::set_flag(m_stateFlags,StateFlags::IsRecording,false);
	primCmd.SetRecording(false);
	static_cast<Anvil::PrimaryCommandBuffer&>(primCmd.GetAnvilCommandBuffer()).stop_recording();

	if(!m_window)
		return;
	// We need to update the main window first
	auto &sigSem = static_cast<VlkWindow&>(*m_window).Submit(primCmd);
	static_cast<VlkWindow&>(*m_window).Present(&sigSem);
	auto *mainWaitSemaphore =  static_cast<VlkWindow&>(*m_window).GetCurrentFrameWaitSemaphore();
	
	// Now we can update all other windows
	for(uint32_t idx=0; auto &wpWindow : m_windows)
	{
		auto window = wpWindow.lock();
		if(!window || window->IsValid() == false)
		{
			++idx;
			continue;
		}
		
		if(window == m_window)
		{
			++idx;
			continue;
		}
		if((validWindows &(1<<idx)) == 0)
			continue;
		static_cast<VlkWindow&>(*window).Submit(primCmd,mainWaitSemaphore);
		static_cast<VlkWindow&>(*window).Present(&sigSem);
	}
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
	return const_cast<VlkContext*>(this)->GetDevice().get_physical_device_surface_capabilities(&static_cast<VlkWindow&>(const_cast<Window&>(GetWindow())).GetRenderingSurface(),&caps);
}

void VlkContext::ReloadWindow()
{
	WaitIdle();
	m_renderPass.reset();
	m_commandBuffers.clear();
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
	IPrContext::Release();

	auto &dev = GetDevice();
	auto it = s_devToContext.find(&dev);
	if(it != s_devToContext.end())
		s_devToContext.erase(it);
	
	m_commandBuffers.clear();
	m_renderPass = nullptr;
	m_devicePtr.reset(); // All Vulkan resources related to the device have to be cleared at this point!
	m_instancePtr.reset();
	m_window = nullptr;
}

Anvil::SGPUDevice &VlkContext::GetDevice() {return *m_pGpuDevice;}
const Anvil::SGPUDevice &VlkContext::GetDevice() const {return const_cast<VlkContext*>(this)->GetDevice();}
const std::shared_ptr<Anvil::RenderPass> &VlkContext::GetMainRenderPass() const {return m_renderPass;}
Anvil::SubPassID VlkContext::GetMainSubPassID() const {return m_mainSubPass;}
Anvil::Swapchain &VlkContext::GetSwapchain() {return static_cast<VlkWindow&>(GetWindow()).GetSwapchain();}

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
		return dev.get_graphics_pipeline_manager()->delete_pipeline(m_prosperPipelineToAnvilPipeline[pipelineId]);
	return dev.get_compute_pipeline_manager()->delete_pipeline(m_prosperPipelineToAnvilPipeline[pipelineId]);
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

std::shared_ptr<prosper::ICommandBufferPool> VlkContext::CreateCommandBufferPool(prosper::QueueFamilyType queueFamilyType)
{
	uint32_t universalQueueFamilyIndex;
	if(GetUniversalQueueFamilyIndex(queueFamilyType,universalQueueFamilyIndex) == false)
		return nullptr;
	auto &dev = GetDevice();
	auto pool = Anvil::CommandPool::create(&dev,dev.get_create_info_ptr()->get_helper_command_pool_create_flags(),universalQueueFamilyIndex);
	return VlkCommandPool::Create(*this,queueFamilyType,std::move(pool));
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
	for(auto &wpWindow : m_windows)
	{
		auto window = wpWindow.lock();
		if(!window || window->IsValid() == false)
			continue;
		auto &vlkWindow = static_cast<VlkWindow&>(*window);
		vlkWindow.ReloadSwapchain();
		if(&vlkWindow == m_window.get())
			OnPrimaryWindowSwapchainReloaded();
	}

	InitCommandBuffers();
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
	}
}

void VlkContext::OnPrimaryWindowSwapchainReloaded()
{
	auto numSwapchainImages = static_cast<VlkWindow&>(GetWindow()).GetSwapchainImageCount();
	if(m_commandBuffers.empty())
	{
		m_commandBuffers.resize(numSwapchainImages);
		InitCommandBuffers();
	}
	m_keepAliveResources.clear();
	m_keepAliveResources.resize(numSwapchainImages);
}

std::shared_ptr<Window> VlkContext::CreateWindow(const WindowSettings &windowCreationInfo)
{
	auto window = VlkWindow::Create(windowCreationInfo,*this);
	if(!window)
		return nullptr;
	window->ReloadSwapchain();
	m_windows.push_back(window);
	return window;
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
				{
					std::string msg = message;
					AddDebugObjectInformation(msg);
					m_callbacks.validationCallback(static_cast<prosper::DebugMessageSeverityFlags>(severityFlags.get_vk()),msg);
				}
				return true;
		} :  Anvil::DebugCallbackFunction(),
			ENABLE_ANVIL_THREAD_SAFETY
		));

	if(umath::is_flag_set(m_stateFlags,StateFlags::ValidationEnabled) == true)
	{
		prosper::debug::set_debug_mode_enabled(true);
		// m_customValidationEnabled = true;
	}

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

	// OpenXr
	devExtConfig.extension_status["XR_KHR_vulkan_enable2"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;

	// Raytracing
	devExtConfig.extension_status["VK_KHR_acceleration_structure"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["VK_KHR_ray_tracing_pipeline"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["VK_KHR_ray_query"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["SPV_KHR_ray_tracing"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["SPV_KHR_ray_query"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["GLSL_EXT_ray_tracing"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["GLSL_EXT_ray_query"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;
	devExtConfig.extension_status["GLSL_EXT_ray_flags_primitive_culling"] = Anvil::ExtensionAvailability::ENABLE_IF_AVAILABLE;

	auto devCreateInfo = Anvil::DeviceCreateInfo::create_sgpu(
		m_physicalDevicePtr,
		true, /* in_enable_shader_module_cache */
		devExtConfig,
		std::vector<std::string>(),
		Anvil::CommandPoolCreateFlagBits::CREATE_RESET_COMMAND_BUFFER_BIT,
		ENABLE_ANVIL_THREAD_SAFETY
	);
	// devCreateInfo->set_pipeline_cache_ptr() // TODO
	m_devicePtr = Anvil::SGPUDevice::create(
		std::move(devCreateInfo)
	);
	if(m_useAllocator)
		m_useReservedDeviceLocalImageBuffer = false; // VMA already handles the allocation of large buffers; We'll just let it do its thing for image allocation

	if(m_useAllocator)
		m_memAllocator = Anvil::MemoryAllocator::create_vma(m_devicePtr.get());

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

	m_rtFunctions.Initialize(m_devicePtr->get_device_vk());

	auto vendor = GetPhysicalDeviceVendor();
	if(vendor == Vendor::AMD)
	{
		// There's a driver bug (?) with some AMD GPUs where baking a shader
		// during runtime can cause it to completely freeze. To avoid that, we'll
		// just load all shaders immediately (at the cost of slower loading times).
		m_loadShadersLazily = false;
	}

#if 0
	auto &memProps = m_physicalDevicePtr->get_memory_properties();
	auto budget = m_physicalDevicePtr->get_available_memory_budget();

	auto numHeaps = memProps.n_heaps;
	std::cout<<"Heaps: "<<numHeaps<<std::endl;
	for(auto i=decltype(numHeaps){0};i<numHeaps;++i)
	{
		auto &heap = memProps.heaps[i];
		std::vector<std::string> flags;
		if((heap.flags &Anvil::MemoryHeapFlagBits::DEVICE_LOCAL_BIT) != Anvil::MemoryHeapFlagBits::NONE)
			flags.push_back("DEVICE_LOCAL_BIT");
		if((heap.flags &Anvil::MemoryHeapFlagBits::MULTI_INSTANCE_BIT_KHR) != Anvil::MemoryHeapFlagBits::NONE)
			flags.push_back("MULTI_INSTANCE_BIT_KHR");
		
		std::cout<<"Heap: "<<heap.index<<std::endl;
		std::cout<<"Budget: "<<::util::get_pretty_bytes(budget.heap_budget[i])<<" ("<<budget.heap_budget[i]<<")"<<std::endl;
		std::cout<<"Usage: "<<::util::get_pretty_bytes(budget.heap_usage[i])<<" ("<<budget.heap_usage[i]<<")"<<std::endl;
		std::cout<<"Flags: ";
		auto first = true;
		for(auto &flag : flags)
		{
			if(first)
				first = false;
			else
				std::cout<<" | ";
			std::cout<<flag;
		}
		std::cout<<std::endl;

		std::cout<<"Size: "<<::util::get_pretty_bytes(heap.size)<<" ("<<heap.size<<")"<<std::endl;

		auto types = memProps.types;
		uint32_t typeIdx = 0;
		for(auto &type : types)
		{
			std::cout<<"Type "<<typeIdx<<":"<<std::endl;
			std::cout<<"Memory properties: "<<magic_enum::flags::enum_name(static_cast<prosper::MemoryPropertyFlags>(type.flags.get_vk()))<<std::endl;
			std::cout<<"Memory features: "<<magic_enum::flags::enum_name(static_cast<prosper::MemoryFeatureFlags>(type.features.get_vk()))<<std::endl;
			
			++typeIdx;
		}
		std::cout<<std::endl;
	}

	if(m_useAllocator)
	{
		auto *backend = m_memAllocator->GetAllocatorBackend();
		auto vmaHandle = static_cast<Anvil::MemoryAllocatorBackends::VMA*>(backend)->GetVmaHandle();
		VmaBudget budget;
		vmaGetBudget(vmaHandle,&budget);
		// static std::unique_ptr<VMA>

		VmaStats stats;
		vmaCalculateStats(vmaHandle,&stats);
		char *statsString = nullptr;
		vmaBuildStatsString(vmaHandle,&statsString,true /* detailedMap */);
		std::cout<<statsString<<std::endl;
		vmaFreeStatsString(vmaHandle,statsString);
	}
#endif
}

std::optional<std::string> VlkContext::DumpMemoryBudget() const
{
	if(m_memAllocator == nullptr)
		return {};
	auto *backend = m_memAllocator->GetAllocatorBackend();
	if(backend == nullptr)
		return {};
	auto vmaHandle = static_cast<Anvil::MemoryAllocatorBackends::VMA*>(backend)->GetVmaHandle();
	if(!vmaHandle)
		return {};
	VmaBudget budget;
	vmaGetBudget(vmaHandle,&budget);

	std::stringstream str {};
	str<<"Total block size: "<<::util::get_pretty_bytes(budget.blockBytes)<<" ("<<budget.blockBytes<<")\n";
	str<<"Total allocation size: "<<::util::get_pretty_bytes(budget.allocationBytes)<<" ("<<budget.allocationBytes<<")\n";
	str<<"Current memory usage "<<::util::get_pretty_bytes(budget.usage)<<" ("<<budget.usage<<")\n";
	str<<"Available memory: "<<::util::get_pretty_bytes(budget.budget)<<" ("<<budget.budget<<")\n";
	return str.str();
}
std::optional<std::string> VlkContext::DumpMemoryStats() const
{
	if(m_memAllocator == nullptr)
		return {};
	auto *backend = m_memAllocator->GetAllocatorBackend();
	if(backend == nullptr)
		return {};
	auto vmaHandle = static_cast<Anvil::MemoryAllocatorBackends::VMA*>(backend)->GetVmaHandle();
	if(!vmaHandle)
		return {};
	char *statsString = nullptr;
	vmaBuildStatsString(vmaHandle,&statsString,true /* detailedMap */);
	std::string str = statsString;
	vmaFreeStatsString(vmaHandle,statsString);
	return str;
}

std::optional<prosper::util::VendorDeviceInfo> VlkContext::GetVendorDeviceInfo() const {return util::get_vendor_device_info(*this);}

std::optional<std::vector<prosper::util::VendorDeviceInfo>> VlkContext::GetAvailableVendorDevices() const {return util::get_available_vendor_devices(*this);}
std::optional<prosper::util::PhysicalDeviceMemoryProperties> VlkContext::GetPhysicslDeviceMemoryProperties() const {return util::get_physical_device_memory_properties(*this);}

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
	auto swapchainImgIdx = GetLastAcquiredSwapchainImageIndex();
	auto *fence = static_cast<VlkWindow&>(GetWindow()).GetFence(swapchainImgIdx);
	if(!fence || fence->is_set())
		return;
	m_keepAliveResources.at(swapchainImgIdx).push_back(resource);
}

bool VlkContext::IsPresentationModeSupported(prosper::PresentModeKHR presentMode) const
{
	return m_window ? static_cast<const VlkWindow&>(GetWindow()).IsPresentationModeSupported(presentMode) : true;
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
		if(m_useAllocator)
		{
			auto bufferCreateInfo = Anvil::BufferCreateInfo::create_no_alloc(
				&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
				sharingMode,createFlags,static_cast<Anvil::BufferUsageFlagBits>(createInfo.usageFlags)
			);
			bufferCreateInfo->set_client_data(data);

			auto buf = Anvil::Buffer::create(std::move(bufferCreateInfo));
			if(data)
			{
				std::unique_ptr<uint8_t[]> dataPtr {new uint8_t[createInfo.size]};
				memcpy(dataPtr.get(),data,createInfo.size);
				m_memAllocator->add_buffer_with_uchar8_data_ptr_based_post_fill(
					buf.get(),std::move(dataPtr),memory_feature_flags_to_anvil_flags(memoryFeatures)
				);
			}
			else
			{
				m_memAllocator->add_buffer(
					buf.get(),memory_feature_flags_to_anvil_flags(memoryFeatures)
				);
			}
			return VlkBuffer::Create(*this,std::move(buf),createInfo,0ull,createInfo.size);
		}
		auto bufferCreateInfo = Anvil::BufferCreateInfo::create_alloc(
			&dev,static_cast<VkDeviceSize>(createInfo.size),queue_family_flags_to_anvil_queue_family(createInfo.queueFamilyMask),
			sharingMode,createFlags,static_cast<Anvil::BufferUsageFlagBits>(createInfo.usageFlags),memory_feature_flags_to_anvil_flags(memoryFeatures)
		);
		bufferCreateInfo->set_client_data(data);
		return VlkBuffer::Create(*this,Anvil::Buffer::create(
			std::move(bufferCreateInfo)
		),createInfo,0ull,createInfo.size);
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
		
		auto anvImg = Anvil::Image::create(Anvil::ImageCreateInfo::create_no_alloc(
			&static_cast<prosper::VlkContext&>(context).GetDevice(),static_cast<Anvil::ImageType>(createInfo.type),static_cast<Anvil::Format>(createInfo.format),
			static_cast<Anvil::ImageTiling>(createInfo.tiling),static_cast<Anvil::ImageUsageFlagBits>(createInfo.usage),
			createInfo.width,createInfo.height,1u,layers,
			static_cast<Anvil::SampleCountFlagBits>(createInfo.samples),queueFamilies,
			sharingMode,bUseFullMipmapChain,imageCreateFlags,
			static_cast<Anvil::ImageLayout>(postCreateLayout),data
		));
		auto *memAllocator = static_cast<prosper::VlkContext&>(context).GetMemoryAllocator();
		if(memAllocator)
		{
			if(sparse == false && dontAllocateMemory == false)
				memAllocator->add_image_whole(anvImg.get(),memoryFeatureFlags);
		}
		auto img = prosper::VlkImage::Create(context,std::move(anvImg),createInfo,false);
		if(!memAllocator && sparse == false && dontAllocateMemory == false)
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

std::shared_ptr<IImage> prosper::VlkContext::CreateImage(const util::ImageCreateInfo &createInfo,const std::function<const uint8_t*(uint32_t layer,uint32_t mipmap,uint32_t &dataSize,uint32_t &rowSize)> &getImageData)
{
	auto byteSize = util::get_pixel_size(createInfo.format);
	std::vector<Anvil::MipmapRawData> anvMipmapData {};
	if(getImageData)
	{
		auto numMipmaps = umath::is_flag_set(createInfo.flags,util::ImageCreateInfo::Flags::FullMipmapChain) ? util::calculate_mipmap_count(createInfo.width,createInfo.height) : 1u;
		anvMipmapData.reserve(createInfo.layers *numMipmaps);
		for(auto iLayer=decltype(createInfo.layers){0u};iLayer<createInfo.layers;++iLayer)
		{
			for(auto iMipmap=decltype(numMipmaps){0u};iMipmap<numMipmaps;++iMipmap)
			{
				auto wMipmap = util::calculate_mipmap_size(createInfo.width,iMipmap);
				auto hMipmap = util::calculate_mipmap_size(createInfo.height,iMipmap);
				auto dataSize = wMipmap *hMipmap *byteSize;
				auto rowSize = wMipmap *byteSize;
				auto *mipmapData = getImageData(iLayer,iMipmap,dataSize,rowSize);
				if(mipmapData == nullptr)
					continue;
				anvMipmapData.emplace_back(Anvil::MipmapRawData::create_2D_from_uchar_ptr(
					util::is_depth_format(createInfo.format) ? Anvil::ImageAspectFlagBits::DEPTH_BIT : Anvil::ImageAspectFlagBits::COLOR_BIT,iMipmap,
					mipmapData,dataSize,rowSize
				));
			}
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
		return dev.get_graphics_pipeline_manager()->get_pipeline_layout(m_prosperPipelineToAnvilPipeline[pipelineId]);
	return dev.get_compute_pipeline_manager()->get_pipeline_layout(m_prosperPipelineToAnvilPipeline[pipelineId]);
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

	auto &window = GetWindow();
	auto &swapchain = static_cast<VlkWindow&>(window).GetSwapchain();
	render_pass_info_ptr->add_color_attachment(swapchain.get_create_info_ptr()->get_format(),
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

	m_renderPass = Anvil::RenderPass::create(std::move(render_pass_info_ptr),&swapchain); // TODO: Deprecated? Remove main render pass entirely

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

std::optional<std::unordered_map<prosper::ShaderStage,std::string>> prosper::VlkContext::OptimizeShader(const std::unordered_map<prosper::ShaderStage,std::string> &shaderStages,std::string &outInfoLog)
{
#ifdef PROSPER_VULKAN_ENABLE_LUNAR_GLASS
	return prosper::optimize_glsl(*this,shaderStages,outInfoLog);
#else
	return {};
#endif
}

bool prosper::VlkContext::GetParsedShaderSourceCode(
	prosper::Shader &shader,std::vector<std::string> &outGlslCodePerStage,std::vector<prosper::ShaderStage> &outGlslCodeStages,std::string &outInfoLog,std::string &outDebugInfoLog,prosper::ShaderStage &outErrStage
) const
{
	auto &stages = shader.GetStages();
	outGlslCodePerStage.reserve(stages.size());
	outGlslCodeStages.reserve(stages.size());
	for(auto i=decltype(stages.size()){0};i<stages.size();++i)
	{
		auto &stage = stages.at(i);
		if(stage == nullptr)
			continue;
		std::string sourceFilePath;
		if(shader.GetSourceFilePath(stage->stage,sourceFilePath) == false)
			return {};
		std::string infoLog;
		std::string debugInfoLog;
		auto shaderCode = prosper::glsl::load_glsl(const_cast<VlkContext&>(*this),stage->stage,sourceFilePath,&infoLog,&debugInfoLog);
		if(shaderCode.has_value() == false)
			return false;
		outGlslCodePerStage.push_back(*shaderCode);
		outGlslCodeStages.push_back(stage->stage);
	}
	return true;
}

std::optional<prosper::PipelineID> VlkContext::AddPipeline(
	prosper::Shader &shader,PipelineID shaderPipelineId,const prosper::ComputePipelineCreateInfo &createInfo,
	prosper::ShaderStageData &stage,PipelineID basePipelineId
)
{
	PipelineID pipelineId;
	if(shader.GetPipelineId(pipelineId,shaderPipelineId) == false)
		return {};
	Anvil::PipelineCreateFlags createFlags = Anvil::PipelineCreateFlagBits::ALLOW_DERIVATIVES_BIT;
	auto bIsDerivative = basePipelineId != std::numeric_limits<PipelineID>::max();
	if(bIsDerivative)
		createFlags = createFlags | Anvil::PipelineCreateFlagBits::DERIVATIVE_BIT;
	auto &dev = static_cast<VlkContext&>(*this).GetDevice();
	Anvil::PipelineID anvBasePipelineId;
	if(bIsDerivative)
		anvBasePipelineId = m_prosperPipelineToAnvilPipeline[basePipelineId];
	auto computePipelineInfo = Anvil::ComputePipelineCreateInfo::create(
		createFlags,
		to_anv_entrypoint(dev,*stage.entryPoint),
		bIsDerivative ? &anvBasePipelineId : nullptr
	);
	if(computePipelineInfo == nullptr)
		return {};
	init_base_pipeline_create_info(createInfo,*computePipelineInfo,true);
	auto *computePipelineManager = dev.get_compute_pipeline_manager();
	Anvil::PipelineID anvPipelineId;
	auto r = computePipelineManager->add_pipeline(std::move(computePipelineInfo),&anvPipelineId);
	if(r == false)
		return {};
	AddShaderPipeline(shader,shaderPipelineId,pipelineId);
	if(pipelineId >= m_prosperPipelineToAnvilPipeline.size())
		m_prosperPipelineToAnvilPipeline.resize(pipelineId +1,std::numeric_limits<Anvil::PipelineID>::max());
	m_prosperPipelineToAnvilPipeline[pipelineId] = anvPipelineId;
	computePipelineManager->bake();
	return pipelineId;
}

std::optional<PipelineID> VlkContext::AddPipeline(
	prosper::Shader &shader,PipelineID shaderPipelineId,const prosper::RayTracingPipelineCreateInfo &createInfo,
	prosper::ShaderStageData &stage,PipelineID basePipelineId
)
{
	// TODO: This is a stub
	return {};
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
	PipelineID pipelineId;
	if(shader.GetPipelineId(pipelineId,shaderPipelineId) == false)
		return {};
	auto &dev = static_cast<VlkContext&>(*this).GetDevice();
	Anvil::PipelineCreateFlags createFlags = Anvil::PipelineCreateFlagBits::ALLOW_DERIVATIVES_BIT;
	auto bIsDerivative = basePipelineId != std::numeric_limits<PipelineID>::max();
	if(bIsDerivative)
		createFlags = createFlags | Anvil::PipelineCreateFlagBits::DERIVATIVE_BIT;
	Anvil::PipelineID anvBasePipelineId;
	if(bIsDerivative)
		anvBasePipelineId = m_prosperPipelineToAnvilPipeline[basePipelineId];
	auto gfxPipelineInfo = Anvil::GraphicsPipelineCreateInfo::create(
		createFlags,
		&static_cast<VlkRenderPass&>(rp).GetAnvilRenderPass(),
		subPassId,
		shaderStageFs ? to_anv_entrypoint(dev,*shaderStageFs->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageGs ? to_anv_entrypoint(dev,*shaderStageGs->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageTc ? to_anv_entrypoint(dev,*shaderStageTc->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageTe ? to_anv_entrypoint(dev,*shaderStageTe->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		shaderStageVs ? to_anv_entrypoint(dev,*shaderStageVs->entryPoint) : Anvil::ShaderModuleStageEntryPoint{},
		nullptr,bIsDerivative ? &anvBasePipelineId : nullptr
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
	gfxPipelineInfo->toggle_stencil_test(isStencilTestEnabled);
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
	Anvil::PipelineID anvPipelineId;
	auto r = gfxPipelineManager->add_pipeline(std::move(gfxPipelineInfo),&anvPipelineId);
	if(r == false)
		return {};
	AddShaderPipeline(shader,shaderPipelineId,pipelineId);
	if(pipelineId >= m_prosperPipelineToAnvilPipeline.size())
		m_prosperPipelineToAnvilPipeline.resize(pipelineId +1,std::numeric_limits<Anvil::PipelineID>::max());
	m_prosperPipelineToAnvilPipeline[pipelineId] = anvPipelineId;
	if(IsValidationEnabled() || !m_loadShadersLazily)
		gfxPipelineManager->bake();
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
	std::optional<uimg::Format> conversionFormatRequired = {};
	auto &imgBuf = imgBuffers.front();
	switch(imgBuf->GetFormat())
	{
	case uimg::Format::RGB8:
		conversionFormatRequired = uimg::Format::RGBA8;
		imgCreateInfo.format = prosper::Format::R8G8B8A8_UNorm;
		break;
	case uimg::Format::RGB16:
		conversionFormatRequired = uimg::Format::RGBA16;
		imgCreateInfo.format = prosper::Format::R16G16B16A16_SFloat;
		break;
	}
	
	static_assert(umath::to_integral(uimg::Format::Count) == 13);
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
	return prosper::VlkRenderPass::Create(*this,renderPassInfo,Anvil::RenderPass::create(std::move(anvRenderPassInfo),&GetSwapchain()));
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
	auto ns = result *static_cast<long double>(const_cast<VlkContext&>(*this).GetDevice().get_physical_device_properties().core_vk1_0_properties_ptr->limits.timestamp_period);
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
				static_cast<Anvil::AttachmentLoadOp>(attInfo.stencilLoadOp),static_cast<Anvil::AttachmentStoreOp>(attInfo.stencilStoreOp),
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
std::unique_ptr<IShaderPipelineLayout> prosper::VlkContext::GetShaderPipelineLayout(const Shader &shader,uint32_t pipelineIdx) const {return VlkShaderPipelineLayout::Create(shader,pipelineIdx);}
std::shared_ptr<prosper::IRenderBuffer> prosper::VlkContext::CreateRenderBuffer(
	const prosper::GraphicsPipelineCreateInfo &pipelineCreateInfo,const std::vector<prosper::IBuffer*> &buffers,
	const std::vector<prosper::DeviceSize> &offsets,const std::optional<IndexBufferInfo> &indexBufferInfo
)
{
	return VlkRenderBuffer::Create(*this,pipelineCreateInfo,buffers,offsets,indexBufferInfo);
}
#pragma optimize("",on)

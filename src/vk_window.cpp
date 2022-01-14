/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <misc/types.h>
#include <vulkan/vulkan.hpp>
#include "vk_context.hpp"
#include "stdafx_prosper_vulkan.h"
#include "vk_window.hpp"
#include "vk_command_buffer.hpp"
#include "image/vk_image.hpp"
#include "image/vk_image_view.hpp"
#include "vk_framebuffer.hpp"
#include <wrappers/instance.h>
#include <wrappers/swapchain.h>
#include <wrappers/semaphore.h>
#include <wrappers/framebuffer.h>
#include <misc/semaphore_create_info.h>
#include <misc/framebuffer_create_info.h>
#include <misc/image_create_info.h>
#include <misc/swapchain_create_info.h>
#include <misc/fence_create_info.h>
#include <misc/rendering_surface_create_info.h>
#include <misc/window_factory.h>
#include <misc/window.h>
#include <misc/image_view_create_info.h>
#include <iglfw/glfw_window.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#else
#define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
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
#pragma optimize("",off)
std::shared_ptr<VlkWindow> prosper::VlkWindow::Create(const WindowSettings &windowCreationInfo,prosper::VlkContext &context)
{
	auto window = std::shared_ptr<VlkWindow>{new VlkWindow{context,windowCreationInfo},[](VlkWindow *window) {
		window->Release();
		delete window;
	}};
	window->InitWindow();
	return window;
}
prosper::VlkWindow::~VlkWindow()
{
	m_frameSignalSemaphores.clear();
	m_frameWaitSemaphores.clear();

	m_cmdFences.clear();
	m_swapchainPtr = nullptr;
	
	vkDestroySurfaceKHR(static_cast<VlkContext&>(GetContext()).GetAnvilInstance().get_instance_vk(),m_surface,nullptr);
	m_windowPtr = nullptr;
	m_renderingSurfacePtr = nullptr;
}

void prosper::VlkWindow::ReleaseWindow()
{
	ReleaseSwapchain();
	m_glfwWindow = nullptr;
	m_windowPtr = nullptr;
}

void prosper::VlkWindow::DoReleaseSwapchain()
{
	for(auto &fbo : m_swapchainFramebuffers)
		fbo.reset();
	m_swapchainPtr.reset();
	m_renderingSurfacePtr.reset();
	
	m_swapchainFramebuffers.clear();
	m_cmdFences.clear();
	m_frameSignalSemaphores.clear();
	m_frameWaitSemaphores.clear();
}

uint32_t prosper::VlkWindow::GetLastAcquiredSwapchainImageIndex() const
{
	return m_swapchainPtr ? GetSwapchain().get_last_acquired_image_index() : 0u;
}

Anvil::SwapchainOperationErrorCode prosper::VlkWindow::AcquireImage()
{
	/* Determine the signal + wait semaphores to use for drawing this frame */
	m_lastSemaporeUsed = (m_lastSemaporeUsed +1) %GetSwapchainImageCount();

	m_curFrameSignalSemaphore = m_frameSignalSemaphores[m_lastSemaporeUsed].get();
	m_curFrameWaitSemaphore = m_frameWaitSemaphores[m_lastSemaporeUsed].get();
	
	uint32_t idx;
	auto errCode = m_swapchainPtr->acquire_image(m_curFrameWaitSemaphore,&idx);
	if(errCode == Anvil::SwapchainOperationErrorCode::OUT_OF_DATE)
		InitSwapchain();
	return errCode;
}

Anvil::Semaphore &prosper::VlkWindow::Submit(VlkPrimaryCommandBuffer &cmd,Anvil::Semaphore *optWaitSemaphore)
{
	/* Submit work chunk and present */
	auto *signalSemaphore = m_curFrameSignalSemaphore;
	std::array<Anvil::Semaphore*,2> waitSemaphores = {m_curFrameWaitSemaphore,optWaitSemaphore};
	auto swapchainImgIdx = GetLastAcquiredSwapchainImageIndex();
	auto &context = static_cast<VlkContext&>(GetContext());
	auto &dev = context.GetDevice();
	const Anvil::PipelineStageFlags wait_stage_mask = Anvil::PipelineStageFlagBits::ALL_COMMANDS_BIT;
	dev.get_universal_queue(0)->submit(Anvil::SubmitInfo::create(
		&cmd.GetAnvilCommandBuffer(),
		1, /* n_semaphores_to_signal */
		&signalSemaphore,
		optWaitSemaphore ? 2 : 1, /* n_semaphores_to_wait_on */
		waitSemaphores.data(),
		&wait_stage_mask,
		false, /* should_block  */
		m_cmdFences.at(swapchainImgIdx).get()
	)); /* opt_fence_ptr */
	return *signalSemaphore;
}

void prosper::VlkWindow::Present(Anvil::Semaphore *optWaitSemaphore)
{
	auto &context = static_cast<VlkContext&>(GetContext());
	auto &dev = context.GetDevice();
	auto swapchainImgIdx = GetLastAcquiredSwapchainImageIndex();
	auto *present_queue_ptr = dev.get_universal_queue(0);
	auto errCode = Anvil::SwapchainOperationErrorCode::SUCCESS;
	auto bPresentSuccess = present_queue_ptr->present(
		m_swapchainPtr.get(),
		swapchainImgIdx,
		optWaitSemaphore ? 1 : 0, /* n_wait_semaphores */
		&optWaitSemaphore,
		&errCode
	);

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
}

void prosper::VlkWindow::InitWindow()
{
	auto &context = static_cast<VlkContext&>(GetContext());
#ifdef _WIN32
	const Anvil::WindowPlatform platform = Anvil::WINDOW_PLATFORM_SYSTEM;
#else
	const Anvil::WindowPlatform platform = Anvil::WINDOW_PLATFORM_XCB;
#endif
	/* Create a window */
	//m_windowPtr = Anvil::WindowFactory::create_window(platform,appName,width,height,true,std::bind(&Context::DrawFrame,this));

	// TODO: Clean this up
	try
	{
		m_glfwWindow = nullptr;
		GLFW::poll_events();
		auto settings = m_settings;
		if(!settings.windowedMode && (settings.monitor.has_value() == false || settings.monitor->GetGLFWMonitor() == nullptr))
			settings.monitor = GLFW::get_primary_monitor();
		m_glfwWindow = GLFW::Window::Create(settings); // TODO: Release
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
		m_settings.width = actualWindowSize.x;
		m_settings.height = actualWindowSize.y;

		m_windowPtr = Anvil::WindowFactory::create_window(platform,hWindow
#ifdef ENABLE_GLFW_ANVIL_COMPATIBILITY
			,pConnection
#endif
		);

		err = glfwCreateWindowSurface(
			context.GetAnvilInstance().get_instance_vk(),
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
	}
	catch(const std::exception &e)
	{

	}
	OnWindowInitialized();
}

void prosper::VlkWindow::InitFrameBuffers()
{
	auto &context = static_cast<VlkContext&>(GetContext());
	bool result;
	auto n = GetSwapchainImageCount();
	auto size = m_glfwWindow->GetSize();
	for(uint32_t n_swapchain_image=0;n_swapchain_image<n;++n_swapchain_image)
	{
		auto *anvImgView = m_swapchainPtr->get_image_view(n_swapchain_image);
		prosper::util::ImageViewCreateInfo imgViewCreateInfo {};
		imgViewCreateInfo.mipmapLevels = 1;
		imgViewCreateInfo.levelCount = 1;
		imgViewCreateInfo.format = static_cast<prosper::Format>(anvImgView->get_create_info_ptr()->get_format());
		auto imgView = VlkImageView::Create(
			context,*m_swapchainImages[n_swapchain_image],imgViewCreateInfo,prosper::ImageViewType::e2D,prosper::ImageAspectFlags::ColorBit,
			Anvil::ImageViewUniquePtr{anvImgView,[](Anvil::ImageView *imgView) {}} /* deletion handled by Anvil */
		);
		auto framebuffer = context.CreateFramebuffer(size.x,size.y,1u,{imgView.get()});
		framebuffer->SetDebugName("Swapchain framebuffer " +std::to_string(n_swapchain_image));
		m_swapchainFramebuffers[n_swapchain_image] = framebuffer;
	}
}

void prosper::VlkWindow::InitSemaphores()
{
	auto &context = static_cast<VlkContext&>(GetContext());
	auto &dev = context.GetDevice();
	auto n = GetSwapchainImageCount();
	for(auto n_semaphore=0u;n_semaphore < n;++n_semaphore)
	{
		auto new_signal_semaphore_ptr = Anvil::Semaphore::create(Anvil::SemaphoreCreateInfo::create(&dev));
		auto new_wait_semaphore_ptr = Anvil::Semaphore::create(Anvil::SemaphoreCreateInfo::create(&dev));

		new_signal_semaphore_ptr->set_name_formatted("Signal semaphore [%d]",n_semaphore);
		new_wait_semaphore_ptr->set_name_formatted("Wait semaphore [%d]",n_semaphore);

		m_frameSignalSemaphores.push_back(std::move(new_signal_semaphore_ptr));
		m_frameWaitSemaphores.push_back(std::move(new_wait_semaphore_ptr));
	}
}

void prosper::VlkWindow::DoInitSwapchain()
{
	auto &context = static_cast<VlkContext&>(GetContext());
	m_renderingSurfacePtr = Anvil::RenderingSurface::create(Anvil::RenderingSurfaceCreateInfo::create(&context.GetAnvilInstance(),&context.GetDevice(),m_windowPtr.get()));
	if(m_renderingSurfacePtr->get_width() == 0 || m_renderingSurfacePtr->get_height() == 0)
		return; // Minimized?

	m_renderingSurfacePtr->set_name("Main rendering surface");
	// TODO: This shouldn't be handled through the context, this could be problematic if there's
	// multiple windows with different present modes!
	context.SetPresentMode(context.GetPresentMode()); // Update present mode to make sure it's supported by our surface

	auto presentMode = context.GetPresentMode();
	uint32_t numSwapchainImages = 0;
	switch(presentMode)
	{
	case prosper::PresentModeKHR::Mailbox:
		numSwapchainImages = 3u;
		break;
	case prosper::PresentModeKHR::Fifo:
		numSwapchainImages = 2u;
		break;
	default:
		numSwapchainImages = 1u;
	}

	m_swapchainImages.clear();
	m_cmdFences.clear();
	m_swapchainFramebuffers.clear();

	auto createInfo = Anvil::SwapchainCreateInfo::create(
		&context.GetDevice(),
		m_renderingSurfacePtr.get(),m_windowPtr.get(),
		Anvil::Format::B8G8R8A8_UNORM,Anvil::ColorSpaceKHR::SRGB_NONLINEAR_KHR,static_cast<Anvil::PresentModeKHR>(presentMode),
		Anvil::ImageUsageFlagBits::COLOR_ATTACHMENT_BIT | Anvil::ImageUsageFlagBits::TRANSFER_DST_BIT,numSwapchainImages
	);
	createInfo->set_mt_safety(Anvil::MTSafety::ENABLED);

	auto recreateSwapchain = (m_swapchainPtr != nullptr);
	if(recreateSwapchain)
		createInfo->set_old_swapchain(m_swapchainPtr.get());
	m_swapchainPtr = Anvil::Swapchain::create(std::move(createInfo));

	// The actual swapchain may have a different number of images
	numSwapchainImages = m_swapchainPtr->get_n_images();
	m_cmdFences.resize(numSwapchainImages);
	m_swapchainFramebuffers.resize(numSwapchainImages);

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
		m_swapchainImages.at(i) = VlkImage::Create(context,std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>>{&img,[](Anvil::Image *img) {
			// Don't delete, image will be destroyed by Anvil
		}},createInfo,true);
		m_cmdFences[i] = Anvil::Fence::create(Anvil::FenceCreateInfo::create(&context.GetDevice(),true));
	}

	m_swapchainPtr->set_name("Main swapchain");

	/* Cache the queue we are going to use for presentation */
	const std::vector<uint32_t>* present_queue_fams_ptr = nullptr;

	if(!m_renderingSurfacePtr->get_queue_families_with_present_support(context.GetDevice().get_physical_device(),&present_queue_fams_ptr))
		anvil_assert_fail();

	m_presentQueuePtr = context.GetDevice().get_queue_for_queue_family_index(present_queue_fams_ptr->at(0),0);

	//if(recreateSwapchain)
	//	InitFrameBuffers();

	InitFrameBuffers();
	InitSemaphores();
}

Anvil::Fence *prosper::VlkWindow::GetFence(uint32_t idx) {return (idx < m_cmdFences.size()) ? m_cmdFences[idx].get() : nullptr;}

bool prosper::VlkWindow::WaitForFence(std::string &outErr)
{
	auto idx = GetLastAcquiredSwapchainImageIndex();
	if(idx == UINT32_MAX)
		return true; // Nothing to wait for
	auto &context = static_cast<VlkContext&>(GetContext());
	auto waitResult = static_cast<prosper::Result>(vkWaitForFences(context.GetDevice().get_device_vk(),1,m_cmdFences[idx]->get_fence_ptr(),true,std::numeric_limits<uint64_t>::max()));
	if(waitResult == prosper::Result::Success)
	{
		if(m_cmdFences[idx]->reset() == false)
		{
			outErr = "Unable to reset swapchain fence!";
			return false;
		}
	}
	else
	{
		outErr = "An error has occurred when waiting for swapchain fence: " +prosper::util::to_string(waitResult);
		return false;
	}
	return true;
}

bool prosper::VlkWindow::IsPresentationModeSupported(prosper::PresentModeKHR presentMode) const
{
	if(m_renderingSurfacePtr == nullptr)
		return true;
	auto res = false;
	auto &context = static_cast<VlkContext&>(GetContext());
	m_renderingSurfacePtr->supports_presentation_mode(context.GetDevice().get_physical_device(),static_cast<Anvil::PresentModeKHR>(presentMode),&res);
	return res;
}
#pragma optimize("",on)

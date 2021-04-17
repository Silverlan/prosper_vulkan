/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __VK_WINDOW_HPP__
#define __VK_WINDOW_HPP__

#include "prosper_vulkan_definitions.hpp"
#include <prosper_window.hpp>
#include <wrappers/event.h>

namespace prosper
{
	class VlkContext;
	class VlkPrimaryCommandBuffer;
	enum class PresentModeKHR : uint32_t;
	class DLLPROSPER_VK VlkWindow
		: public Window
	{
	public:
		static std::shared_ptr<VlkWindow> Create(const WindowSettings &windowCreationInfo,prosper::VlkContext &context);
		virtual ~VlkWindow() override;

		Anvil::Swapchain &GetSwapchain() {return *m_swapchainPtr;}
		const Anvil::Swapchain &GetSwapchain() const {return const_cast<VlkWindow*>(this)->GetSwapchain();}
		Anvil::RenderingSurface &GetRenderingSurface() {return *m_renderingSurfacePtr;}
		const Anvil::RenderingSurface &GetRenderingSurface() const {return const_cast<VlkWindow*>(this)->GetRenderingSurface();}

		Anvil::Semaphore *GetCurrentFrameSignalSemaphore() {return m_curFrameSignalSemaphore;}
		Anvil::Semaphore *GetCurrentFrameWaitSemaphore() {return m_curFrameWaitSemaphore;}
		
		Anvil::Fence *GetFence(uint32_t idx);
		bool WaitForFence(std::string &outErr);
		bool IsPresentationModeSupported(prosper::PresentModeKHR presentMode) const;
		virtual uint32_t GetLastAcquiredSwapchainImageIndex() const override;
		Anvil::SwapchainOperationErrorCode AcquireImage();
		Anvil::Semaphore &Submit(VlkPrimaryCommandBuffer &cmd,Anvil::Semaphore *optWaitSemaphore=nullptr);
		void Present(Anvil::Semaphore *optWaitSemaphore=nullptr);
	protected:
		using Window::Window;
		virtual void InitWindow() override;
		virtual void ReleaseWindow() override;
		virtual void InitSwapchain() override;
		virtual void ReleaseSwapchain() override;
		void InitSemaphores();
		void InitFrameBuffers();

		Anvil::Semaphore *m_curFrameSignalSemaphore = nullptr;
		Anvil::Semaphore *m_curFrameWaitSemaphore = nullptr;

		std::shared_ptr<Anvil::RenderingSurface> m_renderingSurfacePtr;
		Anvil::WindowUniquePtr m_windowPtr = nullptr;
		VkSurfaceKHR_T *m_surface = nullptr;

		std::vector<std::shared_ptr<Anvil::Framebuffer>> m_fbos;
		Anvil::Queue *m_presentQueuePtr = nullptr;
		std::shared_ptr<Anvil::Swapchain> m_swapchainPtr;
		std::vector<std::shared_ptr<Anvil::Fence>> m_cmdFences;

		std::vector<Anvil::SemaphoreUniquePtr> m_frameSignalSemaphores;
		std::vector<Anvil::SemaphoreUniquePtr> m_frameWaitSemaphores;
	};
};

#endif

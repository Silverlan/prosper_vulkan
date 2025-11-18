// SPDX-FileCopyrightText: (c) 2021 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

module;

#include <wrappers/event.h>

export module pragma.prosper.vulkan:window;

export import pragma.prosper;

export namespace prosper {
	class VlkPrimaryCommandBuffer;
	class VlkContext;
	class PR_EXPORT VlkWindow : public Window {
	  public:
		static std::shared_ptr<VlkWindow> Create(const WindowSettings &windowCreationInfo, prosper::VlkContext &context);
		virtual ~VlkWindow() override;

		Anvil::Swapchain &GetSwapchain() { return *m_swapchainPtr; }
		const Anvil::Swapchain &GetSwapchain() const { return const_cast<VlkWindow *>(this)->GetSwapchain(); }
		Anvil::RenderingSurface &GetRenderingSurface() { return *m_renderingSurfacePtr; }
		const Anvil::RenderingSurface &GetRenderingSurface() const { return const_cast<VlkWindow *>(this)->GetRenderingSurface(); }

		Anvil::Semaphore *GetCurrentFrameSignalSemaphore() { return m_curFrameSignalSemaphore; }
		Anvil::Semaphore *GetCurrentFrameWaitSemaphore() { return m_curFrameWaitSemaphore; }

		Anvil::Fence *GetFence(uint32_t idx);
		bool WaitForFence(std::string &outErr);
		bool IsPresentationModeSupported(prosper::PresentModeKHR presentMode) const;
		virtual uint32_t GetLastAcquiredSwapchainImageIndex() const override;
		Anvil::SwapchainOperationErrorCode AcquireImage();
		Anvil::Semaphore &Submit(VlkPrimaryCommandBuffer &cmd, Anvil::Semaphore *optWaitSemaphore = nullptr);
		void Present(Anvil::Semaphore *optWaitSemaphore = nullptr);
		bool UpdateSwapchain();
	  protected:
		using Window::Window;
		void ClearSwapchain();
		void ResetSwapchain();
		virtual void InitWindow() override;
		virtual void ReleaseWindow() override;
		virtual void DoInitSwapchain() override;
		virtual void DoReleaseSwapchain() override;
		virtual void InitCommandBuffers() override;
		void InitSemaphores();
		void InitFrameBuffers();

		bool m_initializeSwapchainWhenPossible = false;
		Anvil::Semaphore *m_curFrameSignalSemaphore = nullptr;
		Anvil::Semaphore *m_curFrameWaitSemaphore = nullptr;

		std::shared_ptr<Anvil::RenderingSurface> m_renderingSurfacePtr;
		Anvil::WindowUniquePtr m_windowPtr = nullptr;
		VkSurfaceKHR_T *m_surface = nullptr;

		Anvil::Queue *m_presentQueuePtr = nullptr;
		std::shared_ptr<Anvil::Swapchain> m_swapchainPtr;
		std::vector<std::shared_ptr<Anvil::Fence>> m_cmdFences;

		std::vector<Anvil::SemaphoreUniquePtr> m_frameSignalSemaphores;
		std::vector<Anvil::SemaphoreUniquePtr> m_frameWaitSemaphores;
	};
};

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
		static std::expected<std::shared_ptr<VlkWindow>, std::string> Create(const WindowSettings &windowCreationInfo, VlkContext &context);
		~VlkWindow() override;

		Anvil::Swapchain &GetSwapchain() { return *m_swapchainPtr; }
		const Anvil::Swapchain &GetSwapchain() const { return const_cast<VlkWindow *>(this)->GetSwapchain(); }
		Anvil::RenderingSurface &GetRenderingSurface() { return *m_renderingSurfacePtr; }
		const Anvil::RenderingSurface &GetRenderingSurface() const { return const_cast<VlkWindow *>(this)->GetRenderingSurface(); }

		Anvil::Fence *GetFence(uint32_t idx);
		bool WaitForFence(std::string &outErr);
		bool IsPresentationModeSupported(PresentModeKHR presentMode) const;
		uint32_t GetLastAcquiredSwapchainImageIndex() const override;
		Anvil::SwapchainOperationErrorCode AcquireImage();
		Anvil::Semaphore &Submit(VlkPrimaryCommandBuffer &cmd, Anvil::Semaphore *optWaitSemaphore = nullptr);
		void Present(Anvil::Semaphore *optWaitSemaphore = nullptr);
		bool UpdateSwapchain();
	  protected:
		using Window::Window;
		void ClearSwapchain();
		void ResetSwapchain();
		void ClearCurrentDrawCmdBuffer();
		std::expected<void, std::string> InitWindow() override;
		void ReleaseWindow() override;
		void DoInitSwapchain() override;
		void DoReleaseSwapchain() override;
		void InitCommandBuffers() override;
		void InitSemaphores();
		void InitFrameBuffers();

		bool m_initializeSwapchainWhenPossible = false;

		std::shared_ptr<Anvil::RenderingSurface> m_renderingSurfacePtr;
		Anvil::WindowUniquePtr m_windowPtr = nullptr;
		VkSurfaceKHR_T *m_surface = nullptr;

		Anvil::Queue *m_presentQueuePtr = nullptr;
		std::shared_ptr<Anvil::Swapchain> m_swapchainPtr;
		std::vector<std::shared_ptr<Anvil::Fence>> m_cmdFences;

		Anvil::Semaphore *m_curRenderFinishedSemaphore = nullptr;
		Anvil::Semaphore *m_presentCompleteSemaphore = nullptr;
		std::vector<Anvil::SemaphoreUniquePtr> m_renderFinishedSemaphores;
		std::vector<Anvil::SemaphoreUniquePtr> m_presentCompleteSemaphores;
	};
};

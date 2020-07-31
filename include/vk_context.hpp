/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_CONTEXT_HPP__
#define __PR_PROSPER_VK_CONTEXT_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "prosper_context.hpp"

namespace Anvil
{
	class BaseDevice;
	class SGPUDevice;
	class RenderPass;
	class Swapchain;
	class Instance;
	struct SurfaceCapabilities;
	struct MipmapRawData;
	class RenderPassCreateInfo;
	class DescriptorSetCreateInfo;
	class PipelineLayout;
	class PhysicalDevice;
	class Queue;
	class RenderingSurface;
	class Window;
	class Fence;
	class Framebuffer;
	class Semaphore;
	struct MemoryType;

	typedef std::unique_ptr<BaseDevice,std::function<void(BaseDevice*)>> BaseDeviceUniquePtr;
	typedef std::unique_ptr<Instance,std::function<void(Instance*)>> InstanceUniquePtr;
	typedef std::unique_ptr<Window,std::function<void(Window*)>> WindowUniquePtr;
	typedef std::unique_ptr<Semaphore,std::function<void(Semaphore*)>> SemaphoreUniquePtr;
};

struct VkSurfaceKHR_T;
namespace prosper
{
	class DLLPROSPER_VK VlkShaderStageProgram
		: public prosper::ShaderStageProgram
	{
	public:
		VlkShaderStageProgram(std::vector<unsigned int> &&spirvBlob);
		const std::vector<unsigned int> &GetSPIRVBlob() const;
	private:
		std::vector<unsigned int> m_spirvBlob;
	};

	class DLLPROSPER_VK VlkContext
		: public IPrContext
	{
	public:
		static std::shared_ptr<VlkContext> Create(const std::string &appName,bool bEnableValidation);
		static IPrContext &GetContext(Anvil::BaseDevice &dev);
		virtual std::string GetAPIIdentifier() const override {return "Vulkan";}

		Anvil::SGPUDevice &GetDevice();
		const Anvil::SGPUDevice &GetDevice() const;
		const std::shared_ptr<Anvil::RenderPass> &GetMainRenderPass() const;
		SubPassID GetMainSubPassID() const;
		std::shared_ptr<Anvil::Swapchain> GetSwapchain();

		const Anvil::Instance &GetAnvilInstance() const;
		Anvil::Instance &GetAnvilInstance();

		bool GetSurfaceCapabilities(Anvil::SurfaceCapabilities &caps) const;
		virtual void ReloadWindow() override;
		virtual Vendor GetPhysicalDeviceVendor() const override;
		virtual bool IsPresentationModeSupported(prosper::PresentModeKHR presentMode) const override;
		virtual MemoryRequirements GetMemoryRequirements(IImage &img) override;
		virtual uint64_t ClampDeviceMemorySize(uint64_t size,float percentageOfGPUMemory,MemoryFeatureFlags featureFlags) const override;
		virtual DeviceSize CalcBufferAlignment(BufferUsageFlags usageFlags) override;

		virtual void GetGLSLDefinitions(glsl::Definitions &outDef) const override;

		using IPrContext::CreateImage;
		std::shared_ptr<IImage> CreateImage(const util::ImageCreateInfo &createInfo,const std::vector<Anvil::MipmapRawData> &data);
		using IPrContext::CreateRenderPass;
		std::shared_ptr<IRenderPass> CreateRenderPass(const prosper::util::RenderPassCreateInfo &renderPassInfo,std::unique_ptr<Anvil::RenderPassCreateInfo> anvRenderPassInfo);
		using IPrContext::CreateDescriptorSetGroup;
		std::shared_ptr<IDescriptorSetGroup> CreateDescriptorSetGroup(const DescriptorSetCreateInfo &descSetCreateInfo,std::unique_ptr<Anvil::DescriptorSetCreateInfo> descSetInfo);
		virtual std::shared_ptr<IDescriptorSetGroup> CreateDescriptorSetGroup(DescriptorSetCreateInfo &descSetInfo) override;

		virtual bool IsImageFormatSupported(
			prosper::Format format,prosper::ImageUsageFlags usageFlags,prosper::ImageType type=prosper::ImageType::e2D,
			prosper::ImageTiling tiling=prosper::ImageTiling::Optimal
		) const override;
		virtual uint32_t GetUniversalQueueFamilyIndex() const override;
		virtual util::Limits GetPhysicalDeviceLimits() const override;
		virtual std::optional<util::PhysicalDeviceImageFormatProperties> GetPhysicalDeviceImageFormatProperties(const ImageFormatPropertiesQuery &query) override;

		virtual std::shared_ptr<prosper::IPrimaryCommandBuffer> AllocatePrimaryLevelCommandBuffer(prosper::QueueFamilyType queueFamilyType,uint32_t &universalQueueFamilyIndex) override;
		virtual std::shared_ptr<prosper::ISecondaryCommandBuffer> AllocateSecondaryLevelCommandBuffer(prosper::QueueFamilyType queueFamilyType,uint32_t &universalQueueFamilyIndex) override;
		virtual bool SavePipelineCache() override;

		virtual void Flush() override;
		virtual Result WaitForFence(const IFence &fence,uint64_t timeout=std::numeric_limits<uint64_t>::max()) const override;
		virtual Result WaitForFences(const std::vector<IFence*> &fences,bool waitAll=true,uint64_t timeout=std::numeric_limits<uint64_t>::max()) const override;
		virtual void DrawFrame(const std::function<void(const std::shared_ptr<prosper::IPrimaryCommandBuffer>&,uint32_t)> &drawFrame) override;
		virtual bool Submit(ICommandBuffer &cmdBuf,bool shouldBlock=false,IFence *optFence=nullptr) override;
		virtual void SubmitCommandBuffer(prosper::ICommandBuffer &cmd,prosper::QueueFamilyType queueFamilyType,bool shouldBlock=false,prosper::IFence *fence=nullptr) override;
		using IPrContext::SubmitCommandBuffer;

		virtual std::shared_ptr<IBuffer> CreateBuffer(const util::BufferCreateInfo &createInfo,const void *data=nullptr) override;
		virtual std::shared_ptr<IDynamicResizableBuffer> CreateDynamicResizableBuffer(
			util::BufferCreateInfo createInfo,
			uint64_t maxTotalSize,float clampSizeToAvailableGPUMemoryPercentage=1.f,const void *data=nullptr
		) override;
		virtual std::shared_ptr<IEvent> CreateEvent() override;
		virtual std::shared_ptr<IFence> CreateFence(bool createSignalled=false) override;
		virtual std::shared_ptr<ISampler> CreateSampler(const util::SamplerCreateInfo &createInfo) override;
		virtual std::shared_ptr<IImage> CreateImage(const util::ImageCreateInfo &createInfo,const ImageData &imgData={}) override;
		virtual std::shared_ptr<IRenderPass> CreateRenderPass(const util::RenderPassCreateInfo &renderPassInfo) override;
		virtual std::shared_ptr<IFramebuffer> CreateFramebuffer(uint32_t width,uint32_t height,uint32_t layers,const std::vector<prosper::IImageView*> &attachments) override;
		virtual std::unique_ptr<ShaderModule> CreateShaderModuleFromStageData(
			const std::shared_ptr<ShaderStageProgram> &shaderStageProgram,
			prosper::ShaderStage stage,
			const std::string &entrypointName="main"
		) override;
		virtual std::shared_ptr<ShaderStageProgram> CompileShader(prosper::ShaderStage stage,const std::string &shaderPath,std::string &outInfoLog,std::string &outDebugInfoLog,bool reload=false) override;
		virtual std::optional<PipelineID> AddPipeline(
			prosper::Shader &shader,PipelineID shaderPipelineId,const prosper::ComputePipelineCreateInfo &createInfo,
			prosper::ShaderStageData &stage,PipelineID basePipelineId=std::numeric_limits<PipelineID>::max()
		) override;
		virtual std::optional<PipelineID> AddPipeline(
			prosper::Shader &shader,PipelineID shaderPipelineId,
			const prosper::GraphicsPipelineCreateInfo &createInfo,IRenderPass &rp,
			prosper::ShaderStageData *shaderStageFs=nullptr,
			prosper::ShaderStageData *shaderStageVs=nullptr,
			prosper::ShaderStageData *shaderStageGs=nullptr,
			prosper::ShaderStageData *shaderStageTc=nullptr,
			prosper::ShaderStageData *shaderStageTe=nullptr,
			SubPassID subPassId=0,
			PipelineID basePipelineId=std::numeric_limits<PipelineID>::max()
		) override;
		virtual bool ClearPipeline(bool graphicsShader,PipelineID pipelineId) override;
		virtual uint32_t GetLastAcquiredSwapchainImageIndex() const override;

		virtual std::shared_ptr<prosper::IQueryPool> CreateQueryPool(QueryType queryType,uint32_t maxConcurrentQueries) override;
		virtual std::shared_ptr<prosper::IQueryPool> CreateQueryPool(QueryPipelineStatisticFlags statsFlags,uint32_t maxConcurrentQueries) override;
		virtual bool QueryResult(const TimestampQuery &query,std::chrono::nanoseconds &outTimestampValue) const override;
		virtual bool QueryResult(const PipelineStatisticsQuery &query,PipelineStatistics &outStatistics) const override;
		virtual bool QueryResult(const Query &query,uint32_t &r) const override;
		virtual bool QueryResult(const Query &query,uint64_t &r) const override;
		virtual void AddDebugObjectInformation(std::string &msgValidation) override;

		std::pair<const Anvil::MemoryType*,prosper::MemoryFeatureFlags> FindCompatibleMemoryType(MemoryFeatureFlags featureFlags) const;

		Anvil::PipelineLayout *GetPipelineLayout(bool graphicsShader,PipelineID pipelineId);
		virtual void *GetInternalDevice() const override;
		virtual void *GetInternalPhysicalDevice() const override;
		virtual void *GetInternalInstance() const override;
		virtual void *GetInternalUniversalQueue() const override;
		virtual bool IsDeviceExtensionEnabled(const std::string &ext) const override;
		virtual bool IsInstanceExtensionEnabled(const std::string &ext) const override;
	protected:
		VlkContext(const std::string &appName,bool bEnableValidation=false);
		virtual void Release() override;

		template<class T,typename TBaseType=T>
			bool QueryResult(const Query &query,T &outResult,QueryResultFlags resultFlags) const;

		virtual std::shared_ptr<IImageView> DoCreateImageView(
			const util::ImageViewCreateInfo &createInfo,IImage &img,Format format,ImageViewType imgViewType,prosper::ImageAspectFlags aspectMask,uint32_t numLayers
		) override;
		virtual void DoKeepResourceAliveUntilPresentationComplete(const std::shared_ptr<void> &resource) override;
		virtual void DoWaitIdle() override;
		virtual void DoFlushSetupCommandBuffer() override;
		virtual std::shared_ptr<IUniformResizableBuffer> DoCreateUniformResizableBuffer(
			const util::BufferCreateInfo &createInfo,uint64_t bufferInstanceSize,
			uint64_t maxTotalSize,const void *data,
			prosper::DeviceSize bufferBaseSize,uint32_t alignment
		) override;
		void InitCommandBuffers();
		void InitFrameBuffers();
		void InitSemaphores();
		void InitSwapchain();
		void InitWindow();
		void InitVulkan(const CreateInfo &createInfo);
		void InitMainRenderPass();
		virtual void ReloadSwapchain() override;
		virtual void InitAPI(const CreateInfo &createInfo) override;

		Anvil::BaseDeviceUniquePtr m_devicePtr = nullptr;
		Anvil::SGPUDevice *m_pGpuDevice = nullptr;
		Anvil::InstanceUniquePtr m_instancePtr = nullptr;
		const Anvil::PhysicalDevice *m_physicalDevicePtr = nullptr;
		Anvil::Queue *m_presentQueuePtr = nullptr;
		std::shared_ptr<Anvil::RenderingSurface> m_renderingSurfacePtr;
		std::shared_ptr<Anvil::Swapchain> m_swapchainPtr;
		Anvil::WindowUniquePtr m_windowPtr = nullptr;
		std::shared_ptr<Anvil::RenderPass> m_renderPass;
		SubPassID m_mainSubPass = std::numeric_limits<SubPassID>::max();
		VkSurfaceKHR_T *m_surface = nullptr;

		std::vector<std::shared_ptr<Anvil::Fence>> m_cmdFences;
		std::vector<std::shared_ptr<Anvil::Framebuffer>> m_fbos;

		std::vector<Anvil::SemaphoreUniquePtr> m_frameSignalSemaphores;
		std::vector<Anvil::SemaphoreUniquePtr> m_frameWaitSemaphores;
	private:
		void ReleaseSwapchain();
		bool GetUniversalQueueFamilyIndex(prosper::QueueFamilyType queueFamilyType,uint32_t &queueFamilyIndex) const;
		static std::unordered_map<Anvil::BaseDevice*,IPrContext*> s_devToContext;
	};
};

#endif

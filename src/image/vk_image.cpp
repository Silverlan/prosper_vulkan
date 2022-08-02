/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prosper_vulkan_definitions.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>
#include "stdafx_prosper_vulkan.h"
#include "image/vk_image.hpp"
#include "buffers/vk_buffer.hpp"
#include "debug/vk_debug_lookup_map.hpp"
#include "vk_memory_tracker.hpp"
#include "debug/vk_debug.hpp"
#include <wrappers/image.h>
#include <wrappers/memory_block.h>
#include <misc/image_create_info.h>

using namespace prosper;

static_assert(sizeof(prosper::Extent2D) == sizeof(vk::Extent2D));

static std::unique_ptr<std::unordered_map<Anvil::Image*,VlkImage*>> s_imageMap = nullptr;
static std::mutex s_imageMapMutex;
VlkImage *prosper::debug::get_image_from_anvil_image(Anvil::Image &img)
{
	std::scoped_lock lock {s_imageMapMutex};
	if(s_imageMap == nullptr)
		return nullptr;
	auto it = s_imageMap->find(&img);
	return (it != s_imageMap->end()) ? it->second : nullptr;
}
std::shared_ptr<VlkImage> VlkImage::Create(
	IPrContext &context,std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>> img,const prosper::util::ImageCreateInfo &createInfo,bool isSwapchainImage,
	const std::function<void(VlkImage&)> &onDestroyedCallback
)
{
	if(img == nullptr)
		return nullptr;
	return std::shared_ptr<VlkImage>(new VlkImage{context,std::move(img),createInfo,isSwapchainImage},[onDestroyedCallback](VlkImage *img) {
		img->OnRelease();
		if(onDestroyedCallback != nullptr)
			onDestroyedCallback(*img);
		img->GetContext().ReleaseResource<VlkImage>(img);
		});
}
static std::mutex g_memAllocMutex {};
std::mutex &get_mem_alloc_mutex() {return g_memAllocMutex;}
VlkImage::VlkImage(IPrContext &context,std::unique_ptr<Anvil::Image,std::function<void(Anvil::Image*)>> img,const prosper::util::ImageCreateInfo &createInfo,bool isSwapchainImage)
	: IImage{context,createInfo},m_image{std::move(img)},m_swapchainImage{isSwapchainImage}
{
	if(m_swapchainImage)
		return;
	g_memAllocMutex.lock();
		m_image->get_image(); // This will invoke memory allocation and is not thread-safe!
	g_memAllocMutex.unlock();

	if(prosper::debug::is_debug_mode_enabled())
	{
		s_imageMapMutex.lock();
		if(s_imageMap == nullptr)
			s_imageMap = std::make_unique<std::unordered_map<Anvil::Image*,VlkImage*>>();
		(*s_imageMap)[m_image.get()] = this;
		s_imageMapMutex.unlock();
	}
	prosper::debug::register_debug_object(m_image->get_image(),*this,prosper::debug::ObjectType::Image);
	MemoryTracker::GetInstance().AddResource(*this);
}
VlkImage::~VlkImage()
{
	if(m_swapchainImage)
		return;
	if(s_imageMap != nullptr)
	{
		s_imageMapMutex.lock();
		auto it = s_imageMap->find(m_image.get());
		if(it != s_imageMap->end())
			s_imageMap->erase(it);
		s_imageMapMutex.unlock();
	}
	prosper::debug::deregister_debug_object(m_image->get_image());
	MemoryTracker::GetInstance().RemoveResource(*this);
}

void VlkImage::Bake()
{
	GetAnvilImage().get_image();
}

DeviceSize VlkImage::GetAlignment() const {return m_image->get_image_alignment(0);}

std::optional<size_t> VlkImage::GetStorageSize() const
{
	if(!m_image)
		return {};
	return m_image->get_image_storage_size(0u);
}

bool VlkImage::Map(DeviceSize offset,DeviceSize size,void **outPtr)
{
	return m_image->get_memory_block()->map(offset,size,outPtr);
}

bool VlkImage::Unmap()
{
	return m_image->get_memory_block()->unmap();
}

const void *VlkImage::GetInternalHandle() const {return m_image->get_image();}

std::optional<prosper::util::SubresourceLayout> VlkImage::GetSubresourceLayout(uint32_t layerId,uint32_t mipMapIdx)
{
	static_assert(sizeof(prosper::util::SubresourceLayout) == sizeof(Anvil::SubresourceLayout));
	Anvil::SubresourceLayout subresourceLayout;
	if((*this)->get_aspect_subresource_layout(Anvil::ImageAspectFlagBits::COLOR_BIT,layerId,mipMapIdx,&subresourceLayout) == false)
		return std::optional<prosper::util::SubresourceLayout>{};
	return reinterpret_cast<prosper::util::SubresourceLayout&>(subresourceLayout);
}

bool VlkImage::WriteImageData(uint32_t x,uint32_t y,uint32_t w,uint32_t h,uint32_t layerIndex,uint32_t mipLevel,uint64_t size,const uint8_t *data)
{
	auto layout = GetSubresourceLayout(layerIndex,mipLevel);
	if(layout.has_value() == false)
		return false;
	void *ptr;
	auto offset = layout->offset +y *layout->row_pitch +x *prosper::util::get_byte_size(GetFormat());
	if(Map(offset,layout->size,&ptr) == false)
		return false;
	uint64_t srcOffset = 0;
	uint64_t dstOffset = 0;
	auto srcSizePerRow = size /h;
	for(auto yc=y;yc<(y +h);++yc)
	{
		memcpy(static_cast<uint8_t*>(ptr) +dstOffset,data +srcOffset,srcSizePerRow);
		
		srcOffset += srcSizePerRow;
		dstOffset += layout->row_pitch;
	}
	return Unmap();
}

bool VlkImage::DoSetMemoryBuffer(prosper::IBuffer &buffer)
{
	return m_image->set_memory(buffer.GetAPITypeRef<VlkBuffer>().GetAnvilBuffer().get_memory_block(0));
}

Anvil::Image &VlkImage::GetAnvilImage() const {return *m_image;}
Anvil::Image &VlkImage::operator*() {return *m_image;}
const Anvil::Image &VlkImage::operator*() const {return const_cast<VlkImage*>(this)->operator*();}
Anvil::Image *VlkImage::operator->() {return m_image.get();}
const Anvil::Image *VlkImage::operator->() const {return const_cast<VlkImage*>(this)->operator->();}

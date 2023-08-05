/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "stdafx_prosper_vulkan.h"
#include "vk_pipeline_cache.hpp"
#include <wrappers/pipeline_cache.h>
#include <wrappers/device.h>
#include <fsys/filesystem.h>

using namespace prosper;

Anvil::PipelineCacheUniquePtr PipelineCache::Create(Anvil::BaseDevice &dev)
{
	auto cache = Anvil::PipelineCache::create(&dev, false);
	if(cache == nullptr)
		return nullptr;
	return std::move(cache);
}

Anvil::PipelineCacheUniquePtr PipelineCache::Load(Anvil::BaseDevice &dev, const std::string &fileName, LoadError &outErr)
{
	auto f = FileManager::OpenFile(fileName.c_str(), "rb");
	if(f == nullptr) {
		outErr = LoadError::FileNotFound;
		return nullptr;
	}
	auto header = f->Read<Header>();
	if(header.size != sizeof(Header)) {
		outErr = LoadError::InvalidFormat;
		return nullptr;
	}
	auto szData = f->GetSize() - f->Tell();
	std::vector<uint8_t> data;
	data.resize(szData);
	f->Read(data.data(), data.size() * sizeof(data.front()));

	// static_assert(umath::to_integral(vk::PipelineCacheHeaderVersion::eOne) == VkPipelineCacheHeaderVersion::VK_PIPELINE_CACHE_HEADER_VERSION_END_RANGE,"Unsupported pipeline cache header version, please update header information! (See https://vulkan.lunarg.com/doc/view/1.0.26.0/linux/vkspec.chunked/ch09s06.html , table 9.1)");
	if(header.version != vk::PipelineCacheHeaderVersion::eOne) {
		outErr = LoadError::UnsupportedCacheVersion;
		return nullptr;
	}

	auto &properties = dev.get_physical_device_properties();
	if(header.vendorId != properties.core_vk1_0_properties_ptr->vendor_id) {
		outErr = LoadError::IncompatibleVendor;
		return nullptr;
	}

	if(header.deviceId != properties.core_vk1_0_properties_ptr->device_id) {
		outErr = LoadError::IncompatibleDevice;
		return nullptr;
	}

	if(sizeof(header.uuid) != sizeof(properties.core_vk1_0_properties_ptr->pipeline_cache_uuid)) {
		outErr = LoadError::IncompatiblePipelineCacheId;
		return nullptr;
	}

	for(auto i = decltype(header.uuid.size()) {0u}; i < header.uuid.size(); ++i) {
		if(header.uuid.at(i) != properties.core_vk1_0_properties_ptr->pipeline_cache_uuid[i]) {
			outErr = LoadError::IncompatiblePipelineCacheId;
			return nullptr;
		}
	}

	auto cache = Anvil::PipelineCache::create(&dev, false, data.size(), data.data());
	if(cache == nullptr)
		return nullptr;
	return std::move(cache);
}

bool PipelineCache::Save(Anvil::PipelineCache &cache, const std::string &fileName)
{
	auto f = FileManager::OpenFile<VFilePtrReal>(fileName.c_str(), "wb");
	if(f == nullptr)
		return false;
	size_t cacheSize {0ull};
	if(cache.get_data(&cacheSize, nullptr) == false || cacheSize == 0ull)
		return false;
	std::vector<uint8_t> data;
	data.resize(cacheSize);
	if(cache.get_data(&cacheSize, data.data()) == false)
		return false;
	f->Write(data.data(), cacheSize);
	return true;
}

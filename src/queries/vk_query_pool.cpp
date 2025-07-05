// SPDX-FileCopyrightText: (c) 2020 Silverlan <opensource@pragma-engine.com>
// SPDX-License-Identifier: MIT

#include "stdafx_prosper_vulkan.h"
#include "queries/vk_query_pool.hpp"
#include "vk_context.hpp"
#include <wrappers/device.h>

using namespace prosper;

VlkQueryPool::VlkQueryPool(IPrContext &context, Anvil::QueryPoolUniquePtr queryPool, QueryType type) : IQueryPool {context, type, queryPool->get_capacity()}, m_queryPool {std::move(queryPool)} {}
Anvil::QueryPool &VlkQueryPool::GetAnvilQueryPool() const { return *m_queryPool; }

bool VlkQueryPool::RequestQuery(uint32_t &queryId, QueryType type)
{
	auto *features = static_cast<VlkContext &>(GetContext()).GetDevice().get_physical_device_features().core_vk1_0_features_ptr;
	switch(type) {
	case QueryType::Occlusion:
		break;
	case QueryType::PipelineStatistics:
		if(features->pipeline_statistics_query == false)
			return false;
		break;
	case QueryType::Timestamp:
		break;
	}
	return IQueryPool::RequestQuery(queryId, type);
}

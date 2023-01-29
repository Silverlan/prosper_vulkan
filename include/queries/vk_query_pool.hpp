/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __PR_PROSPER_VK_QUERY_POOL_HPP__
#define __PR_PROSPER_VK_QUERY_POOL_HPP__

#include "prosper_vulkan_definitions.hpp"
#include "queries/prosper_query_pool.hpp"
#include <wrappers/query_pool.h>

namespace prosper {
	class VlkContext;
	class DLLPROSPER_VK VlkQueryPool : public IQueryPool {
	  public:
		virtual bool RequestQuery(uint32_t &queryId, QueryType type) override;
		Anvil::QueryPool &GetAnvilQueryPool() const;
	  protected:
		friend VlkContext;
		VlkQueryPool(IPrContext &context, std::unique_ptr<Anvil::QueryPool, std::function<void(Anvil::QueryPool *)>> queryPool, QueryType type);
		std::unique_ptr<Anvil::QueryPool, std::function<void(Anvil::QueryPool *)>> m_queryPool = nullptr;

		QueryType m_type = {};
		uint32_t m_queryCount = 0u;
		uint32_t m_nextQueryId = 0u;
		std::queue<uint32_t> m_freeQueries;
	};
};

#endif

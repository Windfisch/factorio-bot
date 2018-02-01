#include "scheduler.hpp"
#include "inventory.hpp"
#include "util.hpp"
#include <memory>
#include <algorithm>
#include <numeric>
#include <assert.h>

using namespace sched;

void TaggedAmount::cleanup()
{
	for (auto it = claims.begin(); it != claims.end(); )
	{
		if (it->owner.expired())
			it = unordered_erase(claims, it);
		else
			it++;
	}
}

size_t TaggedAmount::n_claimed() const
{
	size_t claimed = 0;
	for (const auto& claim : claims)
		if (!claim.owner.expired())
			claimed += claim.amount;
	return claimed;
}

size_t TaggedAmount::claim(const std::shared_ptr<Task>& task, size_t n)
{
	size_t available = amount - n_claimed();
	
	if (available < n)
	{
		// try to steal from lower-priority tasks

		std::vector<size_t> sorted_claims(claims.size());
		std::iota(sorted_claims.begin(), sorted_claims.end(), 0);

		std::sort(sorted_claims.begin(), sorted_claims.end(),
			[this](size_t a, size_t b) -> bool {
				auto aa = claims[a].owner.lock();
				auto bb = claims[b].owner.lock();

				if (!aa && !bb)
					return false;
				if (!aa && bb)
					return true;
				if (aa && !bb)
					return false;
				return aa->priority() > bb->priority();
		});
		
		for (auto it = sorted_claims.begin(); it != sorted_claims.end() && available < n; it++)
		{
			auto& tag = claims[*it];
			auto owner = tag.owner.lock();
			
			if (!owner || owner->priority() > task->priority())
			{
				// we may steal from a task with lower priority
				// (i.e., a higher priority value)


				const size_t needed = n - available;
				if (needed > tag.amount)
				{
					available += tag.amount;
					tag.amount = 0;
				}
				else
				{
					tag.amount -= needed;
					available += needed;
				}
			}
		}
	}

	claims.get(task).amount += std::min(available, n);

	return std::min(available, n);
}

/*
 * Copyright (c) 2017, 2018 Florian Jung
 *
 * This file is part of factorio-bot.
 *
 * factorio-bot is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 3, as published by the Free Software Foundation.
 *
 * factorio-bot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with factorio-bot. If not, see <http://www.gnu.org/licenses/>.
 */

#include "scheduler.hpp"
#include "inventory.hpp"
#include "util.hpp"
#include <memory>
#include <algorithm>
#include <numeric>
#include <assert.h>
#include <iostream>

using namespace sched;

bool Inventory::apply(const Recipe* recipe, bool already_started)
{
	Inventory& self = *this;

	if (!already_started)
	{
		for (const auto& ingredient : recipe->ingredients)
		{
			if (self[ingredient.item] < ingredient.amount)
				return false;
		}
	
		for (const auto& ingredient : recipe->ingredients)
			self[ingredient.item] += ingredient.amount;
	}

	for (const auto& product : recipe->products)
		self[product.item] += product.amount;

	return true;
}

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

size_t TaggedAmount::add_claim(const std::shared_ptr<Task>& task, size_t n)
{
	size_t available = amount - n_claimed();
	if (available < n)
		n = available;
	
	claims.get(task).amount += n;

	return n;
}

[[deprecated]] size_t TaggedAmount::claim(const std::shared_ptr<Task>& task, size_t n)
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

std::string Inventory::str() const
{
	std::string result;
	bool first = true;
	for (auto [proto,amount] : (*this))
	{
		result += (first ? ", " : "") + std::to_string(amount) + "x " + proto->name;
		first = false;
	}
	return result;
}
void Inventory::dump() const
{
	for (auto [proto,amount] : (*this))
		std::cout << "\t" << proto->name << ": " << amount << std::endl;
}

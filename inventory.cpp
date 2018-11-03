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
#include <boost/container/flat_map.hpp>

using namespace sched;

owner_names_t owner_names;
std::string owner_names_t::get(owner_t owner)
{
	return get_or(*this, owner, "[#"+std::to_string(owner)+"]");
}

bool TaggedInventory::can_satisfy(const std::vector<ItemStack>& items_, std::optional<owner_t> owner)
{
	boost::container::flat_map<const ItemPrototype*, size_t> items;
	for (auto [item, amount] : items_)
		items[item]+=amount;

	for (const auto& [item, amount] : items)
		if ((*this)[item].available_to(owner) < amount)
			return false;
	
	return true;
}

bool TaggedInventory::apply(const item_balance_t& bal, std::optional<owner_t> owner)
{
	TaggedInventory& self = *this;

	for (const auto& [item, amount] : bal)
		if (amount < 0 && self[item].available_to(owner) < size_t(-amount))
			return false;

	for (const auto& [item, amount] : bal)
	{
		bool ok = self[item].update(owner, amount);
		assert(ok);
	}
	
	return true;
}

bool Inventory::apply(const item_balance_t& bal)
{
	Inventory& self = *this;

	for (const auto& [item, amount] : bal)
		if (amount < 0 && self[item] < size_t(-amount))
			return false;

	for (const auto& [item, amount] : bal)
		self[item] += amount;
	
	return true;
}

bool Inventory::apply(const Recipe* recipe, bool already_started)
{
	Inventory& self = *this;
	// FIXME deduplicate code with the other apply()

	if (!already_started)
	{
		for (const auto& ingredient : recipe->ingredients)
		{
			if (self[ingredient.item] < ingredient.amount)
				return false;
		}
	
		for (const auto& ingredient : recipe->ingredients)
			self[ingredient.item] -= ingredient.amount;
	}

	for (const auto& product : recipe->products)
		self[product.item] += product.amount;

	return true;
}

size_t TaggedAmount::n_claimed() const
{
	size_t claimed = 0;
	for (const auto& claim : claims)
		claimed += claim.amount;
	return claimed;
}

size_t TaggedAmount::add_claim(owner_t owner, size_t n)
{
	size_t available = amount - n_claimed();
	if (available < n)
		n = available;
	
	claims.get(owner).amount += n;

	return n;
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
	for (auto [proto,amount] : (*this)) if (amount)
		std::cout << "\t" << proto->name << ": " << amount << std::endl;
}

void TaggedInventory::dump() const
{
	for (const auto& [proto, tagged_amount] : (*this)) if (tagged_amount.amount)
	{
		std::cout << "\t" << proto->name << ": " << tagged_amount.amount << std::endl;
		for (const auto& claim : tagged_amount.claims) if (claim.amount)
		{
			std::cout << "\t\t" << owner_names.get(claim.owner) << " <- " << claim.amount << std::endl;
		}
	}
}

void MultiInventory::dump() const
{
	std::vector<inventory_t> types;
	for (const auto& [key, val] : container)
	{
		if (!contains_vec(types, key.inv))
			types.push_back(key.inv);
	}
	for (inventory_t type : types)
	{
		std::cout << inventory_names[type] << std::endl;
		get_inventory(type).dump();
	}
}

Inventory MultiInventory::get_inventory(inventory_t type) const
{
	Inventory result;
	for (const_inv_iterator_t it = inv_begin(type); it != inv_end(type); it++)
		result[it->first.item] = it->second;
	return result;
}

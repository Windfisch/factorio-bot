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

#pragma once

#include "util.hpp"
#include <memory>
#include <algorithm>
#include <numeric>
#include <boost/container/flat_map.hpp>
#include <assert.h>

struct Recipe;

struct ItemPrototype;

using owner_t = intptr_t; // FIXME

struct TaggedAmount
{
	struct Tag
	{
		owner_t owner;
		size_t amount;
	};

	using claims_t = vector_map< Tag, owner_t, &Tag::owner >;
	
	size_t amount;
	claims_t claims;

	/** checks internal consistency */
	void invariant() const
	{
		assert(n_claimed() <= amount);
	}

	/** removes expired Tasks from the list of claims. should be called periodically by the user. */
	void cleanup();

	/** returns the number of claimed items. amount - n_claimed() equals to the maximum amount a very-low-priority owner can claim */
	size_t n_claimed() const;

	/** claims up to n items for `owner`, only considering unclaimed items. claims at
	  * most as many items as are still available. Returns the number of actually
	  * claimed items.
	  */
	size_t add_claim(owner_t owner, size_t n);

	/** adds / removes `n` items for `owner`, also updating the claims. If n is
	 * negative and available_to(owner) is not sufficient, this function fails
	 * by returning false. Otherwise it returns true. */
	bool update(std::optional<owner_t> owner, int n)
	{
		if (n >= 0)
		{
			amount += n;
			if (owner.has_value())
				add_claim(*owner, n);
		}
		else
		{
			if (available_to(owner) < size_t(-n))
				return false;

			amount += n; // n is negative.
			if (owner.has_value())
			{
				auto& claim = claims.get(*owner).amount;
				claim -= std::min(size_t(-n), claim);
			}
		}
		return true;
	}

	/** returns the amount of items claimed for the specified owner. */
	size_t claimed_by(owner_t owner) const
	{
		auto idx = claims.find(owner);
		if (idx == SIZE_MAX)
			return 0;
		else
			return claims[idx].amount;
	}
	
	/** returns the amount of items that is unclaimed */
	size_t unclaimed() const
	{
		assert(n_claimed() <= amount);
		return amount - n_claimed();
	}
	
	/** returns the amount of items available to the specified owner. This includes claimed and free-for-use items. */
	size_t available_to(std::optional<owner_t> owner) const
	{
		if (owner.has_value())
			return unclaimed() + claimed_by(*owner);
		else
			return unclaimed();
	}
};

using item_balance_t = boost::container::flat_map<const ItemPrototype*, int>;

// this is a glorified vector<pair<>>
struct TaggedInventory : boost::container::flat_map<const ItemPrototype*, TaggedAmount>
{
	void dump() const;

	bool apply(const item_balance_t& bal, std::optional<owner_t> owner);
};

struct Inventory : public boost::container::flat_map<const ItemPrototype*, size_t>
{
	/** applies the recipe. if successful, the inventory is altered and true is returned, otherwise the inventory remains unchanged and false is returned.
	  If already_started == true, the ingredients are not removed, but the products are added. In this
	  case, the function always returns true.
	*/
	bool apply(const Recipe*, bool already_started = false);
	
	bool apply(const item_balance_t& bal);


	/** constructs the empty inventory */
	Inventory() : flat_map() {}
  

	Inventory(std::initializer_list<value_type> il) : flat_map(il) {}

	/** constructs an inventory from a TaggedInventory, considering only items claimed by the specified owner */
	static Inventory get_claimed_by(const TaggedInventory& inv, owner_t owner)
	{
		Inventory result;
		for (const auto& entry : inv)
			if (size_t claimed = entry.second.claimed_by(owner))
				result[entry.first] = claimed;
		return result;
	}
	/** constructs an inventory from a TaggedInventory, considering only unclaimed items */
	static Inventory get_unclaimed(const TaggedInventory& inv)
	{
		Inventory result;
		for (const auto& entry : inv)
			if (size_t claimed = entry.second.unclaimed())
				result[entry.first] = claimed;
		return result;
	}

	void dump() const;
	std::string str() const;
};

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
#include "defines.h"
#include <memory>
#include <algorithm>
#include <numeric>
#include <boost/container/flat_map.hpp>
#include <boost/iterator/filter_iterator.hpp>
#include <assert.h>
#include <unordered_map>
#include <string>

struct Recipe;

struct ItemPrototype;
struct ItemStack;

using owner_t = intptr_t; // FIXME

// DEBUG only
struct owner_names_t : public std::unordered_map<owner_t, std::string>
{
	std::string get(owner_t owner);
};
extern owner_names_t owner_names;

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
	bool can_satisfy(const std::vector<ItemStack>& items, std::optional<owner_t> owner);
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

	Inventory& operator+=(const Inventory& other)
	{
		for (const auto& [key,value] : other)
			(*this)[key] += value;
		return *this;
	}
	Inventory& operator-=(const Inventory& other)
	{
		for (const auto& [key,value] : other)
			(*this)[key] -= value;
		return *this;
	}
	Inventory& operator*=(size_t mult)
	{
		for (auto& [key,value] : (*this))
			value *= mult;
		return *this;
	}

	Inventory operator+(const Inventory& other) const
	{
		Inventory result = *this;
		result += other;

		return result;
	}

	Inventory operator-(const Inventory& other) const
	{
		Inventory result = *this;
		result -= other;
		return result;
	}

	Inventory operator*(size_t mult) const
	{
		Inventory result = *this;
		result *= mult;
		return result;
	}

	Inventory max(const Inventory& other) const
	{
		Inventory result = *this;
		for (const auto& [key,value] : other)
			result[key] = std::max(result[key], value);
		return result;
	}

	Inventory min(const Inventory& other) const
	{
		Inventory result = *this;
		for (const auto& [key,value] : other)
		{
			size_t amount = std::min(value, this->get_or(key, 0));
			if (amount)
				result[key] = amount;
		}
		return result;
	}

	size_t get_or(const ItemPrototype* item, size_t default_value) const {
		auto it = find(item);
		if (it != end())
			return it->second;
		else
			return default_value;
	}


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

inline Inventory operator*(size_t mult, const Inventory& inv) { return inv * mult; }

struct MultiInventory
{
	struct key_t
	{
		const ItemPrototype* item;
		inventory_t inv;

		bool operator< (const key_t& o) const
		{
			if (item < o.item) return true;
			if (item > o.item) return false;
			return inv < o.inv;
		}
	};
	using container_t = boost::container::flat_map<key_t, size_t>;
	using iterator_t = container_t::iterator;
	using item_iterator_t = iterator_t;
	using const_iterator_t = container_t::const_iterator;
	using const_item_iterator_t = const_iterator_t;
	
	struct Predicate
	{
		inventory_t inv;
		bool operator()(const container_t::value_type& x)
		{
			return x.first.inv == inv;
		}
	};

	using inv_iterator_t = boost::filter_iterator<Predicate, iterator_t>;
	using const_inv_iterator_t = boost::filter_iterator<Predicate, const_iterator_t>;

	size_t& get(const ItemPrototype* item, inventory_t inv) { return container[key_t{item,inv}]; }
	size_t get_or(const ItemPrototype* item, inventory_t inv, size_t default_value) const {
		auto it = container.find(key_t{item,inv});
		if (it != container.end())
			return it->second;
		else
			return default_value;
	}

	iterator_t begin() { return container.begin(); }
	iterator_t end() { return container.end(); }

	item_iterator_t item_begin(const ItemPrototype* item) { return container.lower_bound(key_t{item, INV_MIN}); }
	item_iterator_t item_end(const ItemPrototype* item) { return container.upper_bound(key_t{item, INV_MAX}); }

	inv_iterator_t inv_begin(inventory_t inv) {
		return inv_iterator_t(Predicate{inv}, container.begin(), container.end());
	}
	inv_iterator_t inv_end(inventory_t inv) {
		return inv_iterator_t(Predicate{inv}, container.end(), container.end());
	}

	const_iterator_t begin() const { return container.begin(); }
	const_iterator_t end() const { return container.end(); }

	const_item_iterator_t item_begin(const ItemPrototype* item) const { return container.lower_bound(key_t{item, INV_MIN}); }
	const_item_iterator_t item_end(const ItemPrototype* item) const { return container.upper_bound(key_t{item, INV_MAX}); }

	const_inv_iterator_t inv_begin(inventory_t inv) const {
		return const_inv_iterator_t(Predicate{inv}, container.begin(), container.end());
	}
	const_inv_iterator_t inv_end(inventory_t inv) const {
		return const_inv_iterator_t(Predicate{inv}, container.end(), container.end());
	}

	void clear() { container.clear(); }

	std::pair<iterator_t, bool> insert(inventory_t inv, const ItemPrototype* item, size_t val)
	{
		return container.insert(std::pair{ key_t{item,inv}, val });
	}

	void dump() const;
	Inventory get_inventory(inventory_t type) const;
	
	private:
		 container_t container;
};

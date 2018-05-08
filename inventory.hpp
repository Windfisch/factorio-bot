#pragma once

#include "util.hpp"
#include <memory>
#include <algorithm>
#include <numeric>
#include <boost/container/flat_map.hpp>
#include <assert.h>

struct Recipe;

namespace sched {
struct Task; // deprecated
}
struct ItemPrototype;

struct TaggedAmount
{
	struct Tag
	{
		std::weak_ptr<sched::Task> owner;
		size_t amount;
	};

	using claims_t = vector_map< Tag, std::weak_ptr<sched::Task>, &Tag::owner, weak_ptr_equal<std::weak_ptr<sched::Task>> >;
	
	size_t amount;
	claims_t claims;

	/** checks internal consistency */
	void invariant() const
	{
		assert(n_claimed() <= amount);
	}

	/** removes expired Tasks from the list of claims. should be called periodically by the user. */
	void cleanup();

	/** returns the number of claimed items. amount - n_claimed() equals to the maximum amount a very-low-priority task can claim */
	size_t n_claimed() const;

	/** claims up to n items for `task`, possibly stealing them from lower-priority tasks */
	size_t claim(const std::shared_ptr<sched::Task>& task, size_t n);

	/** returns the amount of items claimed for the specified task. */
	size_t claimed_by(const std::shared_ptr<sched::Task>& task) const
	{
		auto idx = claims.find(task);
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
	
	/** returns the amount of items available to the specified task. This includes claimed and free-for-use items. */
	size_t available_for(const std::shared_ptr<sched::Task>& task) const
	{
		return unclaimed() + claimed_by(task);
	}
};

// this is a glorified vector<pair<>>
typedef boost::container::flat_map<const ItemPrototype*, TaggedAmount> TaggedInventory;

struct Inventory : public boost::container::flat_map<const ItemPrototype*, size_t>
{
	/** applies the recipe. if successful, the inventory is altered and true is returned, otherwise the inventory remains unchanged and false is returned.
	  If already_started == true, the ingredients are not removed, but the products are added. In this
	  case, the function always returns true.
	*/
	bool apply(const Recipe*, bool already_started = false);


	/** constructs the empty inventory */
	Inventory() : flat_map() {}
  

	Inventory(std::initializer_list<value_type> il) : flat_map(il) {}

	/** constructs an inventory from a TaggedInventory, considering only items claimed by the specified task */
	static Inventory get_claimed_by(const TaggedInventory& inv, const std::shared_ptr<sched::Task>& task)
	{
		Inventory result;
		for (const auto& entry : inv)
			if (size_t claimed = entry.second.claimed_by(task))
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
};

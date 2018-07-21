/*
 * Copyright (c) 2017 Florian Jung
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

#include <string>
#include <vector>
#include <chrono>

#include "item.h"

struct Recipe
{
	struct ItemAmount
	{
		const ItemPrototype* item;
		double amount; // this is mostly int, but some recipes have probabilities.
		               // in this case, the average value is stored here.

		ItemAmount(const ItemPrototype* item_, double amount_) : item(item_),amount(amount_) {}
	};

	std::string name;
	bool enabled;
	double energy;

	std::chrono::milliseconds crafting_duration() const {
		return std::chrono::milliseconds(int(energy*1000.));
	}

	std::vector<ItemAmount> ingredients;
	std::vector<ItemAmount> products;

	/** returns the balance for the specified item. positive values means
	  * that the item is produced, negative means consumed. */
	int balance_for(const ItemPrototype* item) const
	{
		int balance = 0;
		for (auto [product,amount] : products)
			if (product == item)
				balance += amount;
		
		for (auto [ingredient,amount] : ingredients)
			if (ingredient == item)
				balance -= amount;

		return balance;
	}
};

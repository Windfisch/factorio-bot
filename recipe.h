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

#include <string>
#include <vector>

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

	std::vector<ItemAmount> ingredients;
	std::vector<ItemAmount> products;
};

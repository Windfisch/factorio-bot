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
#include "entity.h"
#include <string>


struct ItemPrototype
{
	std::string name;
	std::string type;

	const EntityPrototype* place_result;
	int stack_size;
	double fuel_value;
	double speed;
	double durability;
	
	ItemPrototype(std::string name_, std::string type_, const EntityPrototype* place_result_,
		int stack_size_, double fuel_value_, double speed_, double durability_) :
		name(name_), type(type_), place_result(place_result_), stack_size(stack_size_),
		fuel_value(fuel_value_), speed(speed_), durability(durability_) {}
};

struct ItemStack
{
	const ItemPrototype* proto;
	size_t amount;
};

struct [[deprecated]] OwnedItemStack : public ItemStack
{
	
};



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
#include "pos.hpp"
#include "entity.h"
#include "inventory.hpp"

struct ItemStorage
{
	Pos_f get_pos() const { return entity.pos; } // WorldList<ItemStorage> wants to call this.
	
	Entity entity;
	Inventory inventory;

	// TODO: last_update timestamp; prediction; distance-based updating (more frequently if close)
	// currently, we transmit all inventories of all chests with a fixed period; this will not scale.
};

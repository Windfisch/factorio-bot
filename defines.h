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

enum dir4_t
{
	NORTH=0,
	EAST,
	SOUTH,
	WEST,

	TOP=NORTH,
	RIGHT=EAST,
	BOTTOM=SOUTH,
	LEFT=WEST
};

enum dir8_t
{
	d8_NORTH=0,
	d8_NORTHEAST,
	d8_EAST,
	d8_SOUTHEAST,
	d8_SOUTH,
	d8_SOUTHWEST,
	d8_WEST,
	d8_NORTHWEST
};

enum inventory_t
{
	INV_FUEL,
	INV_BURNT_RESULT,
	INV_CHEST,
	INV_FURNACE_SOURCE,
	INV_FURNACE_RESULT,
	INV_FURNACE_MODULES,
	INV_PLAYER_QUICKBAR,
	INV_PLAYER_MAIN,
	INV_PLAYER_GUNS,
	INV_PLAYER_AMMO,
	INV_PLAYER_ARMOR,
	INV_PLAYER_TOOLS,
	INV_PLAYER_VEHICLE,
	INV_PLAYER_TRASH,
	INV_GOD_QUICKBAR,
	INV_GOD_MAIN,
	INV_ROBOPORT_ROBOT,
	INV_ROBOPORT_MATERIAL,
	INV_ROBOT_CARGO,
	INV_ROBOT_REPAIR,
	INV_ASSEMBLING_MACHINE_INPUT,
	INV_ASSEMBLING_MACHINE_OUTPUT,
	INV_ASSEMBLING_MACHINE_MODULES,
	INV_LAB_INPUT,
	INV_LAB_MODULES,
	INV_MINING_DRILL_MODULES,
	INV_ITEM_MAIN,
	INV_ROCKET_SILO_ROCKET,
	INV_ROCKET_SILO_RESULT,
	INV_CAR_TRUNK,
	INV_CAR_AMMO,
	INV_CARGO_WAGON,
	INV_TURRET_AMMO,
	INV_BEACON_MODULES
};

extern std::string inventory_names[];

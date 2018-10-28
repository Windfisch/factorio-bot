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

#include "defines.h"
#include <string>

using namespace std;

string inventory_names[] = {
	"defines.inventory.fuel",
	"defines.inventory.burnt_result",
	"defines.inventory.chest",
	"defines.inventory.furnace_source",
	"defines.inventory.furnace_result",
	"defines.inventory.furnace_modules",
	"defines.inventory.player_quickbar",
	"defines.inventory.player_main",
	"defines.inventory.player_guns",
	"defines.inventory.player_ammo",
	"defines.inventory.player_armor",
	"defines.inventory.player_tools",
	"defines.inventory.player_vehicle",
	"defines.inventory.player_trash",
	"defines.inventory.god_quickbar",
	"defines.inventory.god_main",
	"defines.inventory.roboport_robot",
	"defines.inventory.roboport_material",
	"defines.inventory.robot_cargo",
	"defines.inventory.robot_repair",
	"defines.inventory.assembling_machine_input",
	"defines.inventory.assembling_machine_output",
	"defines.inventory.assembling_machine_modules",
	"defines.inventory.lab_input",
	"defines.inventory.lab_modules",
	"defines.inventory.mining_drill_modules",
	"defines.inventory.item_main",
	"defines.inventory.rocket_silo_rocket",
	"defines.inventory.rocket_silo_result",
	"defines.inventory.car_trunk",
	"defines.inventory.car_ammo",
	"defines.inventory.cargo_wagon",
	"defines.inventory.turret_ammo",
	"defines.inventory.beacon_modules"
};

inventory_flag_t IN = {true, false};
inventory_flag_t OUT = {false, true};
inventory_flag_t INOUT = {true, true};
inventory_flag_t NONE = {false, false};

inventory_flag_t inventory_flags[] = {
	IN, //"defines.inventory.fuel",
	OUT, //"defines.inventory.burnt_result",
	INOUT, //"defines.inventory.chest",
	IN, //"defines.inventory.furnace_source",
	OUT, //"defines.inventory.furnace_result",
	NONE, //"defines.inventory.furnace_modules",
	NONE, //"defines.inventory.player_quickbar",
	NONE, //"defines.inventory.player_main",
	NONE, //"defines.inventory.player_guns",
	NONE, //"defines.inventory.player_ammo",
	NONE, //"defines.inventory.player_armor",
	NONE, //"defines.inventory.player_tools",
	NONE, //"defines.inventory.player_vehicle",
	NONE, //"defines.inventory.player_trash",
	NONE, //"defines.inventory.god_quickbar",
	NONE, //"defines.inventory.god_main",
	NONE, //"defines.inventory.roboport_robot",
	NONE, //"defines.inventory.roboport_material",
	NONE, //"defines.inventory.robot_cargo",
	NONE, //"defines.inventory.robot_repair",
	IN, //"defines.inventory.assembling_machine_input",
	OUT, //"defines.inventory.assembling_machine_output",
	NONE, //"defines.inventory.assembling_machine_modules",
	IN, //"defines.inventory.lab_input",
	NONE, //"defines.inventory.lab_modules",
	NONE, //"defines.inventory.mining_drill_modules",
	NONE, //"defines.inventory.item_main",
	NONE, //"defines.inventory.rocket_silo_rocket",
	NONE, //"defines.inventory.rocket_silo_result",
	INOUT, //"defines.inventory.car_trunk",
	INOUT, //"defines.inventory.car_ammo",
	INOUT, //"defines.inventory.cargo_wagon",
	IN, //"defines.inventory.turret_ammo",
	NONE, //"defines.inventory.beacon_modules"
};

const unordered_map<string, inventory_t> inventory_types {
	{"defines.inventory.fuel", INV_FUEL},
	{"defines.inventory.burnt_result", INV_BURNT_RESULT},
	{"defines.inventory.chest", INV_CHEST},
	{"defines.inventory.furnace_source", INV_FURNACE_SOURCE},
	{"defines.inventory.furnace_result", INV_FURNACE_RESULT},
	{"defines.inventory.furnace_modules", INV_FURNACE_MODULES},
	{"defines.inventory.player_quickbar", INV_PLAYER_QUICKBAR},
	{"defines.inventory.player_main", INV_PLAYER_MAIN},
	{"defines.inventory.player_guns", INV_PLAYER_GUNS},
	{"defines.inventory.player_ammo", INV_PLAYER_AMMO},
	{"defines.inventory.player_armor", INV_PLAYER_ARMOR},
	{"defines.inventory.player_tools", INV_PLAYER_TOOLS},
	{"defines.inventory.player_vehicle", INV_PLAYER_VEHICLE},
	{"defines.inventory.player_trash", INV_PLAYER_TRASH},
	{"defines.inventory.god_quickbar", INV_GOD_QUICKBAR},
	{"defines.inventory.god_main", INV_GOD_MAIN},
	{"defines.inventory.roboport_robot", INV_ROBOPORT_ROBOT},
	{"defines.inventory.roboport_material", INV_ROBOPORT_MATERIAL},
	{"defines.inventory.robot_cargo", INV_ROBOT_CARGO},
	{"defines.inventory.robot_repair", INV_ROBOT_REPAIR},
	{"defines.inventory.assembling_machine_input", INV_ASSEMBLING_MACHINE_INPUT},
	{"defines.inventory.assembling_machine_output", INV_ASSEMBLING_MACHINE_OUTPUT},
	{"defines.inventory.assembling_machine_modules", INV_ASSEMBLING_MACHINE_MODULES},
	{"defines.inventory.lab_input", INV_LAB_INPUT},
	{"defines.inventory.lab_modules", INV_LAB_MODULES},
	{"defines.inventory.mining_drill_modules", INV_MINING_DRILL_MODULES},
	{"defines.inventory.item_main", INV_ITEM_MAIN},
	{"defines.inventory.rocket_silo_rocket", INV_ROCKET_SILO_ROCKET},
	{"defines.inventory.rocket_silo_result", INV_ROCKET_SILO_RESULT},
	{"defines.inventory.car_trunk", INV_CAR_TRUNK},
	{"defines.inventory.car_ammo", INV_CAR_AMMO},
	{"defines.inventory.cargo_wagon", INV_CARGO_WAGON},
	{"defines.inventory.turret_ammo", INV_TURRET_AMMO},
	{"defines.inventory.beacon_modules", INV_BEACON_MODULES}
};

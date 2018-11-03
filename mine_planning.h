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

#include <vector>
#include "pos.hpp"
#include "entity.h"
#include "resource.hpp"

class FactorioGame;

std::vector<PlannedEntity> plan_mine(const std::vector<Pos>& positions, Pos destination, const FactorioGame& game);
std::vector<PlannedEntity> plan_mine(const std::vector<Pos>& positions, Pos destination, unsigned side_max, const EntityPrototype* belt_proto, const EntityPrototype* machine_proto, int outerx = 4, int outery = 4);

std::vector<PlannedEntity> plan_early_mine(const ResourcePatch& patch, const FactorioGame* game, std::vector<Entity> rig, Pos size, Area mining_area, dir4_t side);

std::vector<PlannedEntity> plan_early_smelter_rig(const ResourcePatch& patch, const FactorioGame* game);

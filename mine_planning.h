#pragma once

#include <vector>
#include "pos.hpp"
#include "entity.h"

class FactorioGame;

std::vector<PlannedEntity> plan_mine(const std::vector<Pos>& positions, Pos destination, const FactorioGame& game);
std::vector<PlannedEntity> plan_mine(const std::vector<Pos>& positions, Pos destination, unsigned side_max, const EntityPrototype* belt_proto, const EntityPrototype* machine_proto, int outerx = 4, int outery = 4);

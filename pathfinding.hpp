#pragma once
#include <vector>
#include "pos.hpp"
#include "factorio_io.h"
#include "worldmap.hpp"

std::vector<Pos> a_star(const Pos& start, const Pos& end, const WorldMap<FactorioGame::walk_t>::Viewport& view, double size);

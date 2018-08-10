#pragma once

#include "pos.hpp"
#include "action.hpp"
#include "item.h"
#include "inventory.hpp"

#include <memory>
#include <boost/container/flat_map.hpp>

struct Player
{
	size_t id; // TODO this should be a string (the player's name) probably?
	Pos_f position;
	bool connected;
	[[deprecated]] std::unique_ptr<action::CompoundGoal> goals;

	TaggedInventory inventory;
};

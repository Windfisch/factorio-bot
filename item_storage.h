#pragma once
#include "pos.hpp"
#include "entity.h"
#include "inventory.hpp"

struct ItemStorage
{
	Pos_f get_pos() const { return entity.pos; } // WorldList<ItemStorage> wants to call this.
	
	Entity entity;
	Inventory inventory;
};

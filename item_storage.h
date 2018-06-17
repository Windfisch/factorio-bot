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

#pragma once
#include "pos.hpp"
#include "entity.h"
#include "inventory.hpp"

struct Chest // FIXME move this to somewhere sane
{
	Pos_f get_pos() const { return entity.pos; } // WorldList<Chest> wants to call this.
	
	Entity entity;
	Inventory inventory;
};

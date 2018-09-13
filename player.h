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
#include "action.hpp"
#include "inventory.hpp"

#include <memory>
#include <boost/container/flat_map.hpp>

struct Player
{
	size_t id; // TODO this should be a string (the player's name) probably?
	Pos_f position;
	bool connected;
	
	TaggedInventory inventory;
	
	/** sets the players actions, starts them (and aborts the previous, if any) */
	void set_actions(std::shared_ptr<action::CompoundAction> a)
	{
		if (actions)
			actions->abort();
		actions = a;
		actions->start();
	}

	/** ticks the player's action, if set. returns true, if the action has finished */
	bool tick_actions()
	{
		if (actions)
		{
			if (actions->is_finished())
			{
				actions = nullptr;
				return true;
			}
			else
			{
				actions->tick();
				return false;
			}
		}
		else
			return false;
	}

	private:
	std::shared_ptr<action::CompoundAction> actions;

	friend class FactorioGame;
};

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
#include "area.hpp"
#include "pos.hpp"
#include <string>
#include <memory>

struct EntityPrototype
{
	std::string name;
	bool collides_player;
	bool collides_object;
	Area_f collision_box;
	bool mineable;

	EntityPrototype(const std::string& name_, const std::string& collision_str, const Area_f& collision_box_, bool mineable_) :
		name(name_),
		collides_player(collision_str.find('P') != std::string::npos),
		collides_object(collision_str.find('O') != std::string::npos),
		collision_box(collision_box_),
		mineable(mineable_)
		{}
};

struct Entity
{
	Pos_f pos;
	const EntityPrototype* proto;

	bool operator==(const Entity& that) { return this->pos==that.pos && this->proto==that.proto; }

	Area_f collision_box() const { return proto->collision_box.shift(pos); }

	Entity(const Pos_f& pos_, const EntityPrototype* proto_) : pos(pos_), proto(proto_) {}
};

struct DesiredEntity : public Entity
{
	std::weak_ptr<Entity> corresponding_actual_entity;
	std::shared_ptr<Entity> get_actual();
};

template<typename container_type> Area_f bounding_box(const container_type& container)
{
	bool first = true;
	Area_f result;
	for (const auto& thing : container)
	{
		if (first) result = Area_f(thing.pos, thing.pos);
		else result = result.expand(thing.pos);
		first=false;
	}
	return result;
}

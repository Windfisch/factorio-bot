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
#include "area.hpp"
#include "pos.hpp"
#include "multivariant.hpp"
#include "defines.h"
#include "inventory.hpp"
#include <string>
#include <memory>
#include <algorithm>

struct ContainerData
{
	MultiInventory inventories;
	bool fuel_is_output = false;
};

struct MachineData : public ContainerData {};
struct MiningDrillData : public ContainerData {};

using mvu = multivariant_utils<typelist<ContainerData, MachineData, MiningDrillData>>;

struct ItemPrototype;

struct EntityPrototype
{
	static double max_collision_box_size;

	std::string name;
	std::string type;
	Area_f collision_box;
	bool collides_player;
	bool collides_object;
	bool mineable;
	std::vector< std::pair<std::string, size_t> > mine_results_str;
	item_balance_t mine_results; // filled in later by resolve_item_references()
	size_t data_kind;
	
	EntityPrototype(const std::string& name_, const std::string& type_, const std::string& collision_str, const Area_f& collision_box_, bool mineable_, std::vector< std::pair<std::string, size_t> > mine_results_str_) :
		name(name_),
		type(type_),
		collision_box(collision_box_),
		collides_player(collision_str.find('P') != std::string::npos),
		collides_object(collision_str.find('O') != std::string::npos),
		mineable(mineable_),
		mine_results_str(mine_results_str_),
		data_kind(mvu::invalid_index)
		{
			max_collision_box_size = std::max(max_collision_box_size, collision_box_.radius_around(Pos_f(0,0)));

			if (type_ == "container" || type_ == "logistic-container")
				data_kind = mvu::index<ContainerData>();
			else if (type_ == "mining-drill")
				data_kind = mvu::index<MiningDrillData>();
			else if (type_ == "assembling-machine")
				data_kind = mvu::index<MachineData>();
			else if (type_ == "furnace")
				data_kind = mvu::index<MachineData>();
		}
};

struct Entity
{
	Pos_f get_pos() const { return pos; } // WorldList<Entity> wants to call this.
	Area_f get_extent() const { return collision_box(); } // WorldList<Entity> wants to call this.
	static double get_max_extent() { return EntityPrototype::max_collision_box_size; } // same

	Pos_f pos;
	const EntityPrototype* proto = nullptr;
	dir4_t direction = NORTH;

	std::string str() const { return proto->name+"@"+pos.str(); }

	refcount_base* data_ptr;

	//bool operator==(const Entity& that) const { return this->pos==that.pos && this->proto==that.proto && this->direction==that.direction && this->data_ptr == that.data_ptr; }
	constexpr bool mostly_equals(const Entity& that) const { return this->pos==that.pos && this->proto==that.proto; }

	struct mostly_equals_comparator
	{
		constexpr bool operator()(const Entity &lhs, const Entity &rhs) const
		{
			return lhs.mostly_equals(rhs);
		}
	};

	void takeover_data(Entity& that)
	{
		assert (this->proto == that.proto);

		release_data();
		this->data_ptr = that.data_ptr;
		that.data_ptr = nullptr;
	}

	Area_f collision_box() const { return proto->collision_box.rotate(direction).shift(pos); }

	template <typename T> T& data()
	{
		return *mvu::get<T>(proto->data_kind, data_ptr);
	}
	template <typename T> T* data_or_null()
	{
		return mvu::get_or_null<T>(proto->data_kind, data_ptr);
	}
	template <typename T> const T& data() const
	{
		return *mvu::get<const T>(proto->data_kind, data_ptr);
	}
	template <typename T> const T* data_or_null() const
	{
		return mvu::get_or_null<const T>(proto->data_kind, data_ptr);
	}

	void make_unique()
	{
		data_ptr = mvu::clone(proto->data_kind, data_ptr);
	}

	~Entity()
	{
		release_data();
	}


	struct nullent_tag {};

	Entity(nullent_tag, const Pos_f& pos_ = Pos_f()) : pos(pos_), proto(nullptr), data_ptr(nullptr)
	{
	}

	Entity(const Pos_f& pos_, const EntityPrototype* proto_, dir4_t direction_=NORTH)
		: pos(pos_), proto(proto_), direction(direction_), data_ptr(mvu::make(proto_->data_kind))
	{
		if (data_ptr)
			data_ptr->refcount = 1;
	}

	Entity(const Entity& e)
	{
		pos = e.pos;
		proto = e.proto;
		direction = e.direction;
		data_ptr = e.data_ptr;
		if (data_ptr)
			data_ptr->refcount++;
	}

	Entity(Entity&& e) noexcept
	{
		pos = std::move(e.pos);
		proto = std::move(e.proto);
		direction = std::move(e.direction);
		data_ptr = e.data_ptr;
		e.data_ptr = nullptr;
	}

	Entity& operator=(const Entity& e)
	{
		release_data();

		pos = e.pos;
		proto = e.proto;
		direction = e.direction;
		data_ptr = e.data_ptr;
		if (data_ptr)
			data_ptr->refcount++;

		return *this;
	}
	Entity& operator=(Entity&& e)
	{
		release_data();

		pos = std::move(e.pos);
		proto = std::move(e.proto);
		direction = std::move(e.direction);
		data_ptr = e.data_ptr;
		e.data_ptr = nullptr;

		return *this;
	}

	private:
		void release_data()
		{
			if (data_ptr)
			{
				data_ptr->refcount--;
				if (data_ptr->refcount == 0)
					mvu::del(proto->data_kind, data_ptr);
			}
		}
};

struct DesiredEntity : public Entity
{
	std::weak_ptr<Entity> corresponding_actual_entity;
	std::shared_ptr<Entity> get_actual();
	
	DesiredEntity(const Entity& ent) : Entity(ent) {}
	DesiredEntity(const Pos_f& pos_, const EntityPrototype* proto_, dir4_t direction_=NORTH)
		: Entity(pos_, proto_, direction_) {}
};

struct PlannedEntity : public DesiredEntity
{
	int level; // at which facility-level to place this

	PlannedEntity(int level_, const Entity& ent) : DesiredEntity(ent), level(level_) {}
	PlannedEntity(int level_, const Pos_f& pos_, const EntityPrototype* proto_, dir4_t direction_=NORTH)
		: DesiredEntity(pos_, proto_, direction_), level(level_) {}
	PlannedEntity(const Pos_f& pos_, const EntityPrototype* proto_, dir4_t direction_=NORTH)
		: DesiredEntity(pos_, proto_, direction_), level(-1) {}
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

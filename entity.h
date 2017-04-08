#pragma once
#include "area.hpp"
#include "pos.hpp"
#include <string>

struct EntityPrototype
{
	bool collides_player;
	bool collides_object;
	Area_f collision_box;
	bool mineable;

	EntityPrototype(const std::string& collision_str, const Area_f& collision_box_, bool mineable_) :
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

	Area_f collision_box() const { return proto->collision_box.shift(pos); }

	Entity(const Pos_f& pos_, const EntityPrototype* proto_) : pos(pos_), proto(proto_) {}
};

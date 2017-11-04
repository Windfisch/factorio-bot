#pragma once
#include "entity.h"
#include <string>

struct ItemPrototype
{
	std::string name;
	std::string type;

	const EntityPrototype* place_result;
	int stack_size;
	double fuel_value;
	double speed;
	double durability;
	
	ItemPrototype(std::string name_, std::string type_, const EntityPrototype* place_result_,
		int stack_size_, double fuel_value_, double speed_, double durability_) :
		name(name_), type(type_), place_result(place_result_), stack_size(stack_size_),
		fuel_value(fuel_value_), speed(speed_), durability(durability_) {}
};


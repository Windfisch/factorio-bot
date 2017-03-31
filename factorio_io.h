#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <set>

#include "pathfinding.hpp"
#include "worldmap.hpp"
#include "pos.hpp"
#include "area.hpp"
#include "rcon.h"

enum dir4_t
{
	NORTH=0,
	EAST,
	SOUTH,
	WEST,

	TOP=NORTH,
	RIGHT=EAST,
	BOTTOM=SOUTH,
	LEFT=WEST
};

constexpr int NOT_YET_ASSIGNED = 0; // TODO FIXME

struct ResourcePatch;

struct Resource
{
	enum type_t
	{
		NONE,
		COAL,
		IRON,
		COPPER,
		STONE,
		OIL
	};
	static const std::unordered_map<std::string, Resource::type_t> types;

	enum floodfill_flag_t
	{
		FLOODFILL_NONE,
		FLOODFILL_QUEUED
	};

	floodfill_flag_t floodfill_flag = FLOODFILL_NONE;
	
	type_t type;
	int patch_id;
	std::weak_ptr<ResourcePatch> resource_patch;

	Resource(type_t t, int parent) : type(t), patch_id(parent) {}
	Resource() : type(NONE), patch_id(NOT_YET_ASSIGNED) {}

};

struct EntityPrototype
{
	bool collides_player;
	Area_f collision_box;

	EntityPrototype(bool collides_player_, const Area_f& collision_box_) : collides_player(collides_player_), collision_box(collision_box_) {}
	EntityPrototype(const std::string& collision_str, const Area_f& collision_box_) :
		collides_player(collision_str.find('P') != std::string::npos), collision_box(collision_box_) {}
};

struct Entity
{
	Pos_f pos;
	const EntityPrototype* proto;

	Area_f collision_box() const { return proto->collision_box.shift(pos); }

	Entity(const Pos_f& pos_, const EntityPrototype* proto_) : pos(pos_), proto(proto_) {}
};

class FactorioGame
{
	private:
		Rcon rcon;

		std::string line_buffer;

		std::string factorio_file_prefix;
		std::ifstream factorio_file;
		int factorio_file_id = 1;

		void change_file_id(int new_id);
		std::string factorio_file_name();
		std::string remove_line_from_buffer();

		std::unordered_map< std::string, const EntityPrototype* > entity_prototypes;

		void parse_tiles(const Area& area, const std::string& data);
		void parse_resources(const Area& area, const std::string& data);
		void parse_entity_prototypes(const std::string& data);
		void parse_players(const std::string& data);
		void parse_objects(const Area& area, const std::string& data);
		
		void floodfill_resources(WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius);
		int next_free_resource_id = 1;
	
	public:
		FactorioGame(std::string prefix);
		void rcon_connect(std::string host, int port, std::string password);
		std::string read_packet();
		void parse_packet(const std::string& data);

		void set_waypoints(int id, const std::vector<Pos>& waypoints);
		void walk_to(int id, const Pos& dest);

		WorldMap<pathfinding::walk_t> walk_map;
		WorldMap<Resource> resource_map;
		std::set< std::shared_ptr<ResourcePatch> > resource_patches;
		
		struct Player
		{
			Pos_f position;
			bool connected;
		};
		std::vector<Player> players;
};

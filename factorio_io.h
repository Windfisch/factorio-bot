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
#include "resource.hpp"
#include "entity.h"
#include "item.h"
#include "recipe.h"
#include "action.hpp"

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

		double max_entity_radius = 0.;
		std::unordered_map< std::string, const EntityPrototype* > entity_prototypes;
		std::unordered_map< std::string, const ItemPrototype* > item_prototypes;
		std::unordered_map< std::string, const Recipe* > recipes;

		std::unordered_map< Pos, std::vector< std::shared_ptr<Entity> > > actual_entities; // list of entities that are actually there per chunk
		std::unordered_map< Pos, std::vector< std::shared_ptr<DesiredEntity> > > desired_entities; // list of entities that we expect to be there per chunk

		/* desired_entities and actual_entities deviate because of the following reasons:
		 *   - trees are irrelevant for desired_entities and thus omitted
		 *   - an entity was scheduled to be built, but is still awaiting its construction
		 *     (walking there takes some time)
		 *   - an entity was destroyed by a biter attack
		 *   - an entity was reconfigured by another player
		 */


		void parse_tiles(const Area& area, const std::string& data);
		void parse_resources(const Area& area, const std::string& data);
		void parse_entity_prototypes(const std::string& data);
		void parse_item_prototypes(const std::string& data);
		void parse_recipes(const std::string& data);
		void parse_action_completed(const std::string& data);
		void parse_players(const std::string& data);
		void parse_objects(const Area& area, const std::string& data);
		void update_walkmap(const Area& area);
		void parse_mined_item(const std::string& data);
		
		void floodfill_resources(WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius);
		int next_free_resource_id = 1;
	
	public:
		FactorioGame(std::string prefix);
		void rcon_connect(std::string host, int port, std::string password);
		void rcon_call(std::string func, std::string args);
		void rcon_call(std::string func, int action_id, int player_id, std::string args);
		std::string read_packet();
		void parse_packet(const std::string& data);

		// never use these functions directly, use player goals instead
		void set_waypoints(int action_id, int player_id, const std::vector<Pos>& waypoints);
		void set_mining_target(int action_id, int player_id, Entity target);
		void unset_mining_target(int player_id);
		void start_crafting(int action_id, int player_id, std::string recipe, int count=1);
		
		[[deprecated("set player goals instead")]]
		void walk_to(int id, const Pos& dest);

		WorldMap<pathfinding::walk_t> walk_map;
		WorldMap<Resource> resource_map;
		std::set< std::shared_ptr<ResourcePatch> > resource_patches;
		
		struct Player
		{
			size_t id; // TODO this should be a string (the player's name) probably?
			Pos_f position;
			bool connected;
			std::unique_ptr<action::CompoundGoal> goals;
		};
		std::vector<Player> players;

		const EntityPrototype& get_entity_prototype(std::string name) const { return *entity_prototypes.at(name); }
};

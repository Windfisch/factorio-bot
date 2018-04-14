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
#include "player.h"
#include "resource.hpp"
#include "entity.h"
#include "item.h"
#include "recipe.h"
#include "action.hpp"
#include "worldlist.hpp"
#include "defines.h"
#include "graphics_definitions.h"

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
	
	public:
		WorldList<Entity> actual_entities; // list of entities that are actually there per chunk
		WorldList<DesiredEntity> desired_entities; // list of entities that we expect to be there per chunk
		
		// the GraphicsDefinition has either one or four entries: north east south west.
		std::unordered_map< std::string, std::vector<GraphicsDefinition> > graphics_definitions;

	private:
		/* desired_entities and actual_entities deviate because of the following reasons:
		 *   - trees are irrelevant for desired_entities and thus omitted
		 *   - an entity was scheduled to be built, but is still awaiting its construction
		 *     (walking there takes some time)
		 *   - an entity was destroyed by a biter attack
		 *   - an entity was reconfigured by another player
		 */

		void parse_graphics(const std::string& data);
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
		void rcon_call(std::string func, int player_id, std::string args);
		void rcon_call(std::string func, int action_id, int player_id, std::string args);
		std::string read_packet();
		void parse_packet(const std::string& data);

		// never use these functions directly, use player goals instead
		void set_waypoints(int action_id, int player_id, const std::vector<Pos>& waypoints);
		void set_mining_target(int action_id, int player_id, Entity target);
		void unset_mining_target(int player_id);
		void start_crafting(int action_id, int player_id, std::string recipe, int count=1);
		void place_entity(int player_id, std::string item_name, Pos_f pos, dir8_t direction);
		void insert_to_inventory(int player_id, std::string item_name, int amount, Entity entity, inventory_t inventory);
		void remove_from_inventory(int player_id, std::string item_name, int amount, Entity entity, inventory_t inventory);
		
		[[deprecated("set player goals instead")]]
		void walk_to(int id, const Pos& dest);

		WorldMap<pathfinding::walk_t> walk_map;
		WorldMap<Resource> resource_map;
		std::set< std::shared_ptr<ResourcePatch> > resource_patches;

		void resource_bookkeeping(const Area& area, WorldMap<Resource>::Viewport resview);
		void assert_resource_consistency() const; // only for debugging purposes
		
		std::vector<Player> players;

		const EntityPrototype& get_entity_prototype(std::string name) const { return *entity_prototypes.at(name); }
};

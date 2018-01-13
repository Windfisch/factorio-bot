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

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <deque>
#include <algorithm>
#include <chrono>
#include <cassert>
#include <climits>

#include "factorio_io.h"
#include "pathfinding.hpp"
#include "gui/gui.h"
#include "action.hpp"
#include "util.hpp"

#define READ_BUFSIZE 1024

using std::string;
using std::unordered_map;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;


using namespace std;

const unordered_map<string, Resource::type_t> Resource::types = { {"coal", COAL}, {"iron-ore", IRON}, {"copper-ore", COPPER}, {"stone", STONE}, {"crude-oil", OIL}, {"uranium-ore", URANIUM} };
	
const string Resource::typestr[] = { "NONE", "COAL", "IRON", "COPPER", "STONE", "OIL", "URANIUM", "OCEAN" };

static const string dir8[] = {
	"defines.direction.north",
	"defines.direction.northeast",
	"defines.direction.east",
	"defines.direction.southeast",
	"defines.direction.south",
	"defines.direction.southwest",
	"defines.direction.west",
	"defines.direction.northwest"
};

FactorioGame::FactorioGame(string prefix) : rcon() // initialize with disconnected rcon
{
	factorio_file_prefix = prefix;
}

void FactorioGame::rcon_connect(string host, int port, string password)
{
	rcon.connect(host, port, password);
}

void FactorioGame::rcon_call(string func, string args)
{
	if (!rcon.connected())
	{
		cout << "rcon_call(): not connected, ignoring" << endl;
		return;
	}

	rcon.sendrecv("/c remote.call('windfisch','"+func+"',"+args+")");
}

void FactorioGame::rcon_call(string func, int player_id, string args)
{
	rcon_call(func, to_string(player_id)+","+args);
}

void FactorioGame::rcon_call(string func, int action_id, int player_id, string args)
{
	rcon_call(func, to_string(action_id)+","+to_string(player_id)+","+args);
}

void FactorioGame::set_waypoints(int action_id, int player_id, const std::vector<Pos>& waypoints)
{
	if (waypoints.size() < 1)
	{
		cout << "ignoring zero-size path" << endl;
		return;
	}

	string foo = "";
	for (Pos p : waypoints)
		foo = foo + ",{" + p.str() + "}";

	foo = foo.substr(1);

	rcon_call("set_waypoints", action_id, player_id, "{"+foo+"}");
}

void FactorioGame::set_mining_target(int action_id, int player_id, Entity target)
{
	rcon_call("set_mining_target", action_id, player_id, "'"+target.proto->name+"',{"+target.pos.str()+"}");
}

void FactorioGame::unset_mining_target(int player_id)
{
	rcon_call("set_mining_target", 0, player_id, "'stop',nil");
}

void FactorioGame::start_crafting(int action_id, int player_id, string recipe, int count)
{
	rcon_call("start_crafting", action_id, player_id, "'"+recipe+"',"+to_string(count));
}

void FactorioGame::place_entity(int player_id, std::string item_name, Pos_f pos, dir8_t direction)
{
	rcon_call("place_entity", player_id, "'"+item_name+"',{"+pos.str()+"},"+dir8[direction]);
}

void FactorioGame::insert_to_inventory(int player_id, std::string item_name, int amount, Entity entity, inventory_t inventory)
{
	rcon_call("insert_to_inventory", player_id, "'"+entity.proto->name+"',{"+entity.pos.str()+"}," + inventory_names[inventory] + ",{name='"+item_name+"',count="+to_string(amount)+"}");
}

void FactorioGame::remove_from_inventory(int player_id, std::string item_name, int amount, Entity entity, inventory_t inventory)
{
	rcon_call("remove_from_inventory", player_id, "'"+entity.proto->name+"',{"+entity.pos.str()+"}," + inventory_names[inventory] + ",{name='"+item_name+"',count="+to_string(amount)+"}");
}

[[deprecated]] void FactorioGame::walk_to(int player_id, const Pos& dest)
{
	unique_ptr<action::CompoundGoal> new_goals(new action::CompoundGoal(this, player_id));
	new_goals->subgoals.emplace_back(new action::WalkTo(this, player_id, dest));

	players[player_id].goals = move(new_goals);
	players[player_id].goals->start();
}

string FactorioGame::factorio_file_name()
{
	return factorio_file_prefix + to_string(factorio_file_id) + ".txt";
}

void FactorioGame::change_file_id(int new_id)
{
	factorio_file.close();
	unlink(factorio_file_name().c_str());
	factorio_file_id = new_id;
}

string FactorioGame::remove_line_from_buffer()
{
	auto pos = line_buffer.find('\n');
	string retval = line_buffer.substr(0,pos);
	line_buffer=line_buffer.substr(pos+1);
	return retval;
}

string FactorioGame::read_packet()
{
	if (line_buffer.find('\n') != string::npos)
		return remove_line_from_buffer();

	if (!factorio_file.good() || !factorio_file.is_open())
	{
		cout << "read_packet: file is not good. trying to open '" << factorio_file_prefix+to_string(factorio_file_id) << ".txt'..." << endl;
		factorio_file.clear();
		factorio_file.open(factorio_file_prefix+to_string(factorio_file_id)+".txt");

		if (!factorio_file.good() || !factorio_file.is_open())
		{
			cout << "still failed :(" << endl;
			return "";
		}
	}

	char buf[READ_BUFSIZE];
	while (!factorio_file.eof() && !factorio_file.fail())
	{
		factorio_file.read(buf, READ_BUFSIZE);
		auto n_read = factorio_file.gcount();
		line_buffer.append(buf, n_read);
		if (memchr(buf,'\n', n_read)) break;
	}
	if (factorio_file.fail() && !factorio_file.eof())
		throw std::runtime_error("file reading error");
	
	factorio_file.clear();
	
	if (line_buffer.find('\n') == string::npos)
		return "";
	else
		return remove_line_from_buffer();
}

void FactorioGame::parse_packet(const string& pkg)
{
	if (pkg=="") return;

	auto p1 = pkg.find(' ');
	auto p2 = pkg.find(':');

	if (p1 == string::npos || p1 > p2)
		p1 = p2;

	if (p2 == string::npos)
		throw runtime_error("malformed packet");

	string type = pkg.substr(0, p1);
	
	Area area(pkg.substr(p1+1, p2-(p1+1)));
	
	string data = pkg.substr(p2+2); // skip space

	if (type=="tiles")
		parse_tiles(area, data);
	else if (type=="resources")
		parse_resources(area, data);
	else if (type=="entity_prototypes")
		parse_entity_prototypes(data);
	else if (type=="item_prototypes")
		parse_item_prototypes(data);
	else if (type=="recipes")
		parse_recipes(data);
	else if (type=="graphics")
		parse_graphics(data);
	else if (type=="objects")
		parse_objects(area,data);
	else if (type=="players")
		parse_players(data);
	else if (type=="action_completed")
		parse_action_completed(data);
	else if (type=="mined_item")
		parse_mined_item(data);
	else
		throw runtime_error("unknown packet type '"+type+"'");
}

static vector<string> split(const string& data, char delim=' ')
{
	istringstream str(data);
	string entry;
	vector<string> result;

	while (getline(str, entry, delim))
		result.push_back(std::move(entry));
	
	return result;
}

void FactorioGame::parse_graphics(const string& data)
{
	for (const string& gfxstring : split(data, '|'))
	{
		vector<string> fields = split(gfxstring, '*');

		// fields is either "name, definition_for_any_direction" or
		// "name, def_north, def_east, def_south, def_west" in that order.

		if (fields.size() != 2 && fields.size() != 5)
			throw runtime_error("malformed parse_graphics packet");
		
		const string& name = fields[0];

		auto& defs = graphics_definitions[name];
		if (defs.size() > 0)
			throw runtime_error("duplicate graphics definition in parse_graphics");

		for (size_t i=1; i<fields.size(); i++)
		{
			auto deffields = split(fields[i], ':');
			if (deffields.size() != 8)
				throw runtime_error("malformed graphics definition in parse_graphics packet");

			GraphicsDefinition def;
			def.filename = deffields[0];
			def.width = stoi(deffields[1]);
			def.height = stoi(deffields[2]);
			def.flip_x = def.width<0;
			def.flip_y = def.height<0;
			def.width = abs(def.width);
			def.height = abs(def.height);
			def.shiftx = stof(deffields[3]);
			def.shifty = stof(deffields[4]);
			def.x = stoi(deffields[5]);
			def.y = stoi(deffields[6]);
			def.scale = stof(deffields[7]);

			defs.push_back(def);
		}

		assert(defs.size() == 1 || defs.size() == 4);
	}
}

void FactorioGame::parse_mined_item(const string& data)
{
	vector<string> fields = split(data, ' ');

	if (fields.size() != 3)
		throw runtime_error("malformed mined_item packet");
	
	int id_ = stoi(fields[0]);
	if (id_ < 0 || size_t(id_) >= players.size())
		throw runtime_error("invalid player id in parse_mined_item()");
	unsigned id = unsigned(id_);

	string itemname = fields[1];
	int amount = stoi(fields[2]);

	if (players[id].goals)
		players[id].goals->on_mined_item(itemname, amount);
}

void FactorioGame::parse_players(const string& data)
{
	for (Player& p : players)
		p.connected = false;

	for (const string& entry : split(data,','))
	{
		vector<string> fields = split(entry,' ');

		if (fields.size() != 3)
			throw runtime_error("malformed players packet");

		int id_ = stoi(fields[0]);
		if (id_ < 0)
			throw runtime_error("invalid player id in parse_players()");
		unsigned id = unsigned(id_);
		Pos_f pos = Pos_f(stod(fields[1]), stod(fields[2]));

		if (id >= players.size())
			players.resize(id + 1);

		players[id].connected = true;
		players[id].position = pos;
		players[id].id = id;
	}
}

void FactorioGame::parse_action_completed(const string& data)
{
	vector<string> fields = split(data,' ');
	if (fields.size() != 2)
		throw runtime_error("malformed action_completed packet");
	
	if (fields[0] != "ok" && fields[0] != "fail")
		throw runtime_error("malformed action_completed packet, expected 'ok' or 'fail'");
	
	int action_id = stoi(fields[1]);
	action::PrimitiveAction::mark_finished(action_id);
}

void FactorioGame::parse_entity_prototypes(const string& data)
{
	istringstream str(data);
	string entry;

	while (getline(str, entry, '$')) if (entry!="")
	{
		vector<string> fields = split(entry);

		if (fields.size() != 4)
			throw runtime_error("malformed parse_entity_prototypes packet");

		const string& name = fields[0];
		const string& collision = fields[1];
		Area_f collision_box = Area_f(fields[2]);
		bool mineable = stoi(fields[3]);

		entity_prototypes[name] = new EntityPrototype(name, collision, collision_box, mineable); // no automatic memory management, because prototypes never change.
		max_entity_radius = max(max_entity_radius, collision_box.radius());
	}
}

void FactorioGame::parse_item_prototypes(const string& data)
{
	istringstream str(data);
	string entry;

	while (getline(str, entry, '$')) if (entry!="")
	{
		vector<string> fields = split(entry);

		if (fields.size() != 7)
			throw runtime_error("malformed parse_item_prototypes packet");

		const string& name = fields[0];
		const string& type = fields[1];
		const string& place_result_str = fields[2];
		int stack_size = stoi(fields[3]);
		double fuel_value = stod(fields[4]);
		double speed = stod(fields[5]);
		double durability = stod(fields[6]);

		const EntityPrototype* place_result;
		if (place_result_str != "nil")
			place_result = entity_prototypes.at(place_result_str);
		else
			place_result = NULL;

		item_prototypes[name] = new ItemPrototype(name, type, place_result, stack_size, fuel_value, speed, durability); // no automatic memory management, because prototypes never change.
	}
}

void FactorioGame::parse_recipes(const string& data)
{
	for (string recipestr : split(data,'$'))
	{
		Recipe* recipe = new Recipe(); // no automatic memory management, because prototypes never change.

		vector<string> fields = split(recipestr,' ');
		if (fields.size() != 5)
			throw runtime_error("malformed parse_recipes packet");

		recipe->name = fields[0];
		recipe->enabled = bool(stoi(fields[1]));
		recipe->energy = stod(fields[2]);
		const string& ingredients = fields[3];
		const string& products = fields[4];

		for (string ingstr : split(ingredients, ','))
		{
			vector<string> fields2 = split(ingstr, '*');
			if (fields2.size() != 2)
				throw runtime_error("malformed ingredient string in parse_recipes packet");

			const string& ingredient = fields2[0];
			int amount = stoi(fields2[1]);

			recipe->ingredients.emplace_back(item_prototypes.at(ingredient), amount);
		}

		for (string prodstr : split(products, ','))
		{
			vector<string> fields2 = split(prodstr, '*');
			if (fields2.size() != 2)
				throw runtime_error("malformed product string in parse_recipes packet");

			const string& product = fields2[0];
			double amount = stod(fields2[1]);

			recipe->products.emplace_back(item_prototypes.at(product), amount);
		}

		recipes[recipe->name] = recipe;
	}
}

void FactorioGame::parse_tiles(const Area& area, const string& data)
{
	if (data.length() != 1024+1023)
		throw runtime_error("parse_tiles: invalid length");
	
	auto view = walk_map.view(area.left_top, area.right_bottom, area.left_top);
	auto resview = resource_map.view(area.left_top - Pos(32,32), area.right_bottom + Pos(32,32), Pos(0,0));

	for (int i=0; i<1024; i++)
	{
		int x = i%32;
		int y = i/32;
		view.at(x,y).known = true;
		view.at(x,y).can_walk = (data[2*i]=='0');
		
		if (!view.at(x,y).can_walk) // FIXME: this is used to recognize "water" for now
		{
			Pos abspos = area.left_top + Pos(x,y);
			if (resview.at(abspos).patch_id != NOT_YET_ASSIGNED)
			{
				if (resview.at(abspos).type != Resource::OCEAN)
					throw std::runtime_error("resource at "+abspos.str()+" changed type from "+Resource::typestr[resview.at(abspos).type]+" to "+Resource::typestr[Resource::OCEAN]+". I can't handle that yet");
				//else // ignore this for now. see the fixme above
			}
			else
			{
				assert(resview.at(abspos).resource_patch.lock() == nullptr);
				resview.at(abspos) = Resource(Resource::OCEAN, NOT_YET_ASSIGNED, Entity(abspos, nullptr));
			}
		}
	}
	
	resource_bookkeeping(area, resview);
}

void FactorioGame::resource_bookkeeping(const Area& area, WorldMap<Resource>::Viewport resview)
{
	// group them together
	for (int x=area.left_top.x; x<area.right_bottom.x; x++)
		for (int y=area.left_top.y; y<area.right_bottom.y; y++)
		{
			if (resview.at(x,y).type != Resource::NONE && resview.at(x,y).patch_id == NOT_YET_ASSIGNED)
				floodfill_resources(resview, area, x,y, 
					resview.at(x,y).type == Resource::OIL ? 30 :
					(resview.at(x,y).type == Resource::OCEAN ? 1 : 5));
		}

	assert_resource_consistency();
}

void FactorioGame::parse_objects(const Area& area, const string& data)
{
	istringstream str(data);
	string entry;

	// move all entities in the area from actual_entities to the temporary pending_entities list.
	vector<Entity> pending_entities;
	auto range = actual_entities.range(area);
	for (auto it = range.begin(); it != range.end();)
	{
		pending_entities.push_back(*it);
		it = actual_entities.erase(it);
	}

	struct { int reused=0; int total=0; } stats; // DEBUG only

	// parse the packet's list of objects
	while(getline(str, entry, ',')) if (entry!="")
	{
		vector<string> fields = split(entry);

		if (fields.size() != 4)
			throw runtime_error("malformed parse_objects packet");

		const string& name = fields[0];
		double ent_x = stod(fields[1]);
		double ent_y = stod(fields[2]);
		const string& dir = fields[3];

		if (dir.length() != 1)
			throw runtime_error("invalid direction '"+dir+"' in parse_objects");
		dir4_t dir4;
		switch(dir[0])
		{
			case 'N': dir4 = NORTH; break;
			case 'E': dir4 = EAST; break;
			case 'S': dir4 = SOUTH; break;
			case 'W': dir4 = WEST; break;
			default: throw runtime_error("invalid direction '"+dir+"' in parse_objects");
		};

		Entity ent(Pos_f(ent_x,ent_y), entity_prototypes.at(name), dir4);

		// ignore various ever-moving entities that need to be handled specially
		if (name == "player")
			continue;

		if (!area.contains(ent.pos.to_int_floor()))
		{
			// this indicates a bug in the lua mod
			cout << "FIXME: parse_objects packet contained an object (at "<<ent.pos.str()<<") that does not belong to area (" << area.str() << "). ignoring" << endl;
			continue;
		}

		// try to find ent in pending_entities
		stats.total++;
		for (auto it = pending_entities.begin(); it != pending_entities.end(); )
			if (*it == ent) // found
			{
				ent = *it;
				it = unordered_erase(pending_entities, it);
				stats.reused++;
				break;
			}
			else
				it++;

		// now place ent_ptr in the appropriate list.
		actual_entities.insert(ent);
	}

	if (pending_entities.size() > 0)
		cout << "in parse_objects(" << area.str() << "): reused " << stats.reused << ", had to create " << (stats.total-stats.reused) << " and deleted " << pending_entities.size() << endl; // DEBUG
	
	// finally, update the walkmap; because our entities have a certain size, we must update a larger portion
	update_walkmap(area.expand(int(ceil(max_entity_radius))));
}

void FactorioGame::update_walkmap(const Area& area)
{
	auto view = walk_map.view(area.left_top, area.right_bottom, Pos(0,0));
	//cout << "update_walkmap("<<area.str()<<")"<<endl; // DEBUG

	// clear margins
	for (int x = area.left_top.x; x < area.right_bottom.x; x++)
		for (int y = area.left_top.y; y < area.right_bottom.y; y++)
			for (int i=0; i<4; i++)
				view.at(x,y).margins[i] = 1.;

	for (const auto& ent : actual_entities.range(area))
	{
		// update walk_t information
		if (ent.proto->collides_player)
		{
			Area_f box = ent.collision_box();
			Area outer = box.outer();
			Area inner = Area( outer.left_top+Pos(1,1), outer.right_bottom-Pos(1,1) );
			Area relevant_outer = outer.intersect(area);
			Area relevant_inner = inner.intersect(area);

			// calculate margins
			double rightmargin_of_lefttile = box.left_top.x - outer.left_top.x;
			double leftmargin_of_righttile = outer.right_bottom.x - box.right_bottom.x;
			double bottommargin_of_toptile = box.left_top.y - outer.left_top.y;
			double topmargin_of_bottomtile = outer.right_bottom.y - box.right_bottom.y;

			if (area.contains_y(outer.left_top.y))
			{
				for (int x = relevant_outer.left_top.x; x < relevant_outer.right_bottom.x; x++)
				{
					auto& tile = view.at(x, outer.left_top.y);
					double& margin = tile.margins[TOP];

					if (margin > bottommargin_of_toptile)
						margin = bottommargin_of_toptile;

					if (outer.right_bottom.y-1 != outer.left_top.y)
						tile.margins[BOTTOM] = 0.;
				}

				for (int x = relevant_inner.left_top.x; x < relevant_inner.right_bottom.x; x++)
				{
					auto& tile = view.at(x, outer.left_top.y);
					tile.margins[LEFT] = tile.margins[RIGHT] = 0.;
				}
			}

			if (area.contains_y(outer.right_bottom.y-1))
			{
				for (int x = relevant_outer.left_top.x; x < relevant_outer.right_bottom.x; x++)
				{
					auto& tile = view.at(x, outer.right_bottom.y-1);
					double& margin = tile.margins[BOTTOM];

					if (margin > topmargin_of_bottomtile)
						margin = topmargin_of_bottomtile;

					if (outer.right_bottom.y-1 != outer.left_top.y)
						tile.margins[TOP] = 0.;
				}
				
				for (int x = relevant_inner.left_top.x; x < relevant_inner.right_bottom.x; x++)
				{
					auto& tile = view.at(x, outer.right_bottom.y-1);
					tile.margins[LEFT] = tile.margins[RIGHT] = 0.;
				}
			}

			if (area.contains_x(outer.left_top.x))
			{
				for (int y = relevant_outer.left_top.y; y < relevant_outer.right_bottom.y; y++)
				{
					auto& tile = view.at(outer.left_top.x, y);
					double& margin = tile.margins[LEFT];

					if (margin > rightmargin_of_lefttile)
						margin = rightmargin_of_lefttile;

					if (outer.right_bottom.x-1 != outer.left_top.x)
						tile.margins[RIGHT] = 0.;
				}
				
				for (int y = relevant_inner.left_top.y; y < relevant_inner.right_bottom.y; y++)
				{
					auto& tile = view.at(outer.left_top.x, y);
					tile.margins[TOP] = tile.margins[BOTTOM] = 0.;
				}
			}

			if (area.contains_x(outer.right_bottom.x-1))
			{
				for (int y = relevant_outer.left_top.y; y < relevant_outer.right_bottom.y; y++)
				{
					auto& tile = view.at(outer.right_bottom.x-1, y);
					double& margin = tile.margins[RIGHT];

					if (margin > leftmargin_of_righttile)
						margin = leftmargin_of_righttile;

					if (outer.right_bottom.x-1 != outer.left_top.x)
						tile.margins[LEFT] = 0.;
				}
				
				for (int y = relevant_inner.left_top.y; y < relevant_inner.right_bottom.y; y++)
				{
					auto& tile = view.at(outer.right_bottom.x-1, y);
					tile.margins[TOP] = tile.margins[BOTTOM] = 0.;
				}
			}
		}
	}
}

void FactorioGame::assert_resource_consistency() const // only for debugging purposes
{
	#ifndef NDEBUG
	set<int> ids;
	for (auto patch : resource_patches)
	{
		auto result = ids.insert(patch->patch_id);
		assert(result.second && "non-unique patch_ids");

		for (Pos pos : patch->positions)
		{
			const auto& res = resource_map.at(pos);
			assert(res.resource_patch.lock() == patch);
			assert(res.patch_id == patch->patch_id);
		}
	}
	#endif
}

void FactorioGame::parse_resources(const Area& area, const string& data)
{
	istringstream str(data);
	string entry;
	
	auto view = resource_map.view(area.left_top - Pos(32,32), area.right_bottom + Pos(32,32), Pos(0,0));

	// FIXME: clear and un-assign existing entities before
	// FIXME: handle vanishing resources

	// parse all entities and write them to the WorldMap
	while (getline(str, entry, ',')) if (entry!="")
	{
		auto p1 = entry.find(' ');
		auto p2 = entry.find(' ', p1+1);

		if (p1 == string::npos || p2 == string::npos)
			throw runtime_error("malformed resource entry");
		assert(p1!=p2);
		
		string type_str;
		Resource::type_t type;
		try
		{
			type_str = entry.substr(0,p1);
			type = Resource::types.at(type_str);
		}
		catch (const out_of_range&)
		{
			throw std::runtime_error("resource type '"+entry.substr(0,p1)+"' is not known in Resource::types[] (definition at the top of factorio_io.cpp");
		}
		double xx = stod( entry.substr(p1+1, p2-(p1+1)) );
		double yy = stod( entry.substr(p2+1) );
		int x = int(floor(xx));
		int y = int(floor(yy));

		if (!area.contains(Pos(x,y)))
		{
			// TODO FIXME
			cout << "wtf, " << Pos(x,y).str() << " is not in "<< area.str() << endl;
			break;
		}

		if (view.at(x,y).patch_id != NOT_YET_ASSIGNED)
		{
			if (view.at(x,y).type != type)
				throw std::runtime_error("resource at "+Pos(x,y).str()+" changed type from "+Resource::typestr[view.at(x,y).type]+" to "+Resource::typestr[type]+". I can't handle that yet");
			else // ignore this for now. see the fixme above
				continue;
		}

		view.at(x,y) = Resource(type, NOT_YET_ASSIGNED, Entity(Pos_f(xx,yy), entity_prototypes.at(type_str)));
	}

	resource_bookkeeping(area, view);
}

void FactorioGame::floodfill_resources(WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius)
{
	int id = next_free_resource_id++;
	
	set< shared_ptr<ResourcePatch> > neighbors;
	deque<Pos> todo;
	vector<Pos> orepatch;
	todo.push_back(Pos(x,y));
	int count = 0;
	auto resource_type = view.at(x,y).type;
	cout << "type = "<<Resource::typestr[resource_type]<< " @"<<Pos(x,y).str()<<endl;

	view.at(x,y).floodfill_flag = Resource::FLOODFILL_QUEUED;

	while (!todo.empty())
	{
		Pos p = todo.front();
		todo.pop_front();
		orepatch.push_back(p);
		count++;
		
		if (area.contains(p)) // this is inside the chunk we should fill
		{
			for (int x_offset=-radius; x_offset<=radius; x_offset++)
				for (int y_offset=-radius; y_offset<=radius; y_offset++)
				{
					const int xx = p.x + x_offset;
					const int yy = p.y + y_offset;
					Resource& ref = view.at(xx,yy);

					if (ref.type == resource_type && ref.floodfill_flag == Resource::FLOODFILL_NONE)
					{
						ref.floodfill_flag = Resource::FLOODFILL_QUEUED;
						todo.push_back(Pos(xx,yy));
					}
				}
		}
		else // it's outside the chunk. do not continue with floodfilling.
		{
			Resource& ref = view.at(p);
			auto ptr = ref.resource_patch.lock();
			if (ptr)
				neighbors.insert(ptr);
			else
				throw std::logic_error("Resource entity in map points to a parent resource patch which has vanished!");
		}
	}

	cout << "count=" << count << ", neighbors=" << neighbors.size() << endl;
	

	shared_ptr<ResourcePatch> resource_patch;

	// TODO: update patchlist
	if (neighbors.size() == 0)
	{
		resource_patch = make_shared<ResourcePatch>(orepatch, resource_type, id);
		resource_patches.insert(resource_patch);
	}
	else
	{
		auto largest = std::max_element(neighbors.begin(), neighbors.end(), [](const auto& a, const auto& b) {return a->positions.size() > b->positions.size();});
		resource_patch = *largest;

		cout << "extending existing patch of size " << resource_patch->positions.size() << endl;

		for (const auto& neighbor : neighbors)
		{
			if (neighbor != resource_patch)
			{
				cout << "merging" << endl;
				neighbor->merge_into(*resource_patch);
				// TODO delete neighbor from list of patches. possibly invalidating or migrating goals
				resource_patches.erase(neighbor);
				assert(neighbor.unique()); // now we should hold the last reference, which should go out-of-scope and trigger deletion right in the next line
			}
			else
				cout << "not merging ourself" << endl;
		}

		resource_patch->extend(orepatch);

		cout << "size now " << resource_patch->positions.size() << endl;
	}

	assert(resource_patch);

	auto view2 = view.extend(resource_patch->bounding_box.left_top, resource_patch->bounding_box.right_bottom);

	// clear floodfill_flag
	for (const Pos& p : resource_patch->positions)
	{
		Resource& ref = view2.at(p);
		ref.floodfill_flag = Resource::FLOODFILL_NONE;
		ref.patch_id = resource_patch->patch_id;
		ref.resource_patch = resource_patch;
	}

	cout << "we now have " << resource_patches.size() << " patches" << endl;
}

static float cluster_quality(int diam, size_t coal_size, size_t iron_size, size_t copper_size, size_t stone_size)
{
	const float COAL_SIZE = 25*10;
	const float IRON_SIZE = 25*10;
	const float COPPER_SIZE = 25*10;
	const float STONE_SIZE = 25*5;
	return (diam + 50)
		/ min(1.f,coal_size/COAL_SIZE) // penalize too-small chunks
		/ min(1.f,iron_size/IRON_SIZE)
		/ min(1.f,copper_size/COPPER_SIZE)
		/ min(1.f,stone_size/STONE_SIZE);
}

struct start_mines_t
{
	std::shared_ptr<ResourcePatch> coal;
	std::shared_ptr<ResourcePatch> iron;
	std::shared_ptr<ResourcePatch> copper;
	std::shared_ptr<ResourcePatch> stone;
	Pos water;
	Area area;
};

static start_mines_t find_start_mines(FactorioGame* game, GUI::MapGui* gui, Pos_f pos = Pos_f(0.,0.))
{
	start_mines_t result;

	// FIXME: we treat "known && !can_walk" as water and "known && can_walk" as land. this is suboptimal.
	// FIXME so many magic numbers!

	// enumerate all relevant resource patches, limit the number to 5

	vector< pair<float, shared_ptr<ResourcePatch>> > patches[Resource::N_RESOURCES];
	for (auto patch : game->resource_patches) if (patch->size() >= 30)
	{
		switch (patch->type)
		{
			case Resource::COAL:
			case Resource::IRON:
			case Resource::COPPER:
			case Resource::STONE:
				patches[patch->type].emplace_back( (pos-patch->bounding_box.center()).len(), patch );
				break;
			
			case Resource::NONE:
				// this should never happen
				assert(patch->type != Resource::NONE);
				break;

			default: // empty
				break;
		}
	}

	// only consider the 5 closest patches of each resource
	int radius = 0;
	for (auto i : {Resource::COAL, Resource::IRON, Resource::COPPER, Resource::STONE})
	{
		sort(patches[i].begin(), patches[i].end());
		patches[i].resize(min(patches[i].size(), size_t(5)));

		if (!patches[i].empty())
			radius = max(radius, int(patches[i].back().first));
		else
			throw runtime_error("find_start_mines could not find "+Resource::typestr[i]);
	}


	// find potential water sources in the starting area
	int water_radius = radius + 100;
	auto view = game->walk_map.view(pos-Pos(water_radius+1,water_radius+1), pos+Pos(water_radius+1,water_radius+1), Pos(0,0));
	auto resview = game->resource_map.view(pos-Pos(water_radius+1,water_radius+1), pos+Pos(water_radius+1,water_radius+1), Pos(0,0));
	struct watersource_t { Pos pos; };
	WorldList<watersource_t> watersources;
	for (int x = int(pos.x)-water_radius; x<=int(pos.x)+water_radius; x++)
		for (int y = int(pos.y)-water_radius; y<=int(pos.y)+water_radius; y++)
			if (view.at(x,y).land())
			{
				int n_water = 0;
				size_t water_size = 0;
				for (Pos p : directions4)
					if (view.at(Pos(x,y)+p).water())
					{
						n_water++;
						water_size = resview.at(Pos(x,y)+p).resource_patch.lock()->size();
					}

				if (n_water == 1 && water_size > 100)
					watersources.insert( watersource_t{Pos(x,y)} );
			}
	
	

	float best = 999999999999; // best quality found so far

	// brute-force all 4-tuples and find the best
	for (auto& coal : patches[Resource::COAL])
		for (auto& iron : patches[Resource::IRON])
			for (auto& copper : patches[Resource::COPPER])
				for (auto& stone : patches[Resource::STONE])
				{
					cout << endl;
					cout << "coal:\t" << coal.second->bounding_box.center().str() << endl;
					cout << "iron:\t" << iron.second->bounding_box.center().str() << endl;
					cout << "copper:\t" << copper.second->bounding_box.center().str() << endl;
					cout << "stone:\t" << stone.second->bounding_box.center().str() << endl;

					// calculate the bounding box enclosing the 4-tuple of essential resources
					int x1 = min( coal  .second->bounding_box.center().x,
					         min( iron  .second->bounding_box.center().x,
					         min( stone .second->bounding_box.center().x,
					              copper.second->bounding_box.center().x )));
					int x2 = max( coal  .second->bounding_box.center().x,
					         max( iron  .second->bounding_box.center().x,
					         max( stone .second->bounding_box.center().x,
					              copper.second->bounding_box.center().x )));
					int y1 = min( coal  .second->bounding_box.center().y,
					         min( iron  .second->bounding_box.center().y,
					         min( stone .second->bounding_box.center().y,
					              copper.second->bounding_box.center().y )));
					int y2 = max( coal  .second->bounding_box.center().y,
					         max( iron  .second->bounding_box.center().y,
					         max( stone .second->bounding_box.center().y,
					              copper.second->bounding_box.center().y )));

					int diam = max( x2-x1, y2-y1 );
					if (cluster_quality(diam, coal.second->size(), iron.second->size(), copper.second->size(), stone.second->size()) > best)
						continue; // we're worse than the best even without extending the diameter to include water. discard and continue.

					cout << "diam = " << diam << endl;

					// find the closest water source and calculate the new bounding box
					Pos_f box_center = Pos_f((x1+x2)/2., (y1+y2)/2.);
					Pos best_water_pos;
					int best_water_diam = INT_MAX;
					for (auto water_pos : watersources.around(box_center))
					{
						if ((water_pos.pos - box_center).len() >= 1.5*(best_water_diam-diam/2) )
							break;

						cout << ".";
						
						int new_diam = max(
							max(x2,water_pos.pos.x)-min(x1,water_pos.pos.x),
							max(y2,water_pos.pos.y)-min(y1,water_pos.pos.y) );

						if (new_diam < best_water_diam)
						{
							best_water_diam = new_diam;
							best_water_pos = water_pos.pos;
							cout << "\b*";
						}
					}
					cout << endl;
					cout << "found water at " << best_water_pos.str() << endl;
					cout << "best_water_diam = " << best_water_diam << endl;

					x1 = min(x1, best_water_pos.x);
					y1 = min(y1, best_water_pos.y);
					x2 = max(x2, best_water_pos.x);
					y2 = max(y2, best_water_pos.y);

					float quality = cluster_quality(best_water_diam, coal.second->size(), iron.second->size(), copper.second->size(), stone.second->size());
					if (quality < best)
					{
						best=quality;
						// TODO
						result.coal = coal.second;
						result.copper = copper.second;
						result.stone = stone.second;
						result.iron = iron.second;
						result.water = best_water_pos;
						result.area = Area(x1,y1,x2,y2);
					}
				}
	
	gui->rect( result.area.left_top, result.area.right_bottom, GUI::Color(0,255,0) );
	gui->rect( result.coal->bounding_box.left_top, result.coal->bounding_box.right_bottom, GUI::Color(255,0,255) );
	gui->rect( result.copper->bounding_box.left_top, result.copper->bounding_box.right_bottom, GUI::Color(255,0,255) );
	gui->rect( result.iron->bounding_box.left_top, result.iron->bounding_box.right_bottom, GUI::Color(255,0,255) );
	gui->rect( result.stone->bounding_box.left_top, result.stone->bounding_box.right_bottom, GUI::Color(255,0,255) );
	return result;
}

int main(int argc, const char** argv)
{
	if (argc != 3 && argc != 6)
	{
		cout << "Usage: " << argv[0] << " outfile-prefix datapath [rcon-host rcon-port rcon-password]" << endl;
		cout << "       outfile-prefix: path to the file generated by the mod, minus the '...1.txt' part" << endl;
		cout << "       The rcon options can be left out. In this case, the bot is observe-only." << endl;
		cout << "       This can be useful for debugging, since Factorio doesn't even need to run. It is" << endl;
		cout << "       sufficient to read a previously generated outfile. " << endl;
		return 1;
	}

	FactorioGame factorio(argv[1]);

	cout << "reading static data" << endl;
	while(true)
	{
		string packet = factorio.read_packet();
		if (packet == "STATIC_DATA_END")
			break;
		else
			factorio.parse_packet(packet);
	}
	cout << "done reading static data" << endl;

	bool online = false;
	if (argc == 6)
	{
		string host(argv[3]);
		int port = atoi(argv[4]);
		string pass(argv[5]);

		factorio.rcon_connect(host, port, pass);
		online = true;
	}

	int frame=0;

	GUI::MapGui gui(&factorio, argv[2]);

	// quick read first part of the file before doing any GUI work. Useful for debugging, since reading in 6000 lines will take more than 6 seconds.
	for (int i=0; i<6000; i++) 
	{
		//cout << i << endl;
		factorio.parse_packet(factorio.read_packet());
	}

	while (true)
	{
		factorio.parse_packet( factorio.read_packet() );
		frame++;
		//cout << "frame " << frame << endl; if (frame>1000) break; // useful for profiling with gprof / -pg option, since we must cleanly exit then (not by ^C)
		
		for (auto& player : factorio.players)
		{
			if (player.goals)
			{
				if (player.goals->is_finished())
					player.goals = nullptr;
				else
					player.goals->tick();
			}
		}

		if (online && frame == 1000)
		{
			start_mines_t start_mines = find_start_mines(&factorio, &gui);

			for (auto& player : factorio.players) if (player.connected)
			{
				cout << "WALKING"<< endl;
				player.goals = make_unique<action::CompoundGoal>(&factorio, player.id);
				
				{
				auto parallel = make_unique<action::ParallelGoal>(&factorio, player.id);
				
				// craft an axe
				parallel->subgoals.emplace_back( make_unique<action::CraftRecipe>(
					&factorio, player.id, "iron-axe", 1) );
				
				auto seq = make_unique<action::CompoundGoal>(&factorio, player.id);

				seq->subgoals.emplace_back( make_unique<action::HaveItem>(
					&factorio, player.id, "raw-wood", 60) );

				// make 2 chests, place a drill and a furnace on the iron field and manually make 15 stone
				auto parallel2 = make_unique<action::ParallelGoal>(&factorio, player.id);
				parallel2->subgoals.emplace_back( make_unique<action::CraftRecipe>(
					&factorio, player.id, "wooden-chest", 2) );

				auto seq2 = make_unique<action::CompoundGoal>(&factorio, player.id);

				Pos iron_position = start_mines.iron->positions[0]; // FIXME
				seq2->subgoals.emplace_back( make_unique<action::WalkAndPlaceEntity>(
					&factorio, player.id, "burner-mining-drill", iron_position, d8_NORTH) );
				seq2->subgoals.emplace_back( make_unique<action::WalkAndPlaceEntity>(
					&factorio, player.id, "stone-furnace", iron_position-Pos(0,2)) );

				seq2->subgoals.emplace_back( make_unique<action::PutToInventory>(
					&factorio, player.id, "raw-wood", 20,
					Entity(iron_position, &factorio.get_entity_prototype("burner-mining-drill")),
					INV_FUEL) );
				seq2->subgoals.emplace_back( make_unique<action::PutToInventory>(
					&factorio, player.id, "raw-wood", 10,
					Entity(iron_position-Pos(0,2), &factorio.get_entity_prototype("stone-furnace")),
					INV_FUEL) );
				
				seq2->subgoals.emplace_back( make_unique<action::WalkAndMineResource>(
					&factorio, player.id, start_mines.stone, 15) );

				parallel2->subgoals.emplace_back( move(seq2) );

				seq->subgoals.push_back( move(parallel2) );

				parallel->subgoals.push_back(move(seq));

				player.goals->subgoals.emplace_back(move(parallel));
				}
				
				player.goals->start();
			}
		}
		
		GUI::wait(0.001);
	}
}

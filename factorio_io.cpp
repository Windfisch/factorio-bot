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

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cstring>
#include <set>
#include <deque>
#include <algorithm>
#include <cassert>

#include "factorio_io.h"
#include "pathfinding.hpp"
#include "action.hpp"
#include "util.hpp"
#include "split.hpp"
#include "safe_cast.hpp"
#include "logging.hpp"

#define READ_BUFSIZE 1024

using namespace std;

const unordered_map<string, Resource::type_t> Resource::types = { {"coal", COAL}, {"iron-ore", IRON}, {"copper-ore", COPPER}, {"stone", STONE}, {"crude-oil", OIL}, {"uranium-ore", URANIUM} };

const string Resource::typestr[] = { "NONE", "COAL", "IRON", "COPPER", "STONE", "OIL", "URANIUM", "OCEAN" };

static const string dir8str[] = {
	"defines.direction.north",
	"defines.direction.northeast",
	"defines.direction.east",
	"defines.direction.southeast",
	"defines.direction.south",
	"defines.direction.southwest",
	"defines.direction.west",
	"defines.direction.northwest"
};
static const string dir4str[] = {
	"defines.direction.north",
	"defines.direction.east",
	"defines.direction.south",
	"defines.direction.west",
};

FactorioGame::FactorioGame(string prefix) : rcon() // initialize with disconnected rcon
{
	factorio_file_prefix = prefix;
}


// FIXME how inefficient :(
const ItemPrototype* FactorioGame::get_item_for(const EntityPrototype* ent_proto) const
{
	for (const auto& [_,item] : item_prototypes)
		if (item->place_result == ent_proto)
			return item.get();
	throw runtime_error("Could not find an item to place the entity '"+ent_proto->name+"'");
}

const Recipe* FactorioGame::get_recipe_for(const ItemPrototype* item) const
{
	const Recipe* best = nullptr;
	double best_ratio = -1.;

	for (const Recipe* recipe : get_recipes_for(item))
	{
		double amount_wanted = 0, amount_junk = 0;
		for (auto [product, amount] : recipe->products)
		{
			if (product == item)
				amount_wanted = amount;
			else
				amount_junk += amount;
		}
		double ratio = amount_wanted / (amount_junk + amount_wanted);
		if (ratio > best_ratio)
		{
			best_ratio = ratio;
			best = recipe;
		}
	}

	if (best == nullptr)
		throw runtime_error("Could not find a recipe to generate "+item->name);
	
	return best;
}

// FIXME how inefficient :(
const std::vector<const Recipe*> FactorioGame::get_recipes_for(const ItemPrototype* item) const
{
	vector<const Recipe*> result;

	for (const auto& [_,recipe] : recipes)
		if (/* FIXME recipe->enabled && */ recipe->balance_for(item) > 0)
			result.push_back(recipe.get());

	return result;
}

void FactorioGame::rcon_connect(string host, int port, string password)
{
	rcon.connect(host, port, password);
}

void FactorioGame::rcon_call(string func, string args)
{
	Logger log("core");
	if (!rcon.connected())
	{
		log << "rcon_call(): not connected, ignoring" << endl;
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
	Logger log("core");

	if (waypoints.size() < 1)
	{
		log << "ignoring zero-size path" << endl;
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

void FactorioGame::place_entity(int player_id, std::string item_name, Pos_f pos, dir4_t direction)
{
	rcon_call("place_entity", player_id, "'"+item_name+"',{"+pos.str()+"},"+dir4str[direction]);
}

void FactorioGame::insert_to_inventory(int player_id, std::string item_name, int amount, Entity entity, inventory_t inventory)
{
	rcon_call("insert_to_inventory", player_id, "'"+entity.proto->name+"',{"+entity.pos.str()+"}," + inventory_names[inventory] + ",{name='"+item_name+"',count="+to_string(amount)+"}");
}

void FactorioGame::remove_from_inventory(int player_id, std::string item_name, int amount, Entity entity, inventory_t inventory)
{
	rcon_call("remove_from_inventory", player_id, "'"+entity.proto->name+"',{"+entity.pos.str()+"}," + inventory_names[inventory] + ",{name='"+item_name+"',count="+to_string(amount)+"}");
}

[[deprecated]] void FactorioGame::walk_to(int /*player_id*/, const Pos& /*dest*/)
{
	// TODO FIXME: create new task with very high priority and force reschedule
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
	Logger log("core");

	if (line_buffer.find('\n') != string::npos)
		return remove_line_from_buffer();

	if (!factorio_file.good() || !factorio_file.is_open())
	{
		log << "read_packet: file is not good. trying to open '" << factorio_file_prefix+to_string(factorio_file_id) << ".txt'..." << endl;
		factorio_file.clear();
		factorio_file.open(factorio_file_prefix+to_string(factorio_file_id)+".txt");

		if (!factorio_file.good() || !factorio_file.is_open())
		{
			log << "still failed :(" << endl;
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

void FactorioGame::resolve_references_to_items()
{
	// FIXME: I really don't like this const_cast :(
	for (auto& [name, proto_] : entity_prototypes)
	{
		EntityPrototype* proto = const_cast<EntityPrototype*>(proto_.get());
		for (const auto& [item_str, amount] : proto->mine_results_str)
			proto->mine_results.emplace(item_prototypes.at(item_str).get(), amount);
	}
}

bool FactorioGame::parse_packet(const string& pkg)
{
	Logger log("core");

	if (pkg=="") return false;

	auto colon = pkg.find(':');

	if (colon == string::npos)
		throw runtime_error("malformed packet: missing colon");

	vector<string> prelude = split(pkg.substr(0, colon), ' ');
	string data = pkg.substr(colon+2); // skip space

	if (prelude.size() != 2 && prelude.size() != 3)
		throw runtime_error("malformed packed: invalid prelude");

	string tickstr = prelude[0];

	int tick = stoi(tickstr);
	if (last_tick > tick)
		log << "wtf, tick decreased from " << last_tick << " to " << tick << endl;
	last_tick = tick;

	string type = prelude[1];

	Area area;
	if (prelude.size() >= 3)
		area = Area(prelude[2]);
	
	if (type=="tiles")
		parse_tiles(area, data);
	else if (type=="resources")
		parse_resources(area, data);
	else if (type=="entity_prototypes")
		parse_entity_prototypes(data);
	else if (type=="item_prototypes")
	{
		parse_item_prototypes(data);
		resolve_references_to_items();
	}
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
	else if (type=="inventory_changed")
		parse_inventory_changed(data);
	else if (type=="item_containers")
		parse_item_containers(data);
	else if (type=="tick")
		return true;
	else
		throw runtime_error("unknown packet type '"+type+"'");
	
	return false;
}

void FactorioGame::parse_item_containers(const string& data_str)
{
	Logger log("container");

	for (const string& container : split(data_str, ',')) if (!container.empty())
	{
		auto [name, ent_x, ent_y, contents] = unpack<string,double,double,string>(container, ' ');

		if (auto entity = actual_entities.search_or_null(Entity(Pos_f(ent_x,ent_y),entity_prototypes.at(name).get())))
		{
			if (auto* data = entity->data_or_null<ContainerData>())
			{
				data->inventories.clear();

				for (const string& inv_string : split(contents, '+'))
				{
					auto [invtype_str, invcontent] = unpack<string, string>(inv_string, '=');
					inventory_t invtype = inventory_types.at("defines.inventory."+invtype_str);

					for (const string& itemstack : split(invcontent, '%'))
					{
						auto [item, amount] = unpack<string, size_t>(itemstack,':');
						auto [_,inserted] = data->inventories.insert(invtype, item_prototypes.at(item).get(), amount);
						if (!inserted)
							throw runtime_error("malformed parse_item_containers packet: duplicate item");
					}
				}
			}
			else
				log << "wtf, got inventory update for " << entity->str() << ", but it has no ContainerData associated" << endl;
		}
		else
		{
			log << "WTF, got container update from an entity which we don't know... ignoring it for now" << endl;
		}
	}
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
	auto [id_, itemname, amount] = unpack<int, string, int>(data, ' ');

	if (id_ < 0 || size_t(id_) >= players.size())
		throw runtime_error("invalid player id in parse_mined_item()");
	unsigned id = unsigned(id_);

	if (players[id].actions)
		players[id].actions->on_mined_item(itemname, amount);
}

void FactorioGame::parse_inventory_changed(const string& data)
{
	Logger log("inventory");

	std::set<int> affected_players;
	for (const string& update : split(data, ' '))
	{ 
		auto [player_id, item, diff, owner_str] = unpack<int,string,int,string>(update, ',');
		bool has_owner = owner_str!="x";
		int owning_action_id = has_owner ? stoi(owner_str) : -1;
		const ItemPrototype* proto = item_prototypes.at(item).get();
		affected_players.insert(player_id);
		
		if (size_t(player_id) >= players.size())
		{
			log << "HACK: parse_inventory_changed() on a player that does not exist yet. (this can be because it has just joined and got its inventory pre-filled by the game.)... resizing players." << endl;
			players.resize(player_id+1); // FIXME
		}

		if (player_id < 0 || size_t(player_id) >= players.size())
			throw runtime_error("invalid player id in parse_mined_item()");

		TaggedAmount& content = players[player_id].inventory[proto];

		if (long(content.amount) < -diff)
			throw runtime_error("inventory desync detected: game removed more '"+item+"' items ("+to_string(diff)+") than we actually have ("+to_string(content.amount)+")");

		content.amount += diff;
		if (has_owner && diff > 0)
		{
			auto action = action::registry.get(owning_action_id);
			
			if (action)
			{
				if (action->owner.has_value())
				{
					size_t added = content.add_claim(*action->owner, diff);
					if (added != safe_cast<size_t>(diff))
						throw runtime_error("inventory desync detected: game added "+to_string(diff)+"x '"+item+"' with an owning task, but only "+to_string(added)+" could be claimed");
				}
				else
					log << "WARN: action with the action id " << owning_action_id << " has no owning task, yet it changed the inventory?" << endl;
			}
			else
				log << "WARN: could not find action associated with action id " << owning_action_id << endl;
		}
	}
	
	for (int player : affected_players)
	{
		Logger log2("inventory_dump");
		log2 << "parse_inventory_changed(), player #"<<player<<"'s new inventory is " << endl;
		players[player].inventory.dump();
	}
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
	Logger log("core");

	auto [type, action_id] = unpack<string,int>(data,' ');

	if (type != "ok" && type != "fail")
		throw runtime_error("malformed action_completed packet, expected 'ok' or 'fail'");
	
	if (auto action = action::registry.get(action_id))
		action->finished = true;
	else
		log << "WARN: unknown action finished (id=" << action_id << ")" << endl;
}

void FactorioGame::parse_entity_prototypes(const string& data)
{
	for (string entry : split(data, '$')) if (entry!="")
	{
		auto [name, type, collision, collision_box, mine_results] = unpack<string,string,string,Area_f,string>(entry);

		bool mineable = (mine_results != "-");
		vector< pair<string, size_t> > mine_results_str;
		if (mineable)
		{
			for (string product : split(mine_results, ','))
			{
				const auto [item, amount] = unpack<string, int>(product, ':');
				mine_results_str.emplace_back(item, amount);
			}
		}

		entity_prototypes[name] = make_unique<EntityPrototype>(name, type, collision, collision_box, mineable, mine_results_str); // prototypes will never change.
		max_entity_radius = max(max_entity_radius, collision_box.radius());
	}
}

void FactorioGame::parse_item_prototypes(const string& data)
{
	for (string entry : split(data, '$')) if (entry!="")
	{
		auto [name, type, place_result_str, stack_size, fuel_value, speed, durability] = unpack<string,string,string,int,double,double,double>(entry);

		const EntityPrototype* place_result;
		if (place_result_str != "nil")
			place_result = entity_prototypes.at(place_result_str).get();
		else
			place_result = nullptr;

		item_prototypes[name] = make_unique<ItemPrototype>(name, type, place_result, stack_size, fuel_value, speed, durability); // prototypes will never change.
	}
}

void FactorioGame::parse_recipes(const string& data)
{
	for (string recipestr : split(data,'$'))
	{
		auto recipe = make_unique<Recipe>();

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
			auto [ingredient, amount] = unpack<string,int>(ingstr, '*');
			recipe->ingredients.emplace_back(item_prototypes.at(ingredient).get(), amount);
		}

		for (string prodstr : split(products, ','))
		{
			auto [product, amount] = unpack<string,double>(prodstr,'*');
			recipe->products.emplace_back(item_prototypes.at(product).get(), amount);
		}

		recipes[recipe->name] = move(recipe);
	}
}

void FactorioGame::update_resource_field(Resource& entry, Resource::type_t new_type, Pos position, Entity entity)
{
	Logger log("resource_fields");

	auto old_type = entry.type;

	if (old_type != Resource::NONE)
	{
		if (auto patch = entry.resource_patch.lock())
		{
			patch->remove(position);
			entry = Resource();

			if (patch->size() == 0)
			{
				log << "patch of type " << Resource::typestr[patch->type] << " has vanished" << endl;
				// remove patch
				auto iter = resource_patches.find(patch);
				assert(iter != resource_patches.end());
				resource_patches.erase(iter);
			}
		}
		else
		{
			log << "wtf, could not lock resource weak reference" << endl;
		}
	}

	if (new_type != Resource::NONE)
	{
		assert(entry.patch_id == NOT_YET_ASSIGNED);
		entry = Resource(new_type, NOT_YET_ASSIGNED, entity);
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
		
		bool is_water = !view.at(x,y).can_walk; // FIXME: this is used to recognize "water" for now

		Pos abspos = area.left_top + Pos(x,y);
		Resource::type_t type = is_water ? Resource::OCEAN : Resource::NONE;
		auto& entry = resview.at(abspos);
		Resource::type_t old_type = entry.type;

		// only care for OCEAN -> something-else and not-ocean -> OCEAN changes here, because
		// all other resources are handled elsewhere.
		if ((old_type == Resource::OCEAN) != (type == Resource::OCEAN))
			update_resource_field(entry, type, abspos, Entity(Entity::nullent_tag{}, abspos));
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
	Logger log("objects");

	// clean up pending_entities
	for (auto it = pending_entities.begin(); it != pending_entities.end(); )
	{
		if (it->last_valid_tick < get_tick())
		{
			log << "removing timed out pending entity" << endl;
			it = unordered_erase(pending_entities, it);
		}
		else
			it++;
	}

	bool debug_pending_entities = false;
	if (pending_entities.size() > 0)
	{
		// FIXME DEBUG only
		debug_pending_entities = true;
		log << "pending_entities.size() == " << pending_entities.size() << endl;
		for (const auto& pe : pending_entities)
		{
			log << "-> pending entity: " << pe.entity.str() << " (best before " << pe.last_valid_tick << ")" << endl;
			if (const ContainerData* data = pe.entity.data_or_null<ContainerData>())
				log << "  fuel_is_output = " << data->fuel_is_output << endl;
		}
	}

	// move all entities in the area from actual_entities to the temporary pending_entities list.
	auto range = actual_entities.within_range(area);
	for (auto it = range.begin(); it != range.end();)
	{
		pending_entities.push_back({get_tick(), std::move(*it)});
		it = actual_entities.erase(it);
	}

	struct { int reused=0; int total=0; } stats; // DEBUG only

	// parse the packet's list of objects
	for (string entry : split(data, ',')) if (entry!="")
	{
		auto [name,ent_x,ent_y,dir] = unpack<string,double,double,string>(entry);

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

		Entity ent(Pos_f(ent_x,ent_y), entity_prototypes.at(name).get(), dir4);

		// ignore various ever-moving entities that need to be handled specially
		if (name == "player")
			continue;

		if (!area.contains(ent.pos.to_int_floor()))
		{
			// this indicates a bug in the lua mod
			log << "FIXME: parse_objects packet contained an object (at "<<ent.pos.str()<<") that does not belong to area (" << area.str() << "). ignoring" << endl;
			continue;
		}

		// try to find ent in pending_entities
		stats.total++;
		for (auto it = pending_entities.begin(); it != pending_entities.end(); )
			if (it->entity.mostly_equals(ent)) // found (modulo rotation and data_ptr)
			{
				ent.takeover_data(it->entity);
				it = unordered_erase(pending_entities, it);
				stats.reused++;
				break;
			}
			else
				it++;

		// now place ent_ptr in the appropriate list.
		actual_entities.insert(std::move(ent));
	}

	if (debug_pending_entities)
		log << "in parse_objects(" << area.str() << "): reused " << stats.reused << ", had to create " << (stats.total-stats.reused) << " and deleted " << pending_entities.size() << endl; // DEBUG
	
	
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

	for (const auto& ent : actual_entities.within_range(area))
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
	assert(area.size() == Pos(32,32));
	Logger log("objects");
	
	auto view = resource_map.view(area.left_top - Pos(32,32), area.right_bottom + Pos(32,32), Pos(0,0));

	struct resource_tile
	{
		Resource::type_t type = Resource::NONE;
		Entity entity = Entity(Entity::nullent_tag{});
	} chunk[32][32] = {};

	// parse all entities and write them to the WorldMap
	for (string entry : split(data, ',')) if (entry!="")
	{
		auto [type_str,xx,yy] = unpack<string,double,double>(entry, ' ');
		
		Resource::type_t type = Resource::types.at(type_str);
		Pos pos = {int(floor(xx)), int(floor(yy))};

		if (!area.contains(pos))
		{
			// TODO FIXME
			log << "wtf, " << pos.str() << " is not in "<< area.str() << endl;
			continue;
		}

		Pos relpos = pos - area.left_top;

		if (chunk[relpos.y][relpos.x].type != Resource::NONE)
			log << "wtf, " << pos.str() << " has a conflicting resource entry?! (broken packet?)" << endl;

		chunk[relpos.y][relpos.x].type = type;
		chunk[relpos.y][relpos.x].entity = Entity(Pos_f(xx,yy), entity_prototypes.at(type_str).get());
	}

	for (int x = area.left_top.x; x < area.right_bottom.x; x++)
		for (int y = area.left_top.y; y < area.right_bottom.y; y++)
		{
			Resource::type_t type = chunk[y-area.left_top.y][x-area.left_top.x].type;
			const Entity& entity = chunk[y-area.left_top.y][x-area.left_top.x].entity;
			auto& entry = view.at(x,y);

			Resource::type_t old_type = entry.type;
		
			// ignore OCEAN->NONE changes, because OCEAN is special and not handled here,
			if (type != old_type && !(type == Resource::NONE && old_type == Resource::OCEAN))
				update_resource_field(entry, type, Pos(x,y), entity);
		}

	resource_bookkeeping(area, view);
}

void FactorioGame::floodfill_resources(WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius)
{
	Logger log("floodfill_resources");

	int id = next_free_resource_id++;

	if (view.at(x,y).patch_id != NOT_YET_ASSIGNED)
		throw std::invalid_argument("floodfill_resources() must be called with a NOT_YET_ASSIGNED position");
	
	set< shared_ptr<ResourcePatch> > neighbors;
	deque<Pos> todo;
	vector<Pos> orepatch;
	todo.push_back(Pos(x,y));
	int count = 0;
	auto resource_type = view.at(x,y).type;
	log << "type = "<<Resource::typestr[resource_type]<< " @"<<Pos(x,y).str()<<endl;

	view.at(x,y).floodfill_flag = Resource::FLOODFILL_QUEUED;

	while (!todo.empty())
	{
		Pos p = todo.front();
		todo.pop_front();
		assert(area.contains(p));
		orepatch.push_back(p);
		count++;

		for (int x_offset=-radius; x_offset<=radius; x_offset++)
			for (int y_offset=-radius; y_offset<=radius; y_offset++)
			{
				const int xx = p.x + x_offset;
				const int yy = p.y + y_offset;
				Resource& ref = view.at(xx,yy);

				if (ref.type == resource_type && ref.floodfill_flag == Resource::FLOODFILL_NONE)
				{
					ref.floodfill_flag = Resource::FLOODFILL_QUEUED;

					if (ref.patch_id == NOT_YET_ASSIGNED)
					{
						assert(area.contains(Pos(xx,yy)));
						todo.push_back(Pos(xx,yy));
					}
					else
					{
						auto ptr = ref.resource_patch.lock();
						if (ptr)
							neighbors.insert(ptr);
						else
							throw std::logic_error("Resource entity in map points to a parent resource patch which has vanished!");
					}
				}
			}
	}

	log << "count=" << count << ", neighbors=" << neighbors.size() << endl;
	

	shared_ptr<ResourcePatch> resource_patch;

	if (neighbors.size() == 0)
	{
		resource_patch = make_shared<ResourcePatch>(orepatch, resource_type, id);
		resource_patches.insert(resource_patch);
	}
	else
	{
		auto largest = std::max_element(neighbors.begin(), neighbors.end(), [](const auto& a, const auto& b) {return a->positions.size() > b->positions.size();});
		resource_patch = *largest;

		log << "extending existing patch of size " << resource_patch->positions.size() << endl;

		for (const auto& neighbor : neighbors)
		{
			if (neighbor != resource_patch)
			{
				log << "merging" << endl;
				neighbor->merge_into(*resource_patch);
				resource_patches.erase(neighbor);
				// FIXME??? assert(neighbor.unique()); // now we should hold the last reference, which should go out-of-scope and trigger deletion right in the next line
			}
			else
			{
				log << "not merging ourself" << endl;
			}
		}

		resource_patch->extend(orepatch);

		log << "size now " << resource_patch->positions.size() << endl;
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

	log << "we now have " << resource_patches.size() << " patches" << endl;
}

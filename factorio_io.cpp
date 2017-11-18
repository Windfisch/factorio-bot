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
	
const string Resource::typestr[] = { "NONE", "COAL", "IRON", "COPPER", "STONE", "OIL", "URANIUM" };

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

	for (int i=0; i<1024; i++)
	{
		int x = i%32;
		int y = i/32;
		view.at(x,y).known = true;
		view.at(x,y).can_walk = (data[2*i]=='0');
	}
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

		if (fields.size() != 3)
			throw runtime_error("malformed parse_objects packet");

		const string& name = fields[0];
		double ent_x = stod(fields[1]);
		double ent_y = stod(fields[2]);

		Entity ent(Pos_f(ent_x,ent_y), entity_prototypes.at(name));

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


void FactorioGame::parse_resources(const Area& area, const string& data)
{
	istringstream str(data);
	string entry;
	
	auto view = resource_map.view(area.left_top - Pos(32,32), area.right_bottom + Pos(32,32), Pos(0,0));

	// FIXME: clear and un-assign existing entities before

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

		view.at(x,y) = Resource(type, NOT_YET_ASSIGNED, Entity(Pos_f(xx,yy), entity_prototypes.at(type_str)));
	}

	// group them together
	for (int x=area.left_top.x; x<area.right_bottom.x; x++)
		for (int y=area.left_top.y; y<area.right_bottom.y; y++)
		{
			if (view.at(x,y).type != Resource::NONE && view.at(x,y).patch_id == NOT_YET_ASSIGNED)
				floodfill_resources(view, area, x,y, view.at(x,y).type == Resource::OIL ? 30 : 5 );
		}
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
	cout << "type = "<<resource_type<< " @"<<Pos(x,y).str()<<endl;

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
	for (const Pos& p : resource_patch->positions) // TODO FIXME: view might be too small
	{
		Resource& ref = view2.at(p);
		ref.floodfill_flag = Resource::FLOODFILL_NONE;
		ref.patch_id = resource_patch->patch_id;
		ref.resource_patch = resource_patch;
	}

	cout << "we now have " << resource_patches.size() << " patches" << endl;
}

int main(int argc, const char** argv)
{
	if (argc != 2 && argc != 5)
	{
		cout << "Usage: " << argv[0] << " outfile-prefix [rcon-host rcon-port rcon-password]" << endl;
		cout << "       outfile-prefix: path to the file generated by the mod, minus the '...1.txt' part" << endl;
		cout << "       The rcon options can be left out. In this case, the bot is observe-only." << endl;
		cout << "       This can be useful for debugging, since Factorio doesn't even need to run. It is" << endl;
		cout << "       sufficient to read a previously generated outfile. " << endl;
		return 1;
	}

	FactorioGame factorio(argv[1]);
	if (argc == 5)
	{
		string host(argv[2]);
		int port = atoi(argv[3]);
		string pass(argv[4]);

		factorio.rcon_connect(host, port, pass);
	}

	int frame=0;

	GUI::MapGui gui(&factorio);

	
	// quick read first part of the file before doing any GUI work. Useful for debugging, since reading in 6000 lines will take more than 6 seconds.
	for (int i=0; i<6000; i++) factorio.parse_packet(factorio.read_packet());

	while (true)
	{
		factorio.parse_packet( factorio.read_packet() );
		frame++;
		//if (frame>6000) break; // useful for profiling with gprof / -pg option, since we must cleanly exit then (not by ^C)
		
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

		if (frame == 1000)
		{
			for (auto& player : factorio.players) if (player.connected)
			{
				// search closest coal patch and mine some coal
				
				auto& patches = factorio.resource_patches; // shorthand

				auto closest_coal_patch = min_element_if(
					patches,
					[](auto patch) { return patch->type == Resource::COAL; } ,
					[&player](auto a, auto b) {
						return (a->bounding_box.center().to_double() - player.position).len()
							< (b->bounding_box.center().to_double() - player.position).len(); }
				);
				if (closest_coal_patch == patches.end())
				{
					cout << "could not find a coal patch :(" << endl;
					continue;
				}

				vector<Entity> nearby_trees;
				for (const auto& closest_tree : factorio.actual_entities.around(player.position))
					if (closest_tree.proto->name.substr(0,4) == "tree")
					{
						for (const auto& tree : factorio.actual_entities.around(closest_tree.pos))
							if (tree.proto->name.substr(0,4) == "tree")
							{
								nearby_trees.push_back(tree);
								if (nearby_trees.size() >= 5)
									break;
							}
						break;
					}

				cout << "WALKING"<< endl;
				player.goals = make_unique<action::CompoundGoal>(&factorio, player.id);
				
				{
				auto parallel = make_unique<action::ParallelGoal>(&factorio, player.id);
				
				parallel->subgoals.emplace_back( make_unique<action::CraftRecipe>(
					&factorio, player.id, "iron-axe", 2) );
				
				auto seq = make_unique<action::CompoundGoal>(&factorio, player.id);

				for (const auto& tree : nearby_trees)
					seq->subgoals.emplace_back( make_unique<action::WalkAndMineObject>(
						&factorio, player.id, tree) );

				seq->subgoals.emplace_back( make_unique<action::WalkAndMineResource>(
					&factorio, player.id, *closest_coal_patch, 5) );

				seq->subgoals.emplace_back( make_unique<action::WalkAndPlaceEntity>(
					&factorio, player.id, "stone-furnace", Pos_f(0,0)) );

				parallel->subgoals.push_back(move(seq));

				player.goals->subgoals.emplace_back(move(parallel));
				}
				
				player.goals->start();
			}
		}
		
		GUI::wait(0.001);
	}
}

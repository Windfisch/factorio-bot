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

#include "factorio_io.h"
#include "pathfinding.hpp"
#include "gui/gui.h"

#define READ_BUFSIZE 1024

using std::string;
using std::unordered_map;
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;


using namespace std;

const unordered_map<string, Resource::type_t> Resource::types = { {"coal", COAL}, {"iron-ore", IRON}, {"copper-ore", COPPER}, {"stone", STONE}, {"crude-oil", OIL} };


FactorioGame::FactorioGame(string prefix) : rcon() // initialize with disconnected rcon
{
	factorio_file_prefix = prefix;
}

void FactorioGame::rcon_connect(string host, int port, string password)
{
	rcon.connect(host, port, password);
}

void FactorioGame::set_waypoints(int player_id, const std::vector<Pos>& waypoints)
{
	if (!rcon.connected())
	{
		cout << "set_waypoints(): not connected, ignoring" << endl;
		return;
	}

	if (waypoints.size() < 1)
	{
		cout << "ignoring zero-size path" << endl;
		return;
	}

	string foo = "";
	for (Pos p : waypoints)
		foo = foo + ",{" + p.str() + "}";

	foo = foo.substr(1);

	rcon.sendrecv("/c remote.call('windfisch', 'set_waypoints', {"+foo+"})");
}

void FactorioGame::walk_to(int player_id, const Pos& dest)
{
	set_waypoints(player_id, a_star(players[player_id].position.to_int(), dest, walk_map, 0.4+0.1));
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
	else if (type=="objects")
		parse_objects(area,data);
	else if (type=="players")
		parse_players(data);
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

void FactorioGame::parse_players(const string& data)
{
	for (Player& p : players)
		p.connected = false;

	for (const string& entry : split(data,','))
	{
		vector<string> fields = split(entry,' ');

		if (fields.size() != 3)
			throw runtime_error("malformed players packet");

		int id = stoi(fields[0]);
		Pos_f pos = Pos_f(stod(fields[1]), stod(fields[2]));

		if (id >= players.size())
			players.resize(id + 1);

		players[id].connected = true;
		players[id].position = pos;
	}
}

void FactorioGame::parse_entity_prototypes(const string& data)
{
	cout << data << endl;
	istringstream str(data);
	string entry;

	while (getline(str, entry, '$')) if (entry!="")
	{
		vector<string> fields = split(entry);

		if (fields.size() != 3)
			throw runtime_error("malformed parse_entity_prototypes packet");

		const string& name = fields[0];
		const string& collision = fields[1];
		Area_f collision_box = Area_f(fields[2]);

		entity_prototypes[name] = new EntityPrototype(collision, collision_box); // no automatic memory management, because prototypes never change.
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

	auto view = walk_map.view(area.left_top, area.right_bottom, Pos(0,0));


	// clear margins
	for (int x = area.left_top.x; x < area.right_bottom.x; x++)
		for (int y = area.left_top.y; y < area.right_bottom.y; y++)
			for (int i=0; i<4; i++)
				view.at(x,y).margins[i] = 1.;

	
	while(getline(str, entry, ',')) if (entry!="")
	{
		vector<string> fields = split(entry);

		if (fields.size() != 3)
			throw runtime_error("malformed parse_objects packet");

		const string& name = fields[0];
		double ent_x = stod(fields[1]);
		double ent_y = stod(fields[2]);

		Entity ent(Pos_f(ent_x,ent_y), entity_prototypes.at(name));

		// TODO FIXME: insert this in some list, and decouple the following code from this function
		
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
		
		Resource::type_t type = Resource::types.at( entry.substr(0,p1) );
		int x = int(floor(stod( entry.substr(p1+1, p2-(p1+1)) )));
		int y = int(floor(stod( entry.substr(p2+1) )));

		if (!area.contains(Pos(x,y)))
		{
			// TODO FIXME
			cout << "wtf, " << Pos(x,y).str() << " is not in "<< area.str() << endl;
			break;
		}

		view.at(x,y) = Resource(type, NOT_YET_ASSIGNED);
	}

	// group them together
	for (int x=area.left_top.x; x<area.right_bottom.x; x++)
		for (int y=area.left_top.y; y<area.right_bottom.y; y++)
		{
			if (view.at(x,y).type != Resource::NONE && view.at(x,y).patch_id == NOT_YET_ASSIGNED)
				floodfill_resources(view, area, x,y, view.at(x,y).type == Resource::OIL ? 30 : 5 );
		}
}

struct ResourcePatch
{
	vector<Pos> positions;
	Resource::type_t type;
	int patch_id;
	Area bounding_box;

	ResourcePatch(const vector<Pos>& positions_, Resource::type_t t, int id) : positions(std::move(positions_)), type(t), patch_id(id)
	{
		recalc_bounding_box();
	}

	void merge_into(ResourcePatch& other)
	{
		assert(this->type == other.type);
		other.extend(this->positions);
	}

	void extend(const vector<Pos>& newstuff)
	{
		positions.insert(positions.end(), newstuff.begin(), newstuff.end());
		recalc_bounding_box();
	}

	private:
		void recalc_bounding_box()
		{
			bool first = true;
			for (const Pos& p : positions)
			{
				if (p.x < bounding_box.left_top.x || first)
					bounding_box.left_top.x = p.x;
				if (p.x >= bounding_box.right_bottom.x || first)
					bounding_box.right_bottom.x = p.x+1;
				if (p.y < bounding_box.left_top.y || first)
					bounding_box.left_top.y = p.y;
				if (p.y >= bounding_box.right_bottom.y || first)
					bounding_box.right_bottom.y = p.y+1;

				first = false;
			}
		}
};

void FactorioGame::floodfill_resources(const WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius)
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

	int i=0;

	GUI::MapGui gui(&factorio);

	
	// quick read first part of the file before doing any GUI work. Useful for debugging, since reading in 6000 lines will take more than 6 seconds.
	for (int i=0; i<20000; i++) factorio.parse_packet(factorio.read_packet());

	auto t1 = high_resolution_clock::now();

	for (int i=0; i<20; i++)
		cout << a_star( Pos(-584,302), Pos(306,-274), factorio.walk_map, 0.4 ).size() << endl;
	
	auto t2 = high_resolution_clock::now();

	cout << "took " << duration_cast<milliseconds>( t2 - t1 ).count() << "ms" << endl;
	
	return 0;


	while (true)
	{
		factorio.parse_packet( factorio.read_packet() );
		i++;
		//if (i>6000) break; // useful for profiling with gprof / -pg option, since we must cleanly exit then (not by ^C)
		
		GUI::wait(0.001);
	}
}

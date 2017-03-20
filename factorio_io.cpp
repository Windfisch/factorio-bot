#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <set>
#include <deque>

#include "factorio_io.h"
#include "gui/gui.h"

#define READ_BUFSIZE 1024

using std::string;
using std::unordered_map;


using namespace std;

const unordered_map<string, Resource::type_t> Resource::types = { {"coal", COAL}, {"iron-ore", IRON}, {"copper-ore", COPPER}, {"stone", STONE}, {"crude-oil", OIL} };


Area::Area(string str)
{
	std::sscanf(str.c_str(), "%i,%i;%i,%i", &left_top.x, &left_top.y, &right_bottom.x, &right_bottom.y);
	right_bottom.x++; // mod sends them as rightbottom-inclusive
	right_bottom.y++; // but we want an exclusive boundary
}

string Area::str() const
{
	return left_top.str() + " -- " + right_bottom.str();
}

FactorioGame::FactorioGame(string prefix, string rcon_host_, int rcon_port_)
{
	factorio_file_prefix = prefix;
	rcon_host = rcon_host_;
	rcon_port = rcon_port_;
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
	auto p2 = pkg.find(':', p1);

	if (p1 == string::npos || p2 == string::npos)
		throw runtime_error("malformed packet");

	string type = pkg.substr(0, p1);
	Area area(pkg.substr(p1+1, p2-(p1+1)));
	string data = pkg.substr(p2+2); // skip space

	//cout << "type="<<type<<", area="<<area.str()<<endl;

	if (type=="tiles")
		parse_tiles(area, data);
	else if (type=="resources")
		parse_resources(area, data);
	else
		throw runtime_error("unknown packet type '"+type+"'");
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
		view.at(x,y).can_walk = (data[2*i]=='0');
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
		int x = int(stod( entry.substr(p1+1, p2-(p1+1)) ));
		int y = int(stod( entry.substr(p2+1) ));

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

void FactorioGame::floodfill_resources(const WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius)
{
	int id = next_free_resource_id++;
	
	set<int> neighbors;
	deque<Pos> todo;
	vector<Pos> orepatch;
	todo.push_back(Pos(x,y));
	int count = 0;
	auto resource_type = view.at(x,y).type;
	cout << "type = "<<resource_type<< " @"<<Pos(x,y).str()<<endl;

	while (!todo.empty())
	{
		Pos p = todo.front();
		todo.pop_front();
		orepatch.push_back(p);
		count++;
		
		if (area.contains(p)) // this is inside the chunk we should fill
		{
			view.at(p).patch_id = id;
			
			for (int x_offset=-radius; x_offset<=radius; x_offset++)
				for (int y_offset=-radius; y_offset<=radius; y_offset++)
				{
					const int xx = x + x_offset;
					const int yy = y + y_offset;
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
			const Resource& ref = view.at(p);

			if (ref.type == resource_type)
				neighbors.insert(ref.patch_id);
		}
	}

	cout << "count=" << count << ", neighbors=" << neighbors.size() << endl;
	

	shared_ptr<ResourcePatch> resource_patch;

	// TODO: update patchlist
	if (neighbors.size() == 0)
	{
		resource_patch = make_shared<ResourcePatch>(orepatch, resource_type);
	}
	else
	{
		auto largest = std::max_element(neighbors.begin(), neighbors.end(), [](auto a, auto b) {return a->elements.size() > a->elements.size();});
	}

	// clear floodfill_flag
	for (const Pos& p : orepatch)
	{
		const Resource& ref = view.at(p);
		ref.floodfill_flag = Resource::FLOODFILL_NONE;
		ref.patch = resource_patch;
	}
}


int main()
{
	FactorioGame factorio("/home/flo/kruschkram/factorio-AImod/script-output/output", "localhost", 1234);
	int i=0;

	GUI::MapGui gui(&factorio);

	while (true)
	{
		factorio.parse_packet( factorio.read_packet() );
		//usleep(1000);
		i++;
		//if (i%100 == 0) cout << i << endl;
		//if (i>6000) break;
		
		GUI::wait(0.01);
	}
}

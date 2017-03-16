#include <regex>
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <unistd.h>

#include "worldmap.h"

#define READ_BUFSIZE 1024

enum dir4_t
{
	NORTH=0,
	EAST,
	SOUTH,
	WEST
};

struct Area
{
	int x1,y1;
	int x2,y2;

	Area() {}
	Area(int x1, int y1, int x2, int y2) : x1(x1), y1(y1), x2(x2), y2(y2) {}
	Area(std::string str);

	std::string prettyprint();
};

class FactorioGame
{
	private:
		std::string line_buffer;

		std::string factorio_file_prefix;
		std::ifstream factorio_file;
		int factorio_file_id = 1;

		std::string rcon_host;
		int rcon_port;

		void change_file_id(int new_id);
		std::string factorio_file_name();
		std::string remove_line_from_buffer();
	
	public:
		FactorioGame(std::string prefix, std::string rcon_host, int rcon_port);
		std::string read_packet();
		void parse_packet(const std::string& data);

		struct walk_t
		{
			bool can_walk;
			bool can_cross;
			bool corridor[4];
		};

		WorldMap<walk_t> walk_map;
};

using namespace std;

Area::Area(string str)
{
	regex exp(" *(-?[0-9]+),(-?[0-9]+);(-?[0-9]+),(-?[0-9]+) *");
	smatch match;
	if (!regex_search(str, match, exp) || match.size() < 4)
		throw runtime_error("invalid area specification '"+str+"'");

	x1 = stoi(match.str(1));
	y1 = stoi(match.str(2));
	x2 = stoi(match.str(3));
	y2 = stoi(match.str(4));
}

string Area::prettyprint()
{
	return "("+to_string(x1)+","+to_string(y1)+") -- ("+to_string(x2)+","+to_string(y2)+")";
}

static bool starts_with(const string& str, const string& begin)
{
	return str.substr(0,begin.length())==begin;
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
	}
	if (!factorio_file.eof())
		throw std::runtime_error("file reading error");
	
	factorio_file.clear();
	
	if (line_buffer.find('\n') == string::npos)
		return "";
	else
		return remove_line_from_buffer();
}

void FactorioGame::parse_packet(const string& pkg)
{
	regex header("^([^ ]*) ([^ ]*): *(.*)$");
	smatch match;
	if (!regex_search(pkg, match, header) || match.size() < 3)
		throw runtime_error("malformed packet");

	string type = match.str(1);
	Area area(match.str(2));
	string data = match.str(3);

	cout << "type="<<type<<", area="<<area.prettyprint()<<endl;

	if (type=="tiles")
		parse_tiles(area, data);
	else if (type=="resources")
		parse_resources(area, data);
	else
		throw runtime_error("unknown packet type '"+type+"'");
}

void FactorioGame::parse_tiles(area, data)
{
	if (data.length() != 1024+1023)
		throw runtime_error("parse_tiles: invalid length");
	
	auto view = walk_map.viewport(area.left_top, area.right_bottom, area.left_top);

	for (int i=0; i<1024; i++)
	{
		int x = i%32;
		int y = i/32;
		view.at(x,y).can_walk = (data[2*i]=="0");
	}
}

enum resource_type_t
{
	COAL,
	IRON,
	COPPER,
	STONE,
	OIL
};

unordered_map<string, resource_type_t> resource_types = { {"coal", COAL}, {"iron-ore", IRON}, {"copper-ore", COPPER}, {"stone", STONE}, {"oil", OIL} };

void FactorioGame::parse_resources(area, data)
{
	istringstream str(data);
	string entry;
	resource_type_t type;
	int x,y;

	regex reg("^ *([^ ]*) ([^ ]*) ([^ ]*) *$");
	smatch match;

	auto view = resource_map.viewport(area.left_top - Pos(32,32), area.right_bottom + Pos(32,32), Pos(0,0));

	// FIXME: clear and un-assign existing entities before

	while (getline(str, entry, ','))
	{
		if (!regex_search(entry, match, reg) || match.size() < 3)
			throw runtime_error("malformed resource entry");
		
		type = resource_types[match.str(1)];
		x = floor(stod(match.str(2)));
		y = floor(stod(match.str(2)));

		view.at(x,y) = Resource(x,y,type, NOT_YET_ASSIGNED);
	}


}


int main()
{
	FactorioGame factorio("/home/flo/kruschkram/factorio2-mod/factorio/script-output/output", "localhost", 1234);

	while (true)
	{
		cout << "> " << factorio.read_packet() << endl;
		usleep(1000000);
	}
}

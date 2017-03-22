#include <string>
#include <memory>
#include <unordered_map>
#include <fstream>
#include <set>

#include "worldmap.hpp"
#include "pos.hpp"

enum dir4_t
{
	NORTH=0,
	EAST,
	SOUTH,
	WEST
};

struct Area
{
	Pos left_top;
	Pos right_bottom;

	Area() {}
	Area(int x1, int y1, int x2, int y2) : left_top(x1,y1), right_bottom(x2,y2)  {}
	Area(const Pos& lt, const Pos& rb) : left_top(lt), right_bottom(rb) {}
	Area(std::string str);

	std::string str() const;

	bool contains(const Pos& p) const { return left_top <= p && p < right_bottom; }
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

		void parse_tiles(const Area& area, const std::string& data);
		void parse_resources(const Area& area, const std::string& data);

		int next_free_resource_id = 1;
	
	public:
		FactorioGame(std::string prefix, std::string rcon_host, int rcon_port);
		std::string read_packet();
		void parse_packet(const std::string& data);
		void floodfill_resources(const WorldMap<Resource>::Viewport& view, const Area& area, int x, int y, int radius);

		struct walk_t
		{
			bool can_walk;
			bool can_cross;
			bool corridor[4];
		};

		WorldMap<walk_t> walk_map;
		WorldMap<Resource> resource_map;
		std::set< std::shared_ptr<ResourcePatch> > resource_patches;
};

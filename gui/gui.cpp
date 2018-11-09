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

#include <vector>
#include <cassert>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wdocumentation"
#pragma GCC diagnostic ignored "-Wdocumentation-deprecated-sync"
#pragma GCC diagnostic ignored "-Wdocumentation-unknown-command"
#pragma GCC diagnostic ignored "-Wextra-semi"
#pragma GCC diagnostic ignored "-Wreserved-id-macro"
#pragma GCC diagnostic ignored "-Wshift-sign-overflow"
#pragma GCC diagnostic ignored "-Wundef"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_PNG_Image.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Image.H>
#include <FL/Fl_Box.H>
#include <FL/fl_draw.H>
#pragma GCC diagnostic pop

#include "gui.h"
#include "../factorio_io.h"
#include "../pathfinding.hpp"
#include "../mine_planning.h" // DEBUG
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <memory>
#include <unordered_map>
#include <array>

using std::cout;
using std::endl;
using std::vector;

using std::floor;
using std::sin;

using std::max;

using std::string;
using std::unordered_map;
using std::shared_ptr;
using std::array;

const int img_minscale = -6;
const int img_maxscale = -3;

static const string ANSI_CLEAR_LINE = "\033[2K";
static const string ANSI_CURSOR_UP = "\033[A";

static string ANSI_ERASE(int n)
{
	string result="";
	for (int i=0; i<n; i++)
		result+=ANSI_CURSOR_UP+ANSI_CLEAR_LINE+"\r";
	return result;
}

template<typename T> static constexpr T clamp(T v, T a, T b)
{
	return (v<a)?a:  ( (v>b?b: v) );
}

namespace GUI
{

void Color::blend(const Color& other, float alpha)
{
	this->r = uint8_t(clamp(int(alpha*this->r + (1.f-alpha)*other.r), 0, 255));
	this->g = uint8_t(clamp(int(alpha*this->g + (1.f-alpha)*other.g), 0, 255));
	this->b = uint8_t(clamp(int(alpha*this->b + (1.f-alpha)*other.b), 0, 255));
}

Color color_hsv(double hue, double sat, double val)
{
	int i = int(floor(hue));
	double f = hue - i;
	int p = int(255 * val * (1 - sat));
	int q = int(255 * val * (1 - f * sat));
	int t = int(255 * val * (1 - (1 - f) * sat));
	int v = int(255 * val);

	switch ((i%6+6)%6)
	{
		case 0: return Color(v,t,p);
		case 1: return Color(q,v,p);
		case 2: return Color(p,v,t);
		case 3: return Color(p,q,v);
		case 4: return Color(t,p,v);
		case 5: return Color(v,p,q);
		default: return Color(255,0,255); // cannot happen
	}
}

Color get_color(int id)
{
	if (id==0) return Color(0,0,0);
	return color_hsv(0.53481*id, 0.8 + 0.2*sin(id/21.324), 0.7+0.3*sin(id/2.13184));
}

static std::unordered_map<Resource::type_t, Color> resource_colors {
	{ Resource::COAL, Color(16,16,16) },
	{ Resource::IRON, Color(130,205,255) },
	{ Resource::COPPER, Color(230,115,0) },
	{ Resource::STONE, Color(160,160,30) },
	{ Resource::OIL, Color(255,0,255) },
	{ Resource::URANIUM, Color(0,255,0) }
};

class MapBox : public Fl_Box
{
	protected:
		virtual void draw(void);
		virtual int handle(int ev);
	private:
		std::vector<unsigned char> imgbuf;
		int imgwidth, imgheight;
		std::unique_ptr<Fl_RGB_Image> rgbimg;
		_MapGui_impl* gui;

		Pos canvas_center; // divide this by 2**zoom_level to get tile coords
		Pos canvas_drag;
		int zoom_level = 0; // powers of two
		
		bool display_patch_type = true; // true -> color represents type, false -> color represents pointer

		WorldList<PlannedEntity> display_debug_entities;
		int display_debug_entity_level = 1;

		Pos zoom_transform(const Pos& p, int factor);
		Pos zoom_transform(const Pos_f& p, int factor);
		void give_info(const Pos& pos);
		void give_detailed_info(const Pos& pos);

		void draw_box(const Pos& center, int radius, const Color& col);

		Pos last_info_pos;

		Pos last_click[3];

		vector<Pos> last_path;

	public:
		MapBox(_MapGui_impl* gui, int X, int Y, int W, int H, const char *L=nullptr);
		void update_imgbuf();
};

Pos MapBox::zoom_transform(const Pos& p, int factor)
{
	if (factor > 0)
		return p*(1<<factor);
	else if (factor < 0)
		return p/(1<<(-factor));
	else
		return p;
}
Pos MapBox::zoom_transform(const Pos_f& p, int factor)
{
	return (p * pow(2, factor)).to_int();
}

struct line_t
{
	Pos a,b;
	Color c;
};

struct rect_t
{
	Pos a,b;
	Color c;
};

struct GameGraphic
{
	vector<shared_ptr<Fl_Image>> img_pyramid;
	Pos_f shift;
};

struct fltk_timeout_guard
{
	double _time;
	Fl_Timeout_Handler _func;
	void* _data;
	
	void add(double time, Fl_Timeout_Handler func, void* data)
	{
		_time = time;
		_func = func;
		_data = data;
		Fl::add_timeout(time, func, data);
	}

	void repeat()
	{
		Fl::repeat_timeout(_time, _func, _data);
	}

	~fltk_timeout_guard()
	{
		Fl::remove_timeout(_func, _data);
	}
};

class _MapGui_impl
{
	public:
		_MapGui_impl(FactorioGame* game, string datapath);

		void line(Pos a, Pos b, Color c)
		{
			lines.push_back(line_t{a,b,c});
		}
		void rect(Pos a, Pos b, Color c)
		{
			rects.push_back(rect_t{a,b,c});
		}
		void rect(Pos a, int size, Color c)
		{
			rect(a-Pos(size,size), a+Pos(size,size), c);
		}
		void clear()
		{
			lines.clear();
			rects.clear();
		}
		int key()
		{
			auto tmp = _key;
			_key = 0;
			return tmp;
		}

		std::unique_ptr<Fl_Double_Window> window;
		std::unique_ptr<MapBox> mapbox;

		fltk_timeout_guard tick_timer;

		FactorioGame* game;
		string datapath;

		string gfx_filename(string orig);

		std::vector<line_t> lines;
		std::vector<rect_t> rects;

		void load_graphics();
		unordered_map<string, array<GameGraphic,4> > game_graphics;
		
		void tick(); // called 5 times per second (roughly)
		int _key;
};

void MapBox::give_info(const Pos& pos)
{
	if (last_info_pos == pos)
		return;
	last_info_pos = pos;

	const pathfinding::walk_t& info = gui->game->walk_map.at(pos);
	const Resource& res = gui->game->resource_map.at(pos);
	auto patch = res.resource_patch.lock();

	cout << ANSI_ERASE(2)
	     << strpad(pos.str()+":",8) << "\tcan_walk = " << info.can_walk << "; north/east/south/west margins = " <<
		info.margins[NORTH] << "," <<
		info.margins[EAST] << "," <<
		info.margins[SOUTH] << "," <<
		info.margins[WEST] << ";" << endl
	     << "\t\t" << "restype = " << Resource::typestr[res.type] << "; patch = " << patch.get();
	     if (patch) cout << ", size = " << patch->size();
	     cout << endl;
}

void MapBox::give_detailed_info(const Pos& pos)
{
	auto around = gui->game->actual_entities.around(pos);
	auto first = around.begin();
	if (first == around.end())
	{
		cout << "no entities around the cursor" << endl;
		return;
	}
	const Entity& entity = *first;
	cout << "Entity of type '" << entity.proto->name << "' at " << entity.pos.str() << endl;
	if (const auto* data = entity.data_or_null<ContainerData>())
	{
		cout << "  fuel_is_output = " << data->fuel_is_output << endl;
		data->inventories.dump();
	}
}

int MapBox::handle(int event)
{
	const Pos mouse = Pos(Fl::event_x(), Fl::event_y()) - Pos(imgwidth/2, imgheight/2) - Pos(x(),y());

	switch (event)
	{
		case FL_PUSH: {
			canvas_drag = canvas_center - mouse;

			int button = Fl::event_button() - 1;
			if (0 <= button && button < 3)
			{
				last_click[button] = zoom_transform(mouse-canvas_center, zoom_level);

				if (button == 2)
				{
					cout << "starting pathfinding from " << last_click[0].str() << " to " << last_click[2].str() << endl;
					last_path = a_star( last_click[0], last_click[2], gui->game->walk_map);
					// FIXME hardcoded action id
					//gui->game->set_waypoints(0, 1, last_path);
					gui->game->walk_to(1,last_click[2]);
					cout << last_path.size() << endl;
				}
			}
			return 1;
		}
		case FL_DRAG:
			canvas_center = canvas_drag + mouse;
			update_imgbuf();
			redraw();
			return 0;
		case FL_MOVE:
			give_info(zoom_transform(mouse-canvas_center, zoom_level));
			return 0;
		case FL_MOUSEWHEEL: {
			int zoom_level_prev = zoom_level;
			zoom_level += Fl::event_dy();
			zoom_level = clamp(zoom_level, -6, 1);

			canvas_center = zoom_transform(canvas_center-mouse, zoom_level_prev-zoom_level)+mouse;
			canvas_drag = zoom_transform(canvas_drag-mouse, zoom_level_prev-zoom_level)+mouse;
			
			update_imgbuf();
			redraw();
			return 1;
		}
		case FL_FOCUS: return 1;
		case FL_UNFOCUS: return 1;
		case FL_KEYDOWN: {
			std::cout << "KEYBOARD EVENT " << Fl::event_key() << std::endl;
			switch (Fl::event_key())
			{
				case 'p': // display patch type
					display_patch_type = !display_patch_type;
					redraw();
					break;
				case 'i': // detailed info
					give_detailed_info(zoom_transform(mouse-canvas_center, zoom_level));
					break;
				case 'm': // debug mine planning
				{
					auto pos = zoom_transform(mouse-canvas_center, zoom_level);
					const Resource& res = gui->game->resource_map.at(pos);
					auto patch = res.resource_patch.lock();
					
					if (patch)
					{
						cout << "planning mine" << endl;

						vector<PlannedEntity> facility;
						switch (patch->type)
						{
							case Resource::COAL:
								facility = plan_early_coal_rig(*patch, gui->game);
								break;
							case Resource::STONE:
								facility = plan_early_chest_rig(*patch, gui->game);
								break;
							default:
								facility = plan_early_smelter_rig(*patch, gui->game);
								break;
						}

						display_debug_entities.clear();
						display_debug_entities.insert_all(facility);
						//display_debug_entities.insert_all( plan_mine(
						//	patch->positions, pos, *gui->game) );
					}
					else
					{
						cout << "no patch under cursor" << endl;
					}
					break;
				}
				case ',':
					display_debug_entity_level = max(display_debug_entity_level-1,0);
					break;
				case '.':
					display_debug_entity_level++;
					break;
				default:
					gui->_key = Fl::event_key();
			}
			return 1;
		}
	}

	return Fl_Box::handle(event);
}

void MapBox::draw(void)
{
	rgbimg->draw(x(),y(),w(),h());


	if (img_minscale <= zoom_level && zoom_level <= img_maxscale)
	{
		// draw entities
		
		Pos top_left_chunk  = Pos::tile_to_chunk( zoom_transform( Pos(-imgwidth/2, -imgheight/2) - canvas_center, zoom_level ) );
		Pos bot_right_chunk = Pos::tile_to_chunk( zoom_transform( Pos(imgwidth/2, imgheight/2) - canvas_center, zoom_level ) );
		for (int y = top_left_chunk.y; y <= bot_right_chunk.y; y++)
			for (int x = top_left_chunk.x; x <= bot_right_chunk.x; x++)
			{
				//cout << "chunk " << x << "/" << y << endl;
				const auto& ent_iter = gui->game->actual_entities.find(Pos(x,y));
				const auto& dbg_iter = display_debug_entities.find(Pos(x,y));
				
				vector<Entity> entities;
				
				if (ent_iter != gui->game->actual_entities.end())
					entities.insert(entities.end(), ent_iter->second.begin(), ent_iter->second.end());

				if (dbg_iter != display_debug_entities.end())
					for (const auto& ent : dbg_iter->second)
						if (ent.level <= display_debug_entity_level)
							entities.push_back(ent);
				
				if (entities.empty())
					continue;

				sort(entities.begin(), entities.end(), [](const Entity& a, const Entity& b) { return a.pos.less_rowwise(b.pos); });

				for (const auto& entity : entities)
				{
					// FIXME this should be a member of EntityPrototype to
					// avoid the hashmap lookup

					auto iter = gui->game_graphics.find(entity.proto->name);
					if (iter != gui->game_graphics.end())
					{
						// we have a graphic for this entity
						// FIXME use the proper direction, once we get it from the game
						const GameGraphic& gfx = iter->second[entity.direction];

						Pos p = zoom_transform(entity.pos + gfx.shift, -zoom_level) + canvas_center + Pos(imgwidth/2, imgheight/2);
						
						const auto& img = gfx.img_pyramid[zoom_level-img_minscale];
						img->draw( this->x()+p.x - img->w()/2, this->y()+p.y - img->h()/2);
					}
				}
			}
	}


	for (const auto& rect : gui->rects)
	{
		Pos a = zoom_transform( rect.a, -zoom_level ) + canvas_center + Pos(imgwidth/2, imgheight/2);
		Pos b = zoom_transform( rect.b, -zoom_level ) + canvas_center + Pos(imgwidth/2, imgheight/2);

		fl_draw_box(FL_BORDER_FRAME, x()+a.x, y()+a.y, b.x-a.x, b.y-a.y, fl_rgb_color(rect.c.r, rect.c.g, rect.c.b));
	}
	for (const auto& line : gui->lines)
	{
		Pos a = zoom_transform( line.a, -zoom_level ) + canvas_center + Pos(imgwidth/2, imgheight/2);
		Pos b = zoom_transform( line.b, -zoom_level ) + canvas_center + Pos(imgwidth/2, imgheight/2);

		fl_color(fl_rgb_color(line.c.r, line.c.g, line.c.b));
		fl_line( x()+a.x, y()+a.y, x()+b.x, y()+b.y );
	}
}

MapBox::MapBox(_MapGui_impl* mb, int x, int y, int w, int h, const char* l) : Fl_Box(x,y,w,h,l), gui(mb)
{
	imgwidth = 1000;
	imgheight = 1000;
	imgbuf.resize(4*imgwidth*imgheight);
	for (int i=0; i<imgwidth*imgheight; i++)
	{
		uint32_t foo = i*16;
		imgbuf[4*i+0] = foo & 0xff;
		imgbuf[4*i+1] = (foo>>8) & 0xff;
		imgbuf[4*i+2] = (foo>>16) & 0xff;
		imgbuf[4*i+3] = 255;
	}

	rgbimg.reset(new Fl_RGB_Image(imgbuf.data(), imgwidth, imgheight, 4));
}

void MapBox::update_imgbuf()
{
	Pos lt = zoom_transform(Pos(-imgwidth/2, -imgheight/2)-canvas_center, zoom_level);
	Pos rb = zoom_transform(Pos(imgwidth-imgwidth/2, imgheight-imgheight/2)-canvas_center, zoom_level) + Pos(1,1);
	auto resview = gui->game->resource_map.view(lt, rb, Pos(0,0));
	auto view = gui->game->walk_map.view(lt, rb, Pos(0,0));
	// the chunks store the data as array [x][y], i.e., y should be the fast-running coordinate to be cache efficient
	for (int x=0; x<imgwidth; x++)
		for (int y=0; y<imgheight; y++)
		{
			int idx = (y*imgwidth+x)*4;
			Pos mappos = zoom_transform(Pos(x-imgwidth/2,y-imgheight/2)-canvas_center, zoom_level);
			//Color col = get_color(view.at(mappos).patch_id);
			//Color col = get_color(long(view.at(mappos).resource_patch.lock().get()));
			
			Color col;
			const auto& w = view.at(mappos);
			if (!w.known) col = Color(0,0,0);
			else if (!w.can_walk) col = Color(0,0,255);
			else if (!w.can_cross) col = Color(255,255,0);
			else col = Color(0,127,0);
			
			int rx = ((mappos.x)%32 + 32)%32;
			int ry = ((mappos.y)%32 + 32)%32;
			if ( (rx==0 || rx==31) && (ry==0 || ry==31))
			{
				if (mappos.x < 0) col.r = 0; else col.r = 255;
				if (mappos.y < 0) col.g = 0; else col.g = 255;
				col.b = 127;
			}

			Pos tmp = Pos(x-imgwidth/2,y-imgwidth/2)-canvas_center;
			int tile_width_px = 1<< (-zoom_level); // drawing width of one tile
			Pos tile_inner_pos = Pos(sane_mod(tmp.x, tile_width_px), sane_mod(tmp.y, tile_width_px));
		
			if (resview.at(mappos).patch_id)
			{
				// there's a resource patch at that location?
				if (display_patch_type)
				{
					if (resview.at(mappos).type != Resource::OCEAN)
						col.blend(resource_colors[resview.at(mappos).type], .15f);
				}
				else
					col.blend(get_color(resview.at(mappos).patch_id), .25f);
			}
			
			// draw the margins caused by objects on the tile
			if (w.known)
			{
				if (zoom_level < -1)
				{
					// draw margins
					double inner_x = tile_inner_pos.x / double(tile_width_px);
					double inner_y = tile_inner_pos.y / double(tile_width_px);

					if ( (w.margins[NORTH] <= inner_y && inner_y < 1.-w.margins[SOUTH]) &&
					     (w.margins[WEST] <= inner_x && inner_x < 1.-w.margins[EAST]) )
						col.blend(Color(255, 127, 0), 0.4f);
				}
				else
				{
					if (w.margins[NORTH] < 1 || w.margins[SOUTH] < 1 || w.margins[EAST] < 1 || w.margins[WEST] < 1)
						col.blend(Color(255, 127, 0), 0.4f);
				}
			}

			// draw lines around the tiles
			if (zoom_level < -2)
			{
				if (tile_inner_pos.x == 0 || tile_inner_pos.y == 0)
					col.blend(Color(127,127,127), zoom_level==-3 ? 0.7f : 0.5f);
			}

			imgbuf[idx+0] = col.r;
			imgbuf[idx+1] = col.g;
			imgbuf[idx+2] = col.b;
			imgbuf[idx+3] = 255;
		}

	for (size_t i=0; i<last_path.size(); i++)
	{
		float x = i / float(last_path.size());
		Color col = Color( int(255*x), int(255*(1-x)), 196 );

		Pos screenpos = zoom_transform( last_path[i], -zoom_level ) + canvas_center + Pos(imgwidth/2, imgheight/2);
		draw_box(screenpos, 1<<max(-zoom_level-1,0), col);
	}

	for (const auto& player : gui->game->players) if (player.connected)
	{
		Pos screenpos = zoom_transform( player.position.to_int(), -zoom_level ) + canvas_center + Pos(imgwidth/2, imgheight/2);

		draw_box(screenpos, 2<<max(-zoom_level,0), Color(255,0,0));
	}

	rgbimg.reset(new Fl_RGB_Image(imgbuf.data(), imgwidth, imgheight, 4));
}

void MapBox::draw_box(const Pos& center, int radius, const Color& col)
{
		for (int x=-radius; x<=radius; x++) for (int y=-radius; y<radius; y++)
		{
			Pos pos = center + Pos(x,y);
			if (Pos(0,0) <= pos && pos < Pos(imgwidth, imgheight))
			{
				int idx = (pos.y*imgwidth+pos.x)*4;

				imgbuf[idx+0] = col.r;
				imgbuf[idx+1] = col.g;
				imgbuf[idx+2] = col.b;
				imgbuf[idx+3] = 255;
			}
		}
}

MapGui::MapGui(FactorioGame* game, const char* datapath) : impl(new _MapGui_impl(game, datapath)) {}
MapGui::~MapGui() {}

void _MapGui_impl::tick()
{
	mapbox->update_imgbuf();
	mapbox->redraw();
}

_MapGui_impl::_MapGui_impl(FactorioGame* game_, string datapath_)
{
	this->game = game_;
	this->datapath = datapath_;

	load_graphics();
	
	window = std::make_unique<Fl_Double_Window>(340,180);
	mapbox = std::make_unique<MapBox>(this,20,40,300,100,"Hello, World!");
	mapbox->box(FL_UP_BOX);
	mapbox->labelfont(FL_BOLD+FL_ITALIC);
	mapbox->labelsize(36);
	mapbox->labeltype(FL_SHADOW_LABEL);
	window->resizable(mapbox.get());
	window->end();
	window->show();
	tick_timer.add(0.2, [](void* ptr){static_cast<_MapGui_impl*>(ptr)->tick();}, this);
}

#define assert_throw(exp) \
	( (exp) ? (void)0 : throw std::runtime_error(#exp))
static shared_ptr<Fl_Image> crop_and_flip_image(const Fl_RGB_Image& img, int x, int y, int w, int h, bool flip_x=false, bool flip_y=false)
{
	int ld = img.ld();
	string debugstr = "W=" + std::to_string(img.w()) + ", H=" + std::to_string(img.h()) + ", x=" + std::to_string(x) + ", y=" + std::to_string(y) + ", w=" + std::to_string(w) + ", h=" + std::to_string(h) + ", d=" + std::to_string(img.d()) + ",ld=" + std::to_string(img.ld()) + ","+std::to_string(ld);
	if (ld == 0) ld = img.w() * img.d();
	
	assert_throw(img.d() >= 1);
	assert_throw(x >= 0 && x < img.w());
	assert_throw(y >= 0 && y < img.h());
	assert_throw(x+w <= img.w());
	assert_throw(y+h <= img.h());

	cout << debugstr << endl;


	// copy the region of interest into a new array, flip as appropriate
	int newld = img.d()*w;
	uchar* array = new uchar[h*newld]; // `array` will be transferred into `result`'s ownership, `result` is responsible for freeing `array`. FL_RGB_Image will use "delete[] (uchar*) array".
	int yo_delta = flip_y ? -1 : 1;
	int xo_delta = flip_x ? -1 : 1;
	for (int yd=0, yo=(flip_y ? h-1 : 0); yd<h; yd++, yo+=yo_delta)
		for (int xd=0, xo=(flip_x ? w-1 : 0); xd<w; xd++, xo+=xo_delta)
			for (int i = 0; i < img.d(); i++)
				array[xd*img.d() + yd*newld + i] = img.array[ (x + xo)*img.d() + (y + yo)*ld + i];

	// create new image using said array
	Fl_RGB_Image* result = new Fl_RGB_Image(array, w, h, img.d(), newld); // `result` will be wrapped in a shared_ptr, which is responsible for deleting result
	result->alloc_array = 1; // we want that Fl_RGB_Image frees `array` upon destruction

	return shared_ptr<Fl_Image>(result);
}

static vector<shared_ptr<Fl_Image>> load_image_pyramid(const string& filename, int x, int y, int w, int h, float scale, bool flip_x = false, bool flip_y = false)
{
	Fl_PNG_Image img(filename.c_str());

	if (img.fail())
	{
		if (img.fail() == Fl_Bitmap::ERR_FORMAT)
			throw std::runtime_error("could not load image " + filename + ": " + strerror(errno));
		else
			throw std::runtime_error("could not load image " + filename + ": invalid format");
	}

	cout << "loaded image " << filename << endl;

	shared_ptr<Fl_Image> cropped = crop_and_flip_image(img, x,y,w,h, flip_x, flip_y);
	vector<shared_ptr<Fl_Image>> result;

	for (int i = img_minscale; i <= img_maxscale; i++)
	{
		float factor = powf(2.f,-i) / 32.f * scale;
		result.emplace_back( cropped->copy( int(w*factor), int(h*factor) ) );
	}

	return result;
}

string _MapGui_impl::gfx_filename(string orig)
{
	string prefix;
	if (orig.substr(0,8) == "__base__")
		prefix = "base";
	else if (orig.substr(0,8) == "__core__" )
		prefix = "core";
	else
		assert(false);

	return datapath + "/" + prefix + orig.substr(8);
}

void _MapGui_impl::load_graphics()
{
	for (const auto& iter : game->graphics_definitions)
	{
		const string& name = iter.first;
		const vector<GraphicsDefinition>& defs = iter.second;
		array<GameGraphic,4>& game_graphic = game_graphics[name];

		assert(defs.size() == 1 || defs.size() == 4);

		try
		{
			if (defs.size() == 4)
			{
				for (int i=0; i<4; i++)
				{
					game_graphic[i].img_pyramid = load_image_pyramid( gfx_filename(defs[i].filename), defs[i].x, defs[i].y, defs[i].width, defs[i].height, defs[i].scale, defs[i].flip_x, defs[i].flip_y );
					game_graphic[i].shift.x = defs[i].shiftx;
					game_graphic[i].shift.y = defs[i].shifty;
				}
			}
			else
			{
				auto pyr = load_image_pyramid( gfx_filename(defs[0].filename), defs[0].x, defs[0].y, defs[0].width, defs[0].height, defs[0].scale, defs[0].flip_x, defs[0].flip_y );
				for (int i=0; i<4; i++)
				{
					game_graphic[i].img_pyramid = pyr;
					game_graphic[i].shift.x = defs[0].shiftx;
					game_graphic[i].shift.y = defs[0].shifty;
				}
			}
		}
		catch(std::runtime_error err)
		{
			cout << "failed to load image: " << err.what() << endl;
			game_graphics.erase( game_graphics.find(name) );
		}
	}
}

double wait(double t)
{
	return Fl::wait(t);
}

void MapGui::line(Pos a, Pos b, Color c) { impl->line(a,b,c); }
void MapGui::rect(Pos a, Pos b, Color c) { impl->rect(a,b,c); }
void MapGui::rect(Pos a, int s, Color c) { impl->rect(a,s,c); }
void MapGui::clear() { impl->clear(); }
int MapGui::key() { return impl->key(); }


} // namespace GUI

#pragma GCC diagnostic pop

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

#include <vector>
#include <cassert>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"

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
#include <FL/Fl_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Box.H>
#pragma GCC diagnostic pop

#include "gui.h"
#include "../factorio_io.h"
#include "../pathfinding.hpp"
#include <cmath>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

using std::cout;
using std::endl;
using std::vector;

using std::floor;
using std::sin;

using std::max;

using std::string;

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

struct Color {
	uint8_t r,g,b;
	Color() : r(0),g(0),b(0){}
	Color(int rr,int gg,int bb):r(uint8_t(rr)),g(uint8_t(gg)),b(uint8_t(bb)){ assert(rr<256 && gg<256 && bb<256); }
	void blend(const Color& other, float alpha)
	{
		this->r = uint8_t(clamp(int(alpha*this->r + (1.f-alpha)*other.r), 0, 255));
		this->g = uint8_t(clamp(int(alpha*this->g + (1.f-alpha)*other.g), 0, 255));
		this->b = uint8_t(clamp(int(alpha*this->b + (1.f-alpha)*other.b), 0, 255));
	}
};

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

std::unordered_map<Resource::type_t, Color> resource_colors {
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
		FactorioGame* game;

		Pos canvas_center; // divide this by zoom_level to get tile coords
		Pos canvas_drag;
		int zoom_level = 0; // powers of two
		
		bool display_patch_type = true; // true -> color represents type, false -> color represents pointer

		Pos zoom_transform(const Pos& p, int factor);
		void give_info(const Pos& pos);

		void draw_box(const Pos& center, int radius, const Color& col);

		Pos last_info_pos;

		Pos last_click[3];

		vector<Pos> last_path;

	public:
		MapBox(FactorioGame* game, int X, int Y, int W, int H, const char *L=0);
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

class _MapGui_impl
{
	public:
		_MapGui_impl(FactorioGame* game);

	private:
		std::unique_ptr<Fl_Window> window;
		std::unique_ptr<MapBox> mapbox;

		FactorioGame* game;
};

void MapBox::give_info(const Pos& pos)
{
	if (last_info_pos == pos)
		return;
	last_info_pos = pos;

	const pathfinding::walk_t& info = game->walk_map.at(pos);
	const Resource& res = game->resource_map.at(pos);

	cout << ANSI_ERASE(2)
	     << strpad(pos.str()+":",8) << "\tcan_walk = " << info.can_walk << "; north/east/south/west margins = " <<
		info.margins[NORTH] << "," <<
		info.margins[EAST] << "," <<
		info.margins[SOUTH] << "," <<
		info.margins[WEST] << ";" << endl
	     << "\t\t" << "restype = " << Resource::typestr[res.type] << "; patch = " << res.resource_patch.lock().get() << endl;
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
					last_path = a_star( last_click[0], last_click[2], game->walk_map, 0.4);
					// FIXME hardcoded action id
					//game->set_waypoints(0, 1, last_path);
					game->walk_to(1,last_click[2]);
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
			zoom_level = clamp(zoom_level, -5, 1);

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
				case 'p':
					display_patch_type = !display_patch_type;
					redraw();
					break;
			}
			return 1;
		}
	}

	return Fl_Box::handle(event);
}

void MapBox::draw(void)
{
	rgbimg->draw(x(),y(),w(),h());
}

MapBox::MapBox(FactorioGame* g, int x, int y, int w, int h, const char* l) : Fl_Box(x,y,w,h,l), game(g)
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

static MapBox* foo = 0;

void MapBox::update_imgbuf()
{
	Pos lt = zoom_transform(Pos(-imgwidth/2, -imgheight/2)-canvas_center, zoom_level);
	Pos rb = zoom_transform(Pos(imgwidth-imgwidth/2, imgheight-imgheight/2)-canvas_center, zoom_level) + Pos(1,1);
	auto resview = game->resource_map.view(lt, rb, Pos(0,0));
	auto view = game->walk_map.view(lt, rb, Pos(0,0));
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
					col.blend(resource_colors[resview.at(mappos).type], .15f);
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

	for (const auto& player : game->players) if (player.connected)
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

MapGui::MapGui(FactorioGame* game) : impl(new _MapGui_impl(game)) {}
MapGui::~MapGui() {}

_MapGui_impl::_MapGui_impl(FactorioGame* game_)
{
	this->game = game_;
	
	window.reset(new Fl_Window(340,180));
	mapbox.reset(new MapBox(game,20,40,300,100,"Hello, World!"));
	mapbox->box(FL_UP_BOX);
	mapbox->labelfont(FL_BOLD+FL_ITALIC);
	mapbox->labelsize(36);
	mapbox->labeltype(FL_SHADOW_LABEL);
	foo = mapbox.get(); // FIXME
	window->resizable(mapbox.get());
	window->end();
	window->show();
}

double wait(double t)
{
	static int cnt = 0;
	if (cnt++ % 100 == 0) {foo->update_imgbuf(); foo->redraw(); }
	return Fl::wait(t);
}

} // namespace GUI

#pragma GCC diagnostic pop

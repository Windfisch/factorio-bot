#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#include "gui.h"
#include "../factorio_io.h"
#include <cmath>

using std::floor;
using std::sin;

static constexpr int clamp(int v, int a, int b)
{
	return (v<a)?a:  ( (v>b?b: v) );
}

namespace GUI
{

class MapBox : public Fl_Box
{
	protected:
		virtual void draw(void);
		virtual int handle(int ev);
	private:
		std::vector<unsigned char> imgbuf;
		int imgwidth, imgheight;
		std::unique_ptr<Fl_RGB_Image> rgbimg;
		const FactorioGame* game;

		Pos canvas_center; // divide this by zoom_level to get tile coords
		Pos canvas_drag;
		int zoom_level = 0; // powers of two

		Pos zoom_transform(const Pos& p, int factor);

	public:
		MapBox(const FactorioGame* game, int X, int Y, int W, int H, const char *L=0);
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
		_MapGui_impl(const FactorioGame* game);

	private:
		std::unique_ptr<Fl_Window> window;
		std::unique_ptr<MapBox> mapbox;

		const FactorioGame* game;
};

int MapBox::handle(int event)
{
	const Pos mouse = Pos(Fl::event_x(), Fl::event_y()) - Pos(imgwidth/2, imgheight/2) - Pos(x(),y());

	switch (event)
	{
		case FL_PUSH:
			canvas_drag = canvas_center - mouse;
			return 1;
		case FL_DRAG:
			canvas_center = canvas_drag + mouse;
			update_imgbuf();
			redraw();
			return 0;
		case FL_MOUSEWHEEL:
			int zoom_level_prev = zoom_level;
			zoom_level += Fl::event_dy();
			zoom_level = clamp(zoom_level, -5, 1);

			canvas_center = zoom_transform(canvas_center-mouse, zoom_level_prev-zoom_level)+mouse;
			canvas_drag = zoom_transform(canvas_drag-mouse, zoom_level_prev-zoom_level)+mouse;
			
			update_imgbuf();
			redraw();
			return 1;
	}

	return Fl_Box::handle(event);
}

void MapBox::draw(void)
{
	rgbimg->draw(x(),y(),w(),h());
}

MapBox::MapBox(const FactorioGame* g, int x, int y, int w, int h, const char* l) : Fl_Box(x,y,w,h,l), game(g)
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

struct Color {
	int r,g,b;
	Color() : r(0),g(0),b(0){}
	Color(int rr,int gg,int bb):r(rr),g(gg),b(bb){}
	void blend(const Color& other, float alpha)
	{
		this->r = clamp(int(alpha*this->r + (1-alpha)*other.r), 0, 255);
		this->g = clamp(int(alpha*this->g + (1-alpha)*other.g), 0, 255);
		this->b = clamp(int(alpha*this->b + (1-alpha)*other.b), 0, 255);
	}
};

Color color_hsv(double hue, double sat, double val)
{
	int i = int(floor(hue));
	double f = hue * 6 - i;
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

static MapBox* foo = 0;

void MapBox::update_imgbuf()
{
	Pos lt = zoom_transform(Pos(-imgwidth/2, -imgheight/2)-canvas_center, zoom_level);
	Pos rb = zoom_transform(Pos(imgwidth-imgwidth/2, imgheight-imgheight/2)-canvas_center, zoom_level) + Pos(1,1);
	//auto view = game->resource_map.view(lt, rb, Pos(0,0));
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

			if (zoom_level < -2)
			{
				Pos tmp = Pos(x-imgwidth/2,y-imgwidth/2)-canvas_center;
				int z = 1<< (-zoom_level);
				if (sane_mod(tmp.x,z)==0 || sane_mod(tmp.y,z)==0)
				{
					//col.r=255-col.r;
					//col.g=255-col.g;
					//col.b=255-col.b;
					col.blend(Color(127,127,127), zoom_level==-3 ? 0.7 : 0.5);
				}
			}

			imgbuf[idx+0] = col.r;
			imgbuf[idx+1] = col.g;
			imgbuf[idx+2] = col.b;
			imgbuf[idx+3] = 255;
		}

	rgbimg.reset(new Fl_RGB_Image(imgbuf.data(), imgwidth, imgheight, 4));
}

MapGui::MapGui(const FactorioGame* game) : impl(new _MapGui_impl(game)) {}
MapGui::~MapGui() {}

_MapGui_impl::_MapGui_impl(const FactorioGame* game_)
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

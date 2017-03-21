#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#include "gui.h"
#include "../factorio_io.h"
#include <cmath>

using std::floor;
using std::sin;

namespace GUI
{

class MapBox : public Fl_Box
{
       protected:
               virtual void draw(void);
       private:
               std::vector<unsigned char> imgbuf;
	       int imgwidth, imgheight;
	       std::unique_ptr<Fl_RGB_Image> rgbimg;
	       const FactorioGame* game;

       public:
               MapBox(const FactorioGame* game, int X, int Y, int W, int H, const char *L=0);
	       void update_imgbuf();
};


class _MapGui_impl
{
	public:
		_MapGui_impl(const FactorioGame* game);

	private:
		std::unique_ptr<Fl_Window> window;
		std::unique_ptr<MapBox> mapbox;

		const FactorioGame* game;
};

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
	Color(int rr,int gg,int bb):r(rr),g(gg),b(bb){}
};

Color color_hsv(double hue, double sat, double val)
{
	double r,g,b;
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
	Pos origin=Pos(-500,-500);
	auto view = game->resource_map.view(Pos(-500,-500),Pos(500,500),origin);
	for (int x=0; x<imgwidth; x++)
		for (int y=0; y<imgheight; y++)
		{
			int idx = (y*imgwidth+x)*4;
			//Color col = get_color(long(view.at(x,y).type));
			Color col = get_color(long(view.at(x,y).resource_patch.lock().get()));
			//Color col = get_color(view.at(x,y).patch_id);
			
			int rx = ((x+origin.x)%32 + 32)%32;
			int ry = ((y+origin.y)%32 + 32)%32;
			if ( (rx==0 || rx==31) && (ry==0 || ry==31))
			{
				if (x + origin.x < 0) col.r = 0; else col.r = 255;
				if (y + origin.y < 0) col.g = 0; else col.g = 255;
				col.b = 127;
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

_MapGui_impl::_MapGui_impl(const FactorioGame* game)
{
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

	this->game = game;
}

double GUI::wait(double t)
{
	static int cnt = 0;
	if (cnt++ % 100 == 0) foo->update_imgbuf();
	return Fl::wait(t);
}

} // namespace GUI

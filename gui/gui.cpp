#include <vector>
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>

#include "gui.h"

namespace GUI
{

class _MapGui_impl
{
	public:
		_MapGui_impl(const FactorioGame* game);

	private:
		std::unique_ptr<Fl_Window> window;
		std::unique_ptr<Fl_Box> mapbox;
};

class MapBox : public Fl_Box
{
       protected:
               virtual void draw(void);
       private:
               std::vector<unsigned char> imgbuf;
	       std::unique_ptr<Fl_RGB_Image> rgbimg;

       public:
               MapBox(int X, int Y, int W, int H, const char *L=0);
};

void MapBox::draw(void)
{
	rgbimg->draw(x(),y());
}

MapBox::MapBox(int x, int y, int w, int h, const char* l) : Fl_Box(x,y,w,h,l)
{
	imgbuf.resize(4*1000*1000);
	for (int i=0; i<1000*1000; i++)
	{
		uint32_t foo = i*16;
		imgbuf[4*i+0] = foo & 0xff;
		imgbuf[4*i+1] = (foo>>8) & 0xff;
		imgbuf[4*i+2] = (foo>>16) & 0xff;
		imgbuf[4*i+3] = 255;
	}

	rgbimg.reset(new Fl_RGB_Image(imgbuf.data(), 1000, 1000, 4));
}


MapGui::MapGui(const FactorioGame* game) : impl(new _MapGui_impl(game)) {}
MapGui::~MapGui() {}

_MapGui_impl::_MapGui_impl(const FactorioGame* game)
{
	window.reset(new Fl_Window(340,180));
	mapbox.reset(new Fl_Box(20,40,300,100,"Hello, World!"));
	mapbox->box(FL_UP_BOX);
	mapbox->labelfont(FL_BOLD+FL_ITALIC);
	mapbox->labelsize(36);
	mapbox->labeltype(FL_SHADOW_LABEL);
	window->end();
	window->show();
}

double GUI::wait(double t)
{
	return Fl::wait(t);
}

} // namespace GUI

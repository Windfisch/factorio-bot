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

#include <memory>
#include <vector>
#include <cassert>
#include "../pos.hpp"

class FactorioGame;

namespace GUI
{

struct Color {
	uint8_t r,g,b;
	Color() : r(0),g(0),b(0){}
	Color(int rr,int gg,int bb):r(uint8_t(rr)),g(uint8_t(gg)),b(uint8_t(bb)){ assert(rr<256 && gg<256 && bb<256); }
	void blend(const Color& other, float alpha);
};

Color color_hsv(double hue, double sat, double val);
Color get_color(int id);

class _MapGui_impl;

class MapGui
{
	public:
		MapGui(FactorioGame* game, const char* datapath);
		~MapGui();

		void line(Pos a, Pos b, Color c);
		void rect(Pos a, Pos b, Color c);
		void rect(Pos a, int size, Color c);
		void clear();

		int key();

	private:
		std::unique_ptr<_MapGui_impl> impl;
};

double wait(double t);

} //namespace GUI

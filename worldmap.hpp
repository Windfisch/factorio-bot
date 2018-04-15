/* This WorldMap implementation is dual-licensed under both the
 * terms of the MIT and the GPL3 license. You can choose which
 * of those licenses you want to use. Note that the MIT option
 * only applies to this file, and not to the rest of
 * factorio-bot (unless stated otherwise).
 */

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

/*
 * MIT License
 *
 * Copyright (c) 2017 Florian Jung
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this templated WorldMap implementation and associated
 * documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <map>
#include <cassert>
#include <array>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <unordered_map>


#include "pos.hpp"

template <class T> using Chunk = std::array< std::array< T, 32 >, 32 >;

template <class T>
class WorldMap
{
	public:
		template <bool is_const>
		class DumbViewport_
		{
			friend class WorldMap;
			
			private:
				typedef typename std::conditional<is_const, const WorldMap<T>*, WorldMap<T>*>::type parenttype;
				typedef typename std::conditional<is_const, const Chunk<T>*, Chunk<T>*>::type chunktype;

				parenttype parent;

				Pos origin;

				DumbViewport_(parenttype parent_, const Pos& origin_) : parent(parent_), origin(origin_) {}

			public:
				typedef typename std::conditional<is_const, const T&, T&>::type reftype;
				
				reftype at(int x, int y) const
				{
					int tilex = x+origin.x;
					int tiley = y+origin.y;
					int chunkx = chunkidx(tilex);
					int chunky = chunkidx(tiley);
					int relx = tileidx(tilex);
					int rely = tileidx(tiley);

					return (*parent->get_chunk(chunkx, chunky))[relx][rely];
				}

				reftype at(const Pos& pos) const { return at(pos.x, pos.y); }
		};

		template <bool is_const>
		class Viewport_
		{
			friend class WorldMap;
			private:
				typedef typename std::conditional<is_const, const WorldMap<T>*, WorldMap<T>*>::type parenttype;
				typedef typename std::conditional<is_const, const Chunk<T>*, Chunk<T>*>::type chunktype;

				parenttype parent;

				Pos origin;
				Pos lefttop_tile, rightbot_tile; // DEBUG
				std::vector<chunktype> chunkcache;
				Pos lefttop_chunk;
				int xlen_chunks, ylen_chunks;

				Viewport_(parenttype parent_, const Pos& lefttop_tile_, const Pos& rightbot_tile_, const Pos& origin_) : parent(parent_), origin(origin_), lefttop_tile(lefttop_tile_), rightbot_tile(rightbot_tile_)
				{
					lefttop_chunk = Pos::tile_to_chunk(lefttop_tile);
					Pos rightbot_chunk = Pos::tile_to_chunk_ceil(rightbot_tile);

					// recalculate tile borders, because we can only deal with chunks. This means that
					// the actually available area is larger than the requested area.
					lefttop_tile = Pos::chunk_to_tile(lefttop_chunk);
					rightbot_tile = Pos::chunk_to_tile(rightbot_chunk);

					xlen_chunks = rightbot_chunk.x - lefttop_chunk.x;
					ylen_chunks = rightbot_chunk.y - lefttop_chunk.y;

					if (xlen_chunks < 0 || ylen_chunks < 0)
						throw std::invalid_argument("invalid Viewport range");

					chunkcache.resize(xlen_chunks * ylen_chunks);
					for (int x = 0; x < xlen_chunks; x++)
						for (int y = 0; y < ylen_chunks; y++)
							chunkcache[x+y*xlen_chunks] = parent->get_chunk(x+lefttop_chunk.x,y+lefttop_chunk.y);
				}
				
				void extend_self(const Pos& new_lefttop_tile, const Pos& new_rightbot_tile)
				{
					Pos old_lefttop_chunk = lefttop_chunk;
					int old_xlen_chunks = xlen_chunks;
					int old_ylen_chunks = ylen_chunks;
					std::vector<chunktype> old_chunkcache = std::move(chunkcache);



					lefttop_tile = new_lefttop_tile;
					rightbot_tile = new_rightbot_tile;

					lefttop_chunk = Pos::tile_to_chunk(lefttop_tile);
					Pos rightbot_chunk = Pos::tile_to_chunk_ceil(rightbot_tile);
					
					// recalculate tile borders, because we can only deal with chunks. This means that
					// the actually available area is larger than the requested area.
					lefttop_tile = Pos::chunk_to_tile(lefttop_chunk);
					rightbot_tile = Pos::chunk_to_tile(rightbot_chunk);

					xlen_chunks = rightbot_chunk.x - lefttop_chunk.x;
					ylen_chunks = rightbot_chunk.y - lefttop_chunk.y;

					if (xlen_chunks < 0 or ylen_chunks < 0)
						throw std::invalid_argument("invalid Viewport range");

					chunkcache.resize(xlen_chunks * ylen_chunks);
					for (int x = 0; x < xlen_chunks; x++)
						for (int y = 0; y < ylen_chunks; y++)
						{
							int oldx = x + lefttop_chunk.x - old_lefttop_chunk.x;
							int oldy = y + lefttop_chunk.y - old_lefttop_chunk.y;

							if (0 <= oldx && oldx < old_xlen_chunks && 
							    0 <= oldy && oldy < old_ylen_chunks)
								chunkcache[x+y*xlen_chunks] = old_chunkcache[oldx+oldy*old_xlen_chunks];
							else
								chunkcache[x+y*xlen_chunks] = parent->get_chunk(x+lefttop_chunk.x,y+lefttop_chunk.y);
						}
				}

			public:
				typedef typename std::conditional<is_const, const T&, T&>::type reftype;

				Viewport_<is_const> extend(const Pos& lefttop_tile_, const Pos& rightbot_tile_) const
				{
					return Viewport_<is_const>(parent, lefttop_tile_, rightbot_tile_, origin);
				}

				reftype at(int x, int y)
				{
					int tilex = x+origin.x;
					int tiley = y+origin.y;
					if (!((tilex >= lefttop_tile.x) && (tiley >= lefttop_tile.y) && (tilex < rightbot_tile.x) && (tiley < rightbot_tile.y)))
					{
						Pos new_lefttop_tile(lefttop_tile);
						Pos new_rightbot_tile(rightbot_tile);
						if (tilex < lefttop_tile.x) new_lefttop_tile.x = tilex;
						else if (tilex >= rightbot_tile.x) new_rightbot_tile.x = tilex+1;
						if (tiley < lefttop_tile.y) new_lefttop_tile.y = tiley;
						else if (tiley >= rightbot_tile.y) new_rightbot_tile.y = tiley+1;

						extend_self(new_lefttop_tile, new_rightbot_tile);
					}
					int chunkx = chunkidx(tilex) - lefttop_chunk.x;
					int chunky = chunkidx(tiley) - lefttop_chunk.y;
					assert(chunkx >= 0);
					assert(chunkx < xlen_chunks);
					assert(chunky >= 0);
					assert(chunky < ylen_chunks);

					int relx = tileidx(tilex);
					int rely = tileidx(tiley);

					return (*chunkcache[chunkx+chunky*xlen_chunks])[relx][rely];
				}
				reftype at(const Pos& pos) { return at(pos.x, pos.y); }
		};


		typedef DumbViewport_<true> ConstDumbViewport;
		typedef DumbViewport_<false> DumbViewport;
		typedef Viewport_<true> ConstViewport;
		typedef Viewport_<false> Viewport;

		Viewport view(const Pos& lefttop, const Pos& rightbot, const Pos& origin)
		{
			return Viewport(this, lefttop, rightbot, origin);
		}

		ConstViewport view(const Pos& lefttop, const Pos& rightbot, const Pos& origin) const
		{
			return ConstViewport(this, lefttop, rightbot, origin);
		}

		DumbViewport dumb_view(const Pos& origin)
		{
			return DumbViewport(this, origin);
		}

		ConstDumbViewport dumb_view(const Pos& origin) const
		{
			return ConstDumbViewport(this, origin);
		}

		Chunk<T>* get_chunk(int x, int y)
		{
			Chunk<T>* retval = &(storage[ Pos(x,y) ]);
			assert(retval != nullptr);
			return retval;
		}
		
		const Chunk<T>* get_chunk(int x, int y) const
		{
			try
			{
				return &(storage.at( Pos(x,y) ));
			}
			catch (const std::out_of_range&)
			{
				return &dummy_chunk;
			}
		}
		
		const T& at(int x, int y) const
		{
			int chunkx = chunkidx(x);
			int chunky = chunkidx(y);
			int relx = tileidx(x);
			int rely = tileidx(y);

			return (*get_chunk(chunkx, chunky))[relx][rely];
		}
		T& at(int x, int y)
		{
			int chunkx = chunkidx(x);
			int chunky = chunkidx(y);
			int relx = tileidx(x);
			int rely = tileidx(y);

			return (*get_chunk(chunkx, chunky))[relx][rely];
		}
		const T& at(const Pos& pos) const { return at(pos.x, pos.y); }
		T& at(const Pos& pos) { return at(pos.x, pos.y); }

	private:
		std::unordered_map< Pos, Chunk<T> > storage;
		Chunk<T> dummy_chunk;
};

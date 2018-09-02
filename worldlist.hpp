/* This WorldList implementation is dual-licensed under both the
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
 * copy of this templated WorldList implementation and associated
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

#include <iostream>
#include <map>
#include <cassert>
#include <array>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

#include "pos.hpp"
#include "area.hpp"

template <class T, class EqualComparator = std::equal_to<T>>
class WorldList : public std::unordered_map< Pos, std::vector<T> >
{
	public:
		/** returns an upper bound of the radius of the circle around center that contains the whole map */
		double radius(Pos_f center = Pos_f(0.,0.)) const
		{
			double r = 0.;
			for (const auto& iter : (*this))
			{
				auto chunk_midpoint = Pos_f::chunk_to_tile(iter.first.to_double() + Pos_f(0.5,0.5));
				r = std::max(r, (chunk_midpoint - center).len() + 0.71 * 32  ); // 0.71 = sqrt(2)/2, rounded up.
			}
			return r;
		}

		template <bool is_const>
		class range_iterator : public std::iterator<std::bidirectional_iterator_tag,
			T, // value_type
			typename std::vector<T>::iterator::difference_type,
			typename std::conditional<is_const, const T*, T*>::type, // pointer
			typename std::conditional<is_const, const T&, T&>::type> // reference
		{
			friend class WorldList;

			private:
				typedef typename std::conditional<is_const, const T&, T&>::type reftype;
				typedef typename std::conditional<is_const, const T*, T*>::type ptrtype;
				typedef std::unordered_map<Pos, std::vector<T>> maptype;
				typedef typename std::conditional<is_const, const WorldList<T,EqualComparator>*, WorldList<T,EqualComparator>*>::type parentptr;
				parentptr parent;
				Area_f range;

				Pos left_top; // incl
				Pos right_bottom; // incl
				Pos curr_pos;
				typedef typename std::conditional<is_const,
					typename std::vector<T>::const_iterator,
					typename std::vector<T>::iterator>::type
					iter_t;
				typedef typename std::conditional<is_const,
					const std::vector<T>*,
					std::vector<T>*>::type
					vec_t;
				typedef typename std::conditional<is_const,
					typename std::unordered_map< Pos, std::vector<T> >::const_iterator,
					typename std::unordered_map< Pos, std::vector<T> >::iterator >::type
					mapiter_t;
				
				vec_t curr_vec;
				iter_t iter;

				// advances curr_pos one or more times until the next existing chunk has been found.
				// returns true on success and false if we moved past the end.
				bool next_chunk()
				{
					mapiter_t it;
					do
					{
						curr_pos.x++;
						if (curr_pos.x > right_bottom.x)
						{
							curr_pos.x = left_top.x;
							curr_pos.y++;

							if (curr_pos.y > right_bottom.y)
								return false;
						}

						it = parent->find(curr_pos);
					} while (it == parent->end() || it->second.empty());

					curr_vec = &(it->second);
					return true;
				}

				// retreats curr_pos one or more times until the previous existing chunk has been found.
				// returns true on success and false if we moved past the beginning.
				bool prev_chunk()
				{
					mapiter_t it;
					do
					{
						curr_pos.x--;
						if (curr_pos.x < left_top.x)
						{
							curr_pos.x = right_bottom.x;
							curr_pos.y--;

							if (curr_pos.y < left_top.y)
								return false;
						}

						it = parent->find(curr_pos);
					} while (it == parent->end() || it->second.empty());

					curr_vec = &(it->second);
					return true;
				}

				// advances iter one or more times until the next valid entry has been found.
				void incr()
				{
					assert(parent);
					assert(curr_vec);
					assert(iter != curr_vec->end());

					do
					{
						++iter;
						if (iter == curr_vec->end())
						{
							if (next_chunk())
								iter = curr_vec->begin();
							else
								break;
							// else stay at this end() position.
						}
					} while (!range.contains(iter->get_pos()));
				}

				// retreats iter one or more times until the next valid entry has been found.
				void decr()
				{
					assert(parent);
					assert(curr_vec);

					do
					{
						if (iter == curr_vec->begin())
						{
							if (!prev_chunk())
								assert(false);
						}
					} while (!range.contains(iter->get_pos()));
				}

				range_iterator(parentptr parent_, const Area_f& range_, bool give_end=false) :
					parent(parent_), range(range_),
					left_top(Pos::tile_to_chunk(range_.left_top.to_int_floor())),
					right_bottom(Pos::tile_to_chunk_ceil(range_.right_bottom.to_int_floor()))
				{
					curr_vec = nullptr;

					if (!give_end)
					{
						curr_pos = left_top;
						curr_pos.x--;
						next_chunk();
						
						if (curr_vec)
						{
							iter = curr_vec->begin();
							if (!range.contains(iter->get_pos()) )
								incr();
						}
					}
					else
					{
						curr_pos = right_bottom;
						curr_pos.x++;
						prev_chunk();
						
						if (curr_vec)
							iter = curr_vec->end();
					}
				}
				range_iterator(parentptr parent_, const Area& range_) : range_iterator(parent_, Area_f(range_.left_top.to_double(), range_.right_bottom.to_double())) {}

			public:
				range_iterator() : parent(nullptr) {}

				// copy-ctor
				//range_iterator(const range_iterator<is_const>&) = default;

				bool operator==(range_iterator<is_const> other) const
				{
					if (!parent) return this->parent == other.parent;
					else return this->parent == other.parent && this->range == other.range && this->curr_vec == other.curr_vec && (!this->curr_vec || !other.curr_vec || this->iter == other.iter);
				}

				bool operator!=(range_iterator<is_const> other) const { return !(*this==other); }

				reftype operator*() const
				{
					assert(curr_vec);
					assert(iter != curr_vec->end());
					return *iter;
				}
				
				ptrtype operator->() const { return &**this; }

				range_iterator<is_const>& operator++()
				{
					incr();
					return *this;
				}

				range_iterator<is_const>& operator--()
				{
					decr();
					return *this;
				}

				range_iterator<is_const> operator++(int)
				{
					range_iterator<is_const> tmp = *this;
					++(*this);
					return tmp;
				}
				range_iterator<is_const> operator--(int)
				{
					range_iterator<is_const> tmp = *this;
					--(*this);
					return tmp;
				}
		};

		template <bool is_const>
		class Range_
		{
			private:
				friend class WorldList;
				typedef typename std::conditional<is_const, const WorldList<T,EqualComparator>*, WorldList<T,EqualComparator>*>::type ptr_type;
				ptr_type parent;
				Area_f area;
				Range_(ptr_type parent_, Area_f area_) : parent(parent_), area(area_) {}
			
			public:
				typedef WorldList<T,EqualComparator>::range_iterator<is_const> iterator;

				// allows to construct a ConstRange from a Range
				Range_( const Range_<false>& other ) : parent(other.parent), area(other.area) {}
				
				// this could be done more nicely with even more template metaprogramming
				// Range_ is not const-correct. this means, if you have a const reference
				// of a Range_, you cannot do anything with it. But there's no need to
				// pass around any references of ranges, since they're so small. Also,
				// Ranges are usually used like this:
				// for (auto& foo : mycontainer.range(...)) {...}, i.e. they only live as
				// anonymous temporary variables.
				iterator begin() { return iterator(parent, area); }
				iterator end() { return iterator(parent, area, true); }
		};

		typedef Range_<true> ConstRange;
		typedef Range_<false> Range;



		template <bool is_const>
		class around_iterator : public std::iterator<std::bidirectional_iterator_tag,
			T, // value_type
			typename std::vector<T>::iterator::difference_type,
			typename std::conditional<is_const, const T*, T*>::type, // pointer
			typename std::conditional<is_const, const T&, T&>::type> // reference
		{
			friend class WorldList;

			private:
				typedef typename std::conditional<is_const, const T&, T&>::type reftype;
				typedef typename std::conditional<is_const, const T*, T*>::type ptrtype;
				typedef std::unordered_map<Pos, std::vector<T>> maptype;
				typedef typename std::conditional<is_const, const WorldList<T,EqualComparator>*, WorldList<T,EqualComparator>*>::type parentptr;

				typedef typename std::conditional<is_const,
					typename std::vector<T>::const_iterator,
					typename std::vector<T>::iterator>::type
					veciter_t;
				typedef typename std::conditional<is_const,
					const std::vector<T>*,
					std::vector<T>*>::type
					vec_t;

				parentptr parent;
				Pos_f center;

				double inner_radius;
				double outer_radius;

				struct worklist_t
				{
					vec_t vector_ptr;
					veciter_t iterator_in_vector;
					double distance;
					bool operator< (const worklist_t& other) { return this->distance < other.distance; }
				};

				typename std::vector<worklist_t> worklist;
				typename std::vector<worklist_t>::iterator worklist_iterator;

				void prepare_worklist()
				{
					// enumerate all items with distance from center d fulfilling: inner_radius <= d < outer_radius.
					const double outer_len = outer_radius;
					const Pos_f outer_shift(outer_len, outer_len);
					const double inner_len = inner_radius / 1.414214 /* this is slightly larger than sqrt(2) */;
					const Pos_f inner_shift(inner_len, inner_len);

					Area_f outer(center-outer_shift, center+outer_shift);
					Area_f inner(center-inner_shift, center+inner_shift);

					// all relevant entities are guaranteed to be contained in outer, but not contained in inner.
					
					// now split that "rectangle with hole" into four rectangles (or don't split at all, if inner_len is too small)
					Area_f ranges[4];
					size_t n_ranges;
					if (inner_len > 32) // will it be useful to spare the inner part
					{
						n_ranges = 4;
						ranges[0] = Area_f( outer.left_top, Pos_f(outer.right_bottom.x, inner.left_top.y) );
						ranges[1] = Area_f( outer.left_top.x, inner.left_top.y,
						                    inner.left_top.x, inner.right_bottom.y );
						ranges[2] = Area_f( inner.right_bottom.x, inner.left_top.y,
						                    outer.right_bottom.x, inner.right_bottom.y );
						ranges[3] = Area_f( Pos_f(outer.left_top.x, inner.right_bottom.y), outer.right_bottom );
					}
					else
					{
						n_ranges = 1;
						ranges[0] = outer;
					}

					// enumerate all relevant objects and insert them into a list
					worklist.clear();
					for (size_t i=0; i<n_ranges; i++)
					{
						auto range = parent->range(ranges[i]);
						auto end = range.end();
						for (auto it = range.begin(); it != end; it++)
						{
							reftype item = *it;

							double distance = (item.get_pos() - center).len();
							if (!(inner_radius <= distance && distance < outer_radius))
								continue;

							// if we reach this line, the item is relevant for the ring we're considering.
							worklist.emplace_back(worklist_t{it.curr_vec, it.iter, distance});
						}
					}

					// sort that list by distance
					std::sort(worklist.begin(), worklist.end());
				}

				around_iterator(parentptr parent_, const Pos_f& center_, bool give_end=false) :
					parent(parent_), center(center_)
				{
					inner_radius = 0.;
					outer_radius = 0.;

					if (!give_end)
					{
						worklist.clear();
						worklist.emplace_back(); // put a dummy inside the worklist
						worklist_iterator = worklist.begin();
						// increment worklist_iterator, which will then point to worklist.end(), leading to an expansion
						// of the outer_radius. effectively, this will make the iterator point to the first element.
						(*this)++;
					}
					else
					{
						worklist.clear();
						worklist_iterator = worklist.end();
					}
				}
				around_iterator(parentptr parent_, const Pos& center_) : around_iterator(parent_, Pos_f(center_.to_double())) {}

			public:
				around_iterator() : parent(nullptr) {}

				// copy-ctor
				around_iterator(const around_iterator<is_const>& other)
				{
					this->parent = other.parent;
					this->center = other.center;
					this->inner_radius = other.inner_radius;
					this->outer_radius = other.outer_radius;
					this->worklist = other.worklist;
					this->worklist_iterator = this->worklist.begin() + (other.worklist_iterator - other.worklist.begin());
				}

				bool operator==(around_iterator<is_const> other) const
				{
					if (!parent) return this->parent == other.parent;
					else return this->parent == other.parent && this->center == other.center && 
						( (this->worklist.empty() && other.worklist.empty()) || // are both pointing to the end()?
						  (!this->worklist.empty() && !other.worklist.empty() && this->worklist_iterator->iterator_in_vector == other.worklist_iterator->iterator_in_vector) ); // if not end(), do both point to the same item?
				}

				bool operator!=(around_iterator<is_const> other) const { return !(*this==other); }

				reftype operator*() const
				{
					assert(worklist_iterator != worklist.end());
					return *worklist_iterator->iterator_in_vector;
				}
				
				ptrtype operator->() const { return &**this; }

				around_iterator<is_const>& operator++()
				{
					assert(worklist_iterator != worklist.end());
					worklist_iterator++;

					double world_radius = -1.; // -1 means uninitialized

					while (worklist_iterator == worklist.end())
					{
						// need to increase the search ring
						inner_radius = outer_radius;
						outer_radius = inner_radius * 2. + 32; // FIXME more sophisticated strategy for selecting the new outer_radius

						prepare_worklist();
						worklist_iterator = worklist.begin();

						if (worklist.empty()) // empty worklist. let's check whether increasing the outer_radius further can get more entities
						{
							if (world_radius < 0.)
								world_radius = parent->radius(center);
							
							if (inner_radius > world_radius) // we've reached the end-of-world. no more items exist.
								break;
						}
					}

					return *this;
				}

				around_iterator<is_const>& operator--()
				{
					assert(worklist_iterator != worklist.end());
					assert(outer_radius > 0.);
					
					while (worklist_iterator == worklist.begin())
					{
						outer_radius = inner_radius;
						inner_radius = std::max(0., (outer_radius - 32. ) / 2.); // FIXME more sophisticated strategy, see above
						if (inner_radius <= 0.) inner_radius = 0.;

						prepare_worklist();
						worklist_iterator = worklist.end();
					}
					assert(!worklist.empty()); // worklist.empty() implies worklist.begin() == .end() == worklist_iterator, which
					                           //implies that we cannot have left the while loop above. a contradiction.

					worklist_iterator--;
					return *this;
				}

				around_iterator<is_const> operator++(int)
				{
					around_iterator<is_const> tmp = *this;
					++(*this);
					return tmp;
				}
				around_iterator<is_const> operator--(int)
				{
					around_iterator<is_const> tmp = *this;
					--(*this);
					return tmp;
				}
		};

		template <bool is_const>
		class Around_
		{
			private:
				friend class WorldList;
				typedef typename std::conditional<is_const, const WorldList<T,EqualComparator>*, WorldList<T,EqualComparator>*>::type ptr_type;
				ptr_type parent;
				Pos_f center;
				Around_(ptr_type parent_, Pos_f center_) : parent(parent_), center(center_) {}
			
			public:
				typedef WorldList<T,EqualComparator>::around_iterator<is_const> iterator;

				// allows to construct a ConstAround from a Around
				Around_( const Around_<false>& other ) : parent(other.parent), center(other.center) {}
				
				// this could be done more nicely with even more template metaprogramming
				// Around_ is not const-correct. this means, if you have a const reference
				// of a Around_, you cannot do anything with it. But there's no need to
				// pass around any references of arounds, since they're so small. Also,
				// Arounds are usually used like this:
				// for (auto& foo : mycontainer.around(...)) {...}, i.e. they only live as
				// anonymous temporary variables.
				iterator begin() { return iterator(parent, center); }
				iterator end() { return iterator(parent, center, true); }
		};

		typedef Around_<true> ConstAround;
		typedef Around_<false> Around;



		/** returns an iterable wrapper that enumerates all items sorted by their distance to `center` */
		ConstAround around(const Pos_f& center) const { return ConstAround(this, center); }
		/** returns an iterable wrapper that enumerates all items sorted by their distance to `center` */
		Around around(const Pos_f& center) { return Around(this, center); }

		/** returns an iterable wrapper that enumerates all items contained in `area` in arbitrary order */
		ConstRange range(const Area_f& area) const { return ConstRange(this, area); }
		/** returns an iterable wrapper that enumerates all items contained in `area` in arbitrary order */
		Range range(const Area_f& area) { return Range(this, area); }
		
		/** inserts `thing` to the WorldList, copy */
		void insert(const T& thing)
		{
			(*this)[Pos::tile_to_chunk(thing.get_pos().to_int_floor())].emplace_back(thing);
		}

		/** inserts `thing` to the WorldList, move */
		void insert(T&& thing)
		{
			(*this)[Pos::tile_to_chunk(thing.get_pos().to_int_floor())].emplace_back(std::move(thing));
		}

		/** inserts all objects in the `things` container to the WorldList */
		template<typename container>
		void insert_all(const container& things)
		{
			for (const auto& thing : things)
				insert(thing);
		}

		/** erases the thing pointed to by `iter` from the WorldList. Invalidates `iter`.
		  * Range::iterators pointing to earlier items as well as Range::iterators pointing to items in different chunks
		  * remain valid, while Range::iterators after `iter` in the same chunk get invalidated.
		  * Around::iterators that contain the thing pointed to by `iter` in their inner_/outer_radius are invalidated,
		  * while those who are in a different ring remain valid.
		  */
		void erase(typename Around::iterator iter)
		{
			assert(iter.parent == this);
			assert(!iter.worklist.empty());
			assert(iter.worklist_iterator != iter.worklist.end());
			assert(iter.worklist_iterator->vector_ptr);
			assert(iter.worklist_iterator->iterator_in_vector != iter.worklist_iterator->vector_ptr->end());

			iter.worklist_iterator->vector_ptr->erase(iter.worklist_iterator->iterator_in_vector);
		}

		/** erases the thing pointed to by `iter` from the WorldList, returning a Range::iterator to the next thing.
		  * Range::iterators pointing to earlier items as well as Range::iterators pointing to items in different chunks
		  * remain valid, while Range::iterators after `iter` in the same chunk get invalidated.
		  * Around::iterators that contain the thing pointed to by `iter` in their inner_/outer_radius are invalidated,
		  * while those who are in a different ring remain valid.
		  */
		typename Range::iterator erase(typename Range::iterator iter)
		{
			assert(iter.parent == this);
			assert(iter.curr_vec);
			assert(!iter.curr_vec->empty());
			assert(iter.iter != iter.curr_vec->end());

			auto tmp = iter.iter;
			if (++tmp == iter.curr_vec->end()) // we're erasing the last element of a vector
			{
				typename Range::iterator result = iter;
				result++;
				// this will move result to the begin of the next chunk (if it exists)
				
				assert(result.curr_vec);
				if (result.curr_vec != iter.curr_vec)
				{
					iter.curr_vec->erase(iter.iter);
					
					assert(result.curr_vec != iter.curr_vec);
					// this implies that the erasure has not invalidated result.iter
				}
				else
				{
					assert(result.iter == result.curr_vec->end());
					assert(result.curr_vec == iter.curr_vec);
					
					iter.curr_vec->erase(iter.iter);
					// this has invalidated result.iter. let's fix that
					result.iter = result.curr_vec->end();
				}

				return result;
			}
			else // we're erasing in the middle of a vector
			{
				std::swap(*iter.iter, iter.curr_vec->back());
				iter.curr_vec->pop_back();

				// iter still points to the same location, which now holds a different element.
				// we need to check whether that element fulfills the filter criteria of the iterator
				// and if not, advance the iterator.
				typename Range::iterator result = iter;
				if (!result.range.contains(result.iter->get_pos()) )
					result.incr();
				
				return result;
			}
		}

		T& search(T what)
		{
			T* result = search_or_null(what);
			if (result)
				return *result;
			else
				throw std::runtime_error("element not present");
		}

		T* search_or_null(T what)
		{
			EqualComparator comp;
			auto& vec = (*this)[Pos::tile_to_chunk(what.get_pos().to_int_floor())];
			for (auto& t : vec)
				if (comp(t,what))
					return &t;
			return nullptr;
		}
};

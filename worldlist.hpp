#pragma once

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

template <class T>
class WorldList : public std::unordered_map< Pos, std::vector<T> >
{
	public:
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
				typedef typename std::conditional<is_const, const WorldList<T>*, WorldList<T>*>::type parentptr;
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
					} while (it == parent->end());

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
					} while (!range.contains((*iter)->pos));
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
					} while (!range.contains((*iter)->pos));
				}

			public:
				// FIXME: i don't like this; these two ctors should be private. but how to do this?
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
							if (!range.contains((*iter)->pos) )
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

				range_iterator() : parent(nullptr) {}

				// copy-ctor
				range_iterator(const range_iterator<is_const>&) = default;

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
				typedef typename std::conditional<is_const, const WorldList<T>*, WorldList<T>*>::type ptr_type;
				ptr_type parent;
				Area_f area;
				Range_(ptr_type parent_, Area_f area_) : parent(parent_), area(area_) {}
			
			public:
				typedef WorldList<T>::range_iterator<is_const> iterator;

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

		ConstRange range(const Area_f& area) const { return ConstRange(this, area); }
		Range range(const Area_f& area) { return Range(this, area); }
		
		/** inserts `thing` to the WorldList */
		void insert(T&& thing)
		{
			(*this)[Pos::tile_to_chunk(thing->pos.to_int())].emplace_back(std::move(thing));
		}

		/** erases the thing pointed to by `iter` from the WorldList, returning an iterator to the next thing.
		  * iterators pointing to earlier items as well as iterators pointing to items in different chunks
		  * remain valid, while iterators after `iter` in the same chunk get invalidated */
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
				assert(result.curr_vec != iter.curr_vec);
				assert(result.curr_vec);
				assert(result.iter == result.curr_vec->begin() || result.iter == result.curr_vec->end());

				iter.curr_vec->erase(iter.iter);

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
				if (!result.range.contains((*result.iter)->pos) )
					result.incr();
				
				return result;
			}
		}
};

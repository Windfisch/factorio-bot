/* This multivariant.hpp file is dual-licensed under both the
 * terms of the MIT and the GPL3 license. You can choose which
 * of those licenses you want to use. Note that the MIT option
 * only applies to this file, and not to the rest of
 * factorio-bot (unless stated otherwise).
 */

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

/*
 * MIT License
 *
 * Copyright (c) 2017, 2018 Florian Jung
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

#include <utility>
#include <stdexcept>

template <typename... Ts> struct typelist;

struct refcount_base
{
	std::size_t refcount;
};

template <typename T> struct refcounted : public refcount_base
{
	T data;

	template<typename... As> refcounted(As&&... args) : data(std::forward<As>(args)...) {}
};


namespace detail {

// typelist_index< T, typelist< S1, ..., Sn > >::value will be i if Si == T
template <typename T, typename Types > struct typelist_index;
template <typename T, typename... Rest>
	struct typelist_index<T, typelist<T, Rest...>>
	{
		static constexpr size_t value = 0;
	};
template <typename T, typename NotT, typename... Rest>
	struct typelist_index<T, typelist<NotT, Rest...>>
	{
		static constexpr size_t value = typelist_index<T, typelist<Rest...>>::value + 1;
	};
template <typename T>
	struct typelist_index<T, typelist<>>
	{
		static_assert(sizeof(T)!=sizeof(T), "Type is not in list");
	};


template <typename Types, size_t I, typename Args> struct make_helper;
template <size_t I, typename T, typename... Ts, typename... As>
	struct make_helper<typelist<T,Ts...>, I, typelist<As...>>
	{
		static constexpr refcount_base* make(size_t type_idx, As&&... args)
		{
			if (type_idx == I)
				return static_cast<refcount_base*>( new refcounted<T>(std::forward<As>(args)...) );
			else
				return make_helper<typelist<Ts...>, I+1, typelist<As...>>::make(type_idx, std::forward<As>(args)...);
		}
	};
template <size_t I, typename... As>
	struct make_helper<typelist<>, I, typelist<As...>>
	{
		static constexpr refcount_base* make(size_t, As...)
		{
			return nullptr;
		}
	};

template <typename Types, size_t I> struct delete_helper;
template <size_t I, typename T, typename... Ts>
	struct delete_helper<typelist<T,Ts...>, I>
	{
		static void del(size_t type_idx, refcount_base* ptr)
		{
			if (type_idx == I)
				delete static_cast<refcounted<T>*>(ptr);
			else
				delete_helper<typelist<Ts...>, I+1>::del(type_idx, ptr);
		}
	};
template <size_t I>
	struct delete_helper<typelist<>, I>
	{
		static void del(size_t, refcount_base*)
		{
			throw std::out_of_range("invalid type id");
		}
	};

template <typename Types, size_t I, typename S> struct is_a_helper;
template <typename T, typename... Ts, size_t I, typename S>
	struct is_a_helper<typelist<T, Ts...>, I, S>
	{
		static constexpr bool is_a(size_t type_idx)
		{
			if (type_idx == I)
				return std::is_base_of<S,T>::value;
			else
				return is_a_helper<typelist<Ts...>, I+1, S>::is_a(type_idx);
		}
	};
template <size_t I, typename S>
	struct is_a_helper<typelist<>, I, S>
	{
		static constexpr bool is_a(size_t)
		{
			throw std::out_of_range("invalid type id");
			return false;
		}
	};
		


// get_helper_2 is used to check whether S is convertible to T at all.
// if yes (ok==true), then magic() checks for that type_idx;
// if no, then magic() just ignores that type in the typelist
//        and instantiates get_helper_1 with the remainder of the list
//        (get_helper_1 will return nullptr/throw if the remainder is empty)
template <typename T, size_t I, typename Types> struct get_helper_1;
template <bool ok, typename T, size_t I, typename Types> struct get_helper_2;

template <typename T, size_t I, typename S, typename... Ss>
	struct get_helper_2<true, T,I,typelist<S,Ss...>>
{
	static constexpr T* magic(refcount_base* ptr, size_t type_idx)
	{
		if (type_idx == I)
		{
			return static_cast<T*>( &static_cast<refcounted<S>*>(ptr)->data );
		}
		else
		{
			return get_helper_1<T, I+1, typelist<Ss...>>::magic(ptr, type_idx);
		}
	}
};
template <typename T, size_t I, typename S, typename... Ss>
	struct get_helper_2<false, T,I,typelist<S,Ss...>>
{
	static constexpr T* magic(refcount_base* ptr, size_t type_idx)
	{
		return get_helper_1<T, I+1, typelist<Ss...>>::magic(ptr, type_idx);
	}
};

template <typename T, size_t I, typename S, typename... Ss>
	struct get_helper_1<T, I, typelist<S, Ss...>>
		: public get_helper_2<std::is_base_of<T,S>::value, T, I, typelist<S, Ss...>> {};
template <typename T, size_t I>
	struct get_helper_1<T, I, typelist<>>
	{
		static constexpr T* magic(refcount_base*, size_t)
		{
			return nullptr;
		}
	};

template <typename T, typename Types> constexpr T* get_part (refcount_base* ptr, size_t type_idx)
{
	return get_helper_1<T, 0, Types>::magic(ptr, type_idx);
}

} 

template <typename Types, typename Args = typelist<> > struct multivariant_utils;
template <typename... Ts, typename... As>
	struct multivariant_utils<typelist<Ts...>, typelist<As...>>
	{
		static constexpr refcount_base* make(size_t type_idx, As&&... args)
		{
			return detail::make_helper<typelist<Ts...>, 0, typelist<As...>>::make(type_idx, std::forward<As>(args)...);
		}
		static constexpr void del(size_t type_idx, refcount_base* ptr)
		{
			return detail::delete_helper<typelist<Ts...>, 0>::del(type_idx, ptr);
		}
		template<typename T> static constexpr T* get(size_t type_idx, refcount_base* ptr)
		{
			if (T* result = detail::get_helper_1<T, 0, typelist<Ts...>>::magic(ptr, type_idx))
				return result;
			else
				throw std::runtime_error("impossible cast requested");
		}
		template<typename T> static constexpr T* get_or_null(size_t type_idx, refcount_base* ptr)
		{
			return detail::get_helper_1<T, 0, typelist<Ts...>>::magic(ptr, type_idx);
		}
		template<typename T> static constexpr size_t index()
		{
			return detail::typelist_index<T, typelist<Ts...>>::value;
		}
		/** is_a<Foo>( index<Bar>() ) == true iif Bar is derived from or equal to Foo, and false otherwise */
		template<typename T> static constexpr bool is_a(size_t index);

		static constexpr size_t invalid_index = SIZE_MAX;
	};

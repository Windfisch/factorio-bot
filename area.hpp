#pragma once
#include <type_traits>
#include <string>
#include <cmath>
#include <algorithm>
#include "pos.hpp"

template<typename T> struct Area_
{
	Pos_<T> left_top;
	Pos_<T> right_bottom;

	Area_() {}
	Area_(T x1, T y1, T x2, T y2) : left_top(x1,y1), right_bottom(x2,y2)  {}
	Area_(const Pos_<T>& lt, const Pos_<T>& rb) : left_top(lt), right_bottom(rb) {}
	Area_(std::string str);
	
	std::string str() const;

	void normalize()
	{
		if (left_top.x > right_bottom.x)
			std::swap(left_top.x, right_bottom.x);
		if (left_top.y > right_bottom.y)
			std::swap(left_top.y, right_bottom.y);
	}

	Pos_<T> center() const { return (left_top + right_bottom) / 2; }

	bool contains(const Pos_<T>& p) const { return left_top <= p && p < right_bottom; }
	bool contains_x(T x) const { return left_top.x <= x && x < right_bottom.x; }
	bool contains_y(T y) const { return left_top.y <= y && y < right_bottom.y; }
	
	template <typename U=T> typename std::enable_if< std::is_floating_point<U>::value, Area_<int> >::type
	/*Area_<int>*/ outer() const
	{
		return Area_<int>( Pos_<int>(int(std::floor(left_top.x)), int(std::floor(left_top.y))),
		                   Pos_<int>(int(std::ceil(right_bottom.x)), int(std::ceil(right_bottom.y))) );
	}

	Area_<T> shift(Pos_<T> offset) const { return Area_<T>(left_top+offset, right_bottom+offset); }
	Area_<T> expand(T radius) const { return Area_<T>(left_top-Pos_<T>(radius,radius), right_bottom+Pos_<T>(radius,radius)); }
	T radius() const { return std::max( std::max( left_top.x, left_top.y), std::max( right_bottom.x, right_bottom.y) ); }
	Area_<T> intersect(Area_<T> other) const
	{
		return Area_<T>(
				Pos_<T>(
					std::max(this->left_top.x, other.left_top.x),
					std::max(this->left_top.y, other.left_top.y)
				),
				Pos_<T>(
					std::min(this->right_bottom.x, other.right_bottom.x),
					std::min(this->right_bottom.y, other.right_bottom.y)
				)
			);
	}
};

typedef Area_<int> Area;
typedef Area_<double> Area_f;

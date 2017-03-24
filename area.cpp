#include <string>
#include <cstdio>
#include "area.hpp"

using std::string;
using std::sscanf;

template <>
Area_<int>::Area_(string str)
{
	sscanf(str.c_str(), "%i,%i;%i,%i", &left_top.x, &left_top.y, &right_bottom.x, &right_bottom.y);
}

template <>
Area_<double>::Area_(string str)
{
	sscanf(str.c_str(), "%lf,%lf;%lf,%lf", &left_top.x, &left_top.y, &right_bottom.x, &right_bottom.y);
}

template <typename T>
string Area_<T>::str() const
{
	return left_top.str() + " -- " + right_bottom.str();
}

template struct Area_<int>;
template struct Area_<double>;

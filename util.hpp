#pragma once

#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

constexpr int sane_div(int a, unsigned b)
{
	return (a>=0) ? (a/b) : (signed(a-b+1)/signed(b));
}

constexpr int sane_mod(int a, unsigned b)
{
	return ((a%b)+b)%b;
}

auto min_element_if = [](auto& container, auto predicate, auto compare)
{
	auto best = container.end();

	for (auto it=container.begin(); it!=container.end(); it++)
		if (predicate(*it))
		{
			if (best==container.end() || compare(*it, *best))
				best = it;
		}
	
	return best;
};

static std::string strpad(std::string s, size_t n)
{
	if (s.length() >= n) return s;
	else return s + std::string(n-s.length(),' ');
}

#pragma GCC diagnostic pop

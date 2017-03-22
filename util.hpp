#pragma once

constexpr int sane_div(int a, unsigned b)
{
	return (a>=0) ? (a/b) : (signed(a-b+1)/signed(b));
}

constexpr int sane_mod(int a, unsigned b)
{
	return ((a%b)+b)%b;
}

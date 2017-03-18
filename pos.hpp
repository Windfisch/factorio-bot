#pragma once
#include <string>

constexpr int tileidx(int x)
{
	return ((x % 32) + 32) % 32;
}

constexpr int chunkidx(int x)
{
	return (x>=0) ? x/32 : (x-31)/32;
}

struct Pos
{
	int x,y;
	Pos(int x_, int y_) : x(x_), y(y_) {}
	Pos() : x(0), y(0) {}

	Pos operator+(const Pos& b) const { return Pos(x+b.x, y+b.y); }
	Pos operator-(const Pos& b) const { return Pos(x-b.x, y-b.y); }

	bool operator==(const Pos that) const
	{
		return this->x == that.x && this->y == that.y;
	}

	bool operator< (const Pos& that) const { return this->x <  that.x && this->y <  that.y; }
	bool operator<=(const Pos& that) const { return this->x <= that.x && this->y <= that.y; }
	bool operator> (const Pos& that) const { return this->x >  that.x && this->y >  that.y; }
	bool operator>=(const Pos& that) const { return this->x >= that.x && this->y >= that.y; }

	std::string str() const { return std::to_string(x) + "," + std::to_string(y); }

	static Pos tile_to_chunk(const Pos& p) { return Pos(chunkidx(p.x), chunkidx(p.y)); }
	static Pos tile_to_chunk_ceil(const Pos& p) { return Pos(chunkidx(p.x+31), chunkidx(p.y+31)); }
};


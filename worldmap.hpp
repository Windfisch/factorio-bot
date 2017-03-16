#include <map>
#include <cassert>
#include <array>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <iostream>

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

	bool operator==(const Pos that) const
	{
		return this->x == that.x && this->y == that.y;
	}

	std::string str() { return std::to_string(x) + "," + std::to_string(y); }

	static Pos tile_to_chunk(const Pos& p) { return Pos(chunkidx(p.x), chunkidx(p.y)); }
	static Pos tile_to_chunk_ceil(const Pos& p) { return Pos(chunkidx(p.x+31), chunkidx(p.x+31)); }
};

namespace std {
	template <> struct hash<Pos>
	{
		typedef Pos argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& p) const
		{
			result_type const h1( std::hash<int>{}(p.x) );
			result_type const h2( std::hash<int>{}(p.y) );
			return h1 ^ (h2 << 1);
		}
	};
}

template <class T> using Chunk = std::array< std::array< T, 32 >, 32 >;

template <class T>
class WorldMap
{
	public:
		template <bool is_const>
		class Viewport_
		{
			friend class WorldMap;
			private:
				typedef typename std::conditional<is_const, const WorldMap<T>*, WorldMap<T>*>::type parenttype;
				typedef typename std::conditional<is_const, const Chunk<T>*, Chunk<T>*>::type chunktype;

				parenttype parent;

				Pos lefttop;
				Pos origin;
				std::vector<chunktype> chunkcache;
				Pos lefttop_chunk;
				int xlen_chunks, ylen_chunks;

				Viewport_(parenttype parent_, const Pos& lefttop_tile, const Pos& rightbot_tile, const Pos& origin_) : parent(parent_), origin(origin_)
				{
					lefttop_chunk = Pos::tile_to_chunk(lefttop_tile);
					Pos rightbot_chunk = Pos::tile_to_chunk_ceil(rightbot_tile);

					xlen_chunks = rightbot_chunk.x - lefttop_chunk.x;
					ylen_chunks = rightbot_chunk.y - lefttop_chunk.y;

					if (xlen_chunks <= 0 or ylen_chunks <= 0)
						throw std::invalid_argument("invalid Viewport range");

					chunkcache.resize(xlen_chunks * ylen_chunks);
					for (int x = 0; x < xlen_chunks; x++)
						for (int y = 0; y < ylen_chunks; y++)
							chunkcache[x+y*xlen_chunks] = parent->get_chunk(x+lefttop_chunk.x,y+lefttop_chunk.y);
				}
			public:
				typedef typename std::conditional<is_const, const T&, T&>::type reftype;

				reftype at(int x, int y) const
				{
					int tilex = x+origin.x;
					int tiley = y+origin.y;
					int chunkx = chunkidx(tilex) - lefttop_chunk.x;
					int chunky = chunkidx(tiley) - lefttop_chunk.y;
					assert(chunkx >= 0);
					assert(chunkx < xlen_chunks);
					assert(chunky >= 0);
					assert(chunky < ylen_chunks);

					int relx = tileidx(tilex);
					int rely = tileidx(tiley);


					std::cout<<"chunkcache[1]="<<chunkcache[1]<<std::endl;
					return (*chunkcache[chunkx+chunky*xlen_chunks])[relx][rely];
				}
				reftype at(const Pos& pos) const { return at(pos.x, pos.y); }
		};


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

		Chunk<T>* get_chunk(int x, int y)
		{
			Chunk<T>* retval = &(storage[ Pos(x,y) ]);
			assert(retval != NULL);
			std::cout << "get_chunk returning "<<retval<<std::endl;
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

	private:
		std::unordered_map< Pos, Chunk<T> > storage;
		Chunk<T> dummy_chunk;
};

using namespace std;

struct bla
{
	int foo;
	int bar;
	bla(int f, int b) : foo(f), bar(b) {}
	bla() {foo=42; bar=1337;}
};

int main()
{
	WorldMap<bla> map;
	const WorldMap<bla>& ref = map;

	WorldMap<bla>::Viewport vp1 = map.view(Pos(10,10), Pos(40,40), Pos(0,0));
	auto vp2 = ref.view(Pos(10,10), Pos(40,40), Pos(0,0));

	vp1.at(10,30)=bla(3,1);
	volatile auto x = vp2.at(10,30);
	cout << x.foo << x.bar << endl;

	auto vp3 = map.view(Pos(1000,1000), Pos(1100,1100), Pos(1050,1050));
	auto y = vp3.at(0,0);
	cout << y.foo << endl;
}

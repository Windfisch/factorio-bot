#include <vector>
#include <iostream>
#include "pos.hpp"
#include "area.hpp"

using namespace std;

constexpr unsigned SANE_MOD(int a, unsigned b) { return ((a%b)+b)%b; }

// gives a list of positions, so that U_{i \in result} { [i; i+width[ } \superset array
static vector<int> array_cover(const vector<bool>& array, int width, Pos size)
{
	vector<int> result;

	int i=0;
	while (i<array.size())
	{
		if (array[i])
		{
			result.push_back(i);
			i+=width;
		}
		else
			i++;
	}
	
	// ensure that the last object is not placed outside of array's width. if so, move it inside.
	assert(result.size()<=1 || result[ result.size()-1 ] >= result[ result.size()-2 ]+width);
	if (result.back() > size.x-width)
		result.back() = size.x-width;

	return result;
}


void plan_rectgrid_belt(const std::vector<Pos>& positions, int outerx, int outery, int innerx, int innery)
{
	Area bounding_box(positions);

	Pos size = bounding_box.right_bottom - bounding_box.left_top;

	// FIXME: vector<bool> or vector<uint8_t> for speed?
	
	vector<bool> normal_layout(size.x*size.y, false); // x \times y array
	vector<bool> mirrored_layout(size.x*size.y, false); // y times x array

	for (const Pos& pos : positions)
	{
		normal_layout[pos.y + size.y * pos.x] = true;
		mirrored_layout[pos.x + size.x * pos.y] = true;
	}
}

static void plan_rectgrid_belt_horiz(const vector<bool>& grid, const Pos& size, int outerx, int outery, int innerx, int innery)
{
	assert(outerx%2 == innerx%2 && outery%2 == innery%2);
}

// a belt at position i is located between drill(i-1) and drill(i)
static vector<int> plan_belts(const vector< vector<int> >& rows, int side_max, bool start_south=true)
{
	int a=0;
	int side[2] = {0,0};

	vector<int> belts;
	belts.push_back(start_south ? 1 : 0);

	for (int i= (start_south ? 1 : 0); i<int(rows.size()+1); i+=2)
	{
		int size1 = 0, size2 = 0;
		if (i-1 >= 0)
			size1 = rows[i-1].size();
		if (i < rows.size())
			size2 = rows[i].size();
		a = 1-a;

		if (side[0]+size1 > side_max || side[1]+size2 > side_max)
		{
			// the i-th row must start a new belt
			belts.push_back(i);
			a=0;
			side[0]=side[1]=0;
		}
		side[a] += size1;
		side[1-a] += size2;
		
		cout << (belts.back()!=i ? ">>" : "**") << " i=" << i << ", sides = " << side[0] << "/" << side[1] << " (max=" << side_max << ")" << endl;
	}

	return belts;
}

template <typename T> void dump(const vector<T>& xs, string name = "")
{
	if (name != "")
		cout << name << ": ";
	for (const T& x : xs)
		cout << x << " ";
	cout << endl;
}
template <typename T> void dump(const vector<vector<T>>& vecs, string name = "")
{
	cout << name << "{\n";
	for (const auto& vec : vecs)
	{
		cout << "  ";
		dump(vec);
	}
	cout << "}\n";
}

static void plan_rectgrid_belt_horiz_ystart(const vector<bool>& grid, const Pos& size, int ystart, int outerx, int outery, int innerx, int innery, int side_max, int preferred_y_out, dir4_t x_side_)
{
	assert(-outery < ystart && ystart <= 0);
	assert(outerx%2 == innerx%2 && outery%2 == innery%2);

	if (preferred_y_out < 0) preferred_y_out = 0;
	else if (preferred_y_out >= size.y) preferred_y_out = size.y-1;

	int n_rows = (size.y-ystart + outery-1) / outery;
	
	// plan the grid of machines. FIXME: this doesn't account for "forbidden tiles"
	// caused by other ores in proximity
	vector< vector<int> > rows;
	for (int row = 0; row < n_rows; row++)
	{
		int y1 = (row*outery - ystart); // incl
		int y2 = y1 + outery; // excl

		// for each machine row (which is outery tiles large), perform a logical OR
		// over the tile rows.
		vector<bool> used(size.x, false);
		for (int x = 0; x < size.x; x++)
			for (int y=y1; y<y2; y++)
				if (grid[x*size.y + y])
				{
					used[x] = true;
					break;
				}
		
		// calculate the machines needed for that machine row.
		rows.push_back( array_cover(used, outerx, size) );
	}
	dump(rows, "rows");

	// calculate the rows each belt group must cover, taking
	// into account the maximum belt capacity.
	vector<int> belts;
	bool firstrow_south;
	{
	auto belts1 = plan_belts(rows, side_max, true);
	auto belts2 = plan_belts(rows, side_max, false);
	dump(belts1, "belts1");
	dump(belts2, "belts2");
	if (belts1.size() <= belts2.size())
	{
		belts = move(belts1);
		firstrow_south = true;
	}
	else
	{
		belts = move(belts2);
		firstrow_south = false;
	}
	}
	dump(belts,"belts");


	vector< pair<int,int> > belt_ranges; // (begin incl, end excl)
	for (int i=1; i<belts.size(); i++)
		belt_ranges.emplace_back( belts[i-1], belts[i] );
	belt_ranges.emplace_back( belts.back(), n_rows+1 );
	if (belt_ranges.back().second % 2 != belt_ranges.back().first % 2)
		belt_ranges.back().second++;
	
	// search that rowrange covered by a single belt which contains preferred_y_out
	size_t preferred_rowrange = SIZE_MAX;
	for (size_t i=0; i<belt_ranges.size(); i++)
		if (belt_ranges[i].first*outery + ystart <= preferred_y_out && preferred_y_out < belt_ranges[i].second*outery + ystart)
		{
			// this must be executed exactly once.
			assert(preferred_rowrange == SIZE_MAX);
			preferred_rowrange = i;
		}
	assert(preferred_rowrange != SIZE_MAX);

	enum mine_direction_t { MINE_NORTH = -1, MINE_SOUTH = 1};
	enum layout_direction_t { LAY_LEFT = -1, LAY_RIGHT = 1};
	layout_direction_t x_side = (x_side_ == LEFT) ? LAY_LEFT : LAY_RIGHT;

	// this tells us whether the belts should transport the ore upwards or downwards
	mine_direction_t mine_direction = 
		(preferred_y_out - belt_ranges[preferred_rowrange].first*outery+ystart <
			belt_ranges[preferred_rowrange].second*outery+ystart - preferred_y_out)
		? MINE_NORTH
		: MINE_SOUTH;
	
	cout << "preferred_rowrange = " << preferred_rowrange << ", mine_direction = " << (mine_direction == MINE_NORTH ? "MINE_NORTH" : "MINE_SOUTH") << " = " << mine_direction << endl;

	
	// calculate the directions (right to left or left to right) of the belt rows
	// and layout the belts and the mining drills in their respective orders
	assert(belt_ranges.size() > 0);
	int i=preferred_rowrange;

	struct belt_row_t
	{
		int row;
		layout_direction_t dir;
		bool connect_from_prev;

		int xmin;
		int xmax;

		belt_row_t(int r, layout_direction_t d, bool c) : row(r), dir(d), connect_from_prev(c) {}
	};
	vector<belt_row_t> belt_rows;
	do
	{
		cout << "loop, i = " << i << endl;
		// consider the i-th belt group. start from the preferred mine_direction
		// (i.e., the end of the belt) and lay belts reverse in a zig-zag fashion.
		// also place and connect the miners on the way
		const auto& range = belt_ranges[i];

		// if the belts shall go from south to north, then we start at range.first,
		// i.e. the northmost point
		int start = mine_direction == MINE_NORTH ? range.first : range.second-2;
		int end   = mine_direction == MINE_NORTH ? range.second : range.first-2;

		cout << "\tstart = " << start << ", end = " << end << endl;

		assert (SANE_MOD(start,2) == SANE_MOD(end,2));

		for (int row = start; row != end; row -= 2*mine_direction)
		{
			layout_direction_t lay_dir = layout_direction_t(
				(SANE_MOD(row,4)==SANE_MOD(start,4))
				? x_side
				: -x_side );
			
			int y = row*outery + ystart;
			cout << "\trow = " << row << " @ y = " << y << " should go to " << (lay_dir == LAY_LEFT ? "left" : "right") << endl;

			belt_rows.emplace_back(row, lay_dir, row!=start);
		}

		// advance to the next belt group
		i = (i-mine_direction+belt_ranges.size()) % belt_ranges.size();
	} while (i != preferred_rowrange);


	int prev_x;
	// now we know which machines to places and where the belts should be. we must bring them in an order and add inter-row-connections.
	for (auto i=0; i<belt_rows.size(); i++)
	{
		int row = belt_rows[i].row;
		layout_direction_t lay_dir = belt_rows[i].dir;
		bool connect_from_prev = belt_rows[i].connect_from_prev;
		cout << "\nlaying out belt row " << i << " with direction " << lay_dir << endl;

		int belt_y = row*outery + ystart;

		// get the machines that feed the current belt
		vector< pair<Pos, dir4_t> > machines;
		if (row-1 >= 0)
		{
			for (int machine_x : rows[row-1])
				machines.emplace_back(
					Pos(machine_x, (row-1)*outery+ystart),
					SOUTH);
		}
		if (row < rows.size()) // the if is only for symmetry
		{
			for (int machine_x : rows[row])
				machines.emplace_back(
					Pos(machine_x, row*outery+ystart),
					NORTH);
		}
		if (lay_dir == LAY_LEFT)
			sort(machines.begin(), machines.end(), [](const auto& a, const auto& b) {return a.first.x < b.first.x;});
		else
			sort(machines.begin(), machines.end(), [](const auto& a, const auto& b) {return a.first.x > b.first.x;});

		cout << "machines: ";
		for (auto m : machines) cout << m.first.str() << " ";
		cout << endl;

		const int output_offset = outerx/2; // integer division rounds down
		int xbegin = machines[0].first.x + output_offset;
		int xend = machines.back().first.x + output_offset - lay_dir;
		
		if (connect_from_prev)
		{
			assert(i > 0);
			cout << "\tconnect_from_prev:TODO" << endl;
			int prevrow = belt_rows[i-1].row;
			int prev_belt_y = prevrow*outery + ystart;
			
			int connect_x = lay_dir == LAY_LEFT ? min(prev_x, xbegin) - innery/2 - 1 : max(prev_x, xbegin) + innery/2 + 1;
			
			for (int x = prev_x; x != connect_x; x += lay_dir)
				cout << "\tbelt at " << Pos(x, prev_belt_y).str() << " *" << endl;
			for (int y = prev_belt_y; y != belt_y; y += (prev_belt_y < belt_y ? 1 : -1))
				cout << "\tbelt at " << Pos(connect_x, y).str() << " *" << endl;
			for (int x = connect_x; x != xbegin; x -= lay_dir)
				cout << "\tbelt at " << Pos(x, belt_y).str() << " *" << endl;
		}
		

		int mach_idx = 0;
		int x;
		for (x = xbegin; x != xend; x-=lay_dir)
		{
			cout << "\tbelt at " << Pos(x, belt_y).str() << endl;

			if (x == machines[mach_idx].first.x + output_offset)
			{
				while (mach_idx < machines.size() && x == machines[mach_idx].first.x + output_offset)
				{
					cout << "\tmachine at " << machines[mach_idx].first.str() << endl;
					cout << "\t----------------" << endl;
					mach_idx++;
				}

				if (mach_idx >= machines.size())
					break;
			}
		}

		prev_x = xend;

		belt_rows[i].xmin = machines[0].first.x;
		belt_rows[i].xmax = machines.back().first.x + outerx;
	}
}


static void rect(vector<bool>& v, Pos size, Pos lt, Pos rb, bool val)
{
	for (int x=lt.x; x<rb.x; x++)
		for (int y=lt.y; y<rb.y; y++)
			v[x*size.y + y] = val;
}

typedef vector<int> v;
int main()
{
	auto result = plan_belts( { v(5), v(5),    v(6), v(2),   v(1), v(4),   v(3), v(3) }, 11, false);
	for (auto x : result) cout << x << " ";
	cout << endl;

	Pos size(25,25);
	vector<bool> grid(size.x*size.y, true);
	rect(grid, size, Pos(10,0), Pos(12,7), false);
	rect(grid, size, Pos(14,0), Pos(20,7), false);
	rect(grid, size, Pos(13,17), Pos(22,25), false);

	plan_rectgrid_belt_horiz_ystart(grid, size, /*ystart*/ 0, 5, 5, 3, 3, 10, /*preferred_y_out*/ 8, WEST);
}
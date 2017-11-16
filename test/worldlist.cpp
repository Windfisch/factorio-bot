#include "../worldlist.hpp"
#include "../entity.h"

#include <memory>
#include <iostream>
#include <experimental/propagate_const>
#include <string>

using namespace std;
using std::experimental::propagate_const;

typedef WorldList<propagate_const<unique_ptr<Entity>>> WL;


static void show(const WL& l);
static void test_erase(WL l, size_t i);
static void test_sorted(WL l, Pos_f center);
static WL makeWL();

static void show(const WL& l)
{
	WL::ConstRange r(l.range( Area_f(0,0,100,100) ));
	for (auto& x : r) { cout << "\t" << x->pos.str() << endl; }
}


static void test_erase(WL l, size_t i)
{
	cout << "erasing element #" << i << ": ";

	WL::Range r = l.range( Area_f(0,0,100,100) );

	WL::Range::iterator it = r.begin();
	advance(it, i);

	cout << "elem = " << (*it)->pos.str();

	it = l.erase(it);

	if (it == r.end())
		cout << ", next = (end)" << endl;
	else
		cout << ", next = " << (*it)->pos.str() << endl;

	show(l);
	cout << endl;
}

static void test_sorted(WL l, Pos_f center)
{
	WL::Sorted s = l.sorted(center);

	int i=0;
	cout << "sorting around " << center.str() << endl;
	for (WL::Sorted::iterator it = s.begin(); it != s.end(); it++, i++)
		cout << "\t" << (*it)->pos.str() << "\t(dist = " << ((*it)->pos-center).len() << ")" << endl;
	
	cout << "\tthat's " << i << " objects" << endl;
}

static WL makeWL()
{
	WL l;
	l.insert(make_unique<Entity>(Pos_f(2.5,80.2), nullptr));
	l.insert(make_unique<Entity>(Pos_f(0.4,0.2), nullptr));
	l.insert(make_unique<Entity>(Pos_f(35.4,1.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.4,1.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.5,1.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(100.6,1.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.7,1.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(100.8,1.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.4,41.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.5,41.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(100.6,41.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.7,41.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(99.8,41.5), nullptr));
	l.insert(make_unique<Entity>(Pos_f(2.5,5.2), nullptr));
	l.insert(make_unique<Entity>(Pos_f(-4.2,4), nullptr));
	l.insert(make_unique<Entity>(Pos_f(2.5,101), nullptr));
	l.insert(make_unique<Entity>(Pos_f(101,1.0), nullptr));
	return l;
}

int main()
{
	cout << "original" << endl;
	show(makeWL());
	cout << endl;

	for (size_t i=0; i<11; i++)
		test_erase(makeWL(), i);

	test_sorted(makeWL(), Pos_f(0.,0.));
	test_sorted(makeWL(), Pos_f(70.,35.));
	test_sorted(makeWL(), Pos_f(0.,-9999.));
}

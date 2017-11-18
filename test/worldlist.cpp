/* This WorldList implementation is dual-licensed under both the
 * terms of the MIT and the GPL3 license. You can choose which
 * of those licenses you want to use. Note that the MIT option
 * only applies to this file, and not to the rest of
 * factorio-bot (unless stated otherwise).
 */

/*
 * Copyright (c) 2017 Florian Jung
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
 * Copyright (c) 2017 Florian Jung
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

#include "../worldlist.hpp"
#include "../entity.h"

#include <memory>
#include <iostream>
#include <string>

using namespace std;

typedef WorldList<Entity> WL;


static void show(const WL& l);
static void test_erase(WL l, size_t i);
static void test_around(WL l, Pos_f center);
static void test_around_erase(WL l, Pos_f center, size_t idx);
static WL makeWL();

static void show(const WL& l)
{
	WL::ConstRange r(l.range( Area_f(0,0,100,100) ));
	for (auto& x : r) { cout << "\t" << x.pos.str() << endl; }
}


static void test_erase(WL l, size_t i)
{
	cout << "erasing element #" << i << ": ";

	WL::Range r = l.range( Area_f(0,0,100,100) );

	WL::Range::iterator it = r.begin();
	advance(it, i);

	cout << "elem = " << it->pos.str();

	it = l.erase(it);

	if (it == r.end())
		cout << ", next = (end)" << endl;
	else
		cout << ", next = " << it->pos.str() << endl;

	show(l);
	cout << endl;
}

static void test_around(WL l, Pos_f center)
{
	WL::Around s = l.around(center);

	int i=0;
	cout << "sorting around " << center.str() << endl;
	for (WL::Around::iterator it = s.begin(); it != s.end(); it++, i++)
		cout << "\t" << it->pos.str() << "\t(dist = " << (it->pos-center).len() << ")" << endl;
	
	cout << "\tthat's " << i << " objects" << endl;
}

static void test_around_erase(WL l, Pos_f center, size_t idx)
{
	WL::Around s = l.around(center);

	int i=0;
	cout << "erasing "<<idx<<"-closest item to " << center.str() << endl;
	
	WL::Around::iterator iter_erase = s.begin();
	advance(iter_erase, idx);
	l.erase(iter_erase);
	
	for (WL::Around::iterator it = s.begin(); it != s.end(); it++, i++)
		cout << "\t" << it->pos.str() << "\t(dist = " << (it->pos-center).len() << ")" << endl;
	
	cout << "\tthat's " << i << " objects" << endl;
}

static WL makeWL()
{
	WL l;
	l.insert(Entity(Pos_f(2.5,80.2), nullptr));
	l.insert(Entity(Pos_f(0.4,0.2), nullptr));
	l.insert(Entity(Pos_f(35.4,1.5), nullptr));
	l.insert(Entity(Pos_f(99.4,1.5), nullptr));
	l.insert(Entity(Pos_f(99.5,1.5), nullptr));
	l.insert(Entity(Pos_f(100.6,1.5), nullptr));
	l.insert(Entity(Pos_f(99.7,1.5), nullptr));
	l.insert(Entity(Pos_f(100.8,1.5), nullptr));
	l.insert(Entity(Pos_f(99.4,41.5), nullptr));
	l.insert(Entity(Pos_f(99.5,41.5), nullptr));
	l.insert(Entity(Pos_f(100.6,41.5), nullptr));
	l.insert(Entity(Pos_f(99.7,41.5), nullptr));
	l.insert(Entity(Pos_f(99.8,41.5), nullptr));
	l.insert(Entity(Pos_f(2.5,5.2), nullptr));
	l.insert(Entity(Pos_f(-4.2,4), nullptr));
	l.insert(Entity(Pos_f(2.5,101), nullptr));
	l.insert(Entity(Pos_f(101,1.0), nullptr));
	return l;
}

int main()
{
	cout << "original" << endl;
	show(makeWL());
	cout << endl;

	for (size_t i=0; i<11; i++)
		test_erase(makeWL(), i);

	test_around(makeWL(), Pos_f(0.,0.));
	test_around(makeWL(), Pos_f(70.,35.));
	test_around(makeWL(), Pos_f(0.,-9999.));

	test_around_erase(makeWL(), Pos_f(0.,0.), 2);
}

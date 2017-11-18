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

#include <string>
#include <iostream>

#include "rcon.h"

using namespace std;

int main(int argc, char** argv)
{
	if (argc < 5)
	{
		cout << "Usage: " << argv[0] << " 'host' 'port' 'password' 'command'" << endl;
		return 1;
	}

	Rcon rcon(argv[1], atoi(argv[2]), argv[3]);
	cout << "result: " << rcon.sendrecv(argv[4]).data << endl;
}

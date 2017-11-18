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

#include <stdint.h>
#include <string>

class Rcon
{
	private:
		int sockfd = -1;
		uint32_t curr_id = 0;
	
	public:
		static const int MAXLEN = 4096;
		
		Rcon() {}
		Rcon(std::string host, int port) { connect(host, port); }
		Rcon(std::string host, int port, std::string password) { connect(host, port, password); }

		bool connected() { return sockfd!=-1; }
		
		enum pkgtype
		{
			AUTH = 3,
			AUTH_RESPONSE = 2,
			EXECCOMMAND = 2,
			RESPONSE_VALUE = 0
		};

		struct Packet
		{
			uint32_t id;
			pkgtype type;
			std::string data;
			
			Packet(uint32_t id_, pkgtype type_, const std::string& data_) : id(id_), type(type_), data(data_) {}
			Packet() {}

			void dump();
		};

		void connect(std::string host, int port);
		void connect(std::string host, int port, std::string password);

		void send(uint32_t id, pkgtype type, const std::string& data);
		Packet recv();
		Packet sendrecv(const std::string& data);
};

#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/types.h>
#include <stdexcept>
#include <system_error>
#include <string>
#include <unistd.h>
#include <string.h>
#include <iostream>

#include "rcon.h"


static int connect_socket(const char* address, int port)
{
	struct addrinfo hints = { 0 };
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	struct addrinfo *result, *rp;

	std::string portstr = std::to_string(port);

	int sock;
	int s = getaddrinfo(address, portstr.c_str(), &hints, &result);
	if (s)
		throw std::runtime_error("getaddrinfo() failed");
	
	for (rp = result; rp; rp=rp->ai_next)
	{
		sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock == -1)
			continue;

		if (connect(sock, rp->ai_addr, rp->ai_addrlen) == -1)
		{
			close(sock);
			continue;
		}
		else
		{
			return sock;
		}
	}

	throw std::runtime_error("could not establish a connection");
}

static void serialize_uint32(uint8_t* buf, uint32_t num)
{
	buf[0] = (num      ) & 0xFF;
	buf[1] = (num >>  8) & 0xFF;
	buf[2] = (num >> 16) & 0xFF;
	buf[3] = (num >> 24) & 0xFF;
}

static uint32_t deserialize_uint32(const uint8_t* buf)
{
	return uint32_t(buf[0]) +
		(uint32_t(buf[1]) <<  8) +
		(uint32_t(buf[2]) << 16) +
		(uint32_t(buf[3]) << 24);
}

static void recvn(int sockfd, uint8_t* buf, size_t len)
{
	size_t i = 0;
	ssize_t n;
	while (i < len)
	{
		n = ::recv(sockfd, buf+i, len-i, 0);
		if (n < 0)
			throw std::system_error(errno, std::generic_category(), "recv failed");
		else if (n == 0)
			throw std::runtime_error("remote closed the connection prematurely");
		else
			i+=n;
	}
}

void Rcon::connect(std::string host, int port)
{
	sockfd = connect_socket(host.c_str(), port);
}

void Rcon::connect(std::string host, int port, std::string password)
{
	sockfd = connect_socket(host.c_str(), port);

	send(0xDEADBEEF, AUTH, password.c_str());
	Packet reply = recv();

	if (reply.id != 0xDEADBEEF || reply.type != AUTH_RESPONSE)
		throw std::runtime_error("unable to authenticate");
}

void Rcon::Packet::dump()
{
	std::cout << "id=" << id << ", type=" << type << ", data=" << data << ", datalen=" << data.length() << std::endl;
}

void Rcon::send(uint32_t id, pkgtype type, const std::string& data)
{
	if (data.length() > MAXLEN - 10 || data.length() < 1)
		throw std::invalid_argument("illegal packet length in Rcon::send");
	
	uint8_t buf[4+MAXLEN];
	uint32_t len = static_cast<uint32_t>(data.length());

	serialize_uint32(buf  , 10+len);
	serialize_uint32(buf+4, id);
	serialize_uint32(buf+8, type);
	memcpy(buf+12, data.c_str(), len);
	buf[12+len] = 0;
	buf[13+len] = 0;

	if (-1 == ::send(sockfd, buf, 14+len, 0))
		throw std::system_error(errno, std::generic_category(), "send failed");
}

Rcon::Packet Rcon::recv() {
	uint8_t lenbuf[4];
	Packet pkg;
	
	recvn(sockfd, lenbuf, 4);

	size_t len = deserialize_uint32(lenbuf);
	if (len > MAXLEN)
		throw std::runtime_error("received packet of invalid length");

	uint8_t pkgbuf[MAXLEN];
	recvn(sockfd, pkgbuf, len);

	pkg.id = deserialize_uint32(pkgbuf + 0);
	pkg.type = static_cast<pkgtype>(deserialize_uint32(pkgbuf + 4));
	pkg.data = std::string(reinterpret_cast<char*>(pkgbuf+8), len-10);

	pkg.dump();

	return pkg;
}

Rcon::Packet Rcon::sendrecv(const std::string& data)
{
	send(++curr_id, EXECCOMMAND, data);
	Packet pkg = recv();
	if (pkg.id != curr_id)
		throw std::runtime_error("sendrecv() received invalid ID");
	return pkg;
}

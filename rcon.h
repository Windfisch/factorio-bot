#include <stdint.h>
#include <string>

class Rcon
{
	private:
		int sockfd;
		uint32_t curr_id = 0;
	
	public:
		static const int MAXLEN = 4096;
		
		Rcon(std::string host, int port);
		Rcon(std::string host, int port, std::string password);
		
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

		void send(uint32_t id, pkgtype type, const std::string& data);
		Packet recv();
		Packet sendrecv(const std::string& data);
};

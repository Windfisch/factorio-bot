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

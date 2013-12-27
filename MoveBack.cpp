#include "IndiControl.hpp"

int main(int argc, char *argv[])
{
	ScopeClient client(true);
	while(1) {
		//sleep(3600);
		client.MoveBack();
		sleep(3600);
	}

}

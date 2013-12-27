#include "IndiControl.hpp"

int main(int argc, char *argv[])
{
  double ra = atof(argv[1]) + 4.622;
  if (ra > 24) ra -= 24;
  double dec = atof(argv[2]);
  std::cout << ra << " " << dec << std::endl;
	ScopeClient client;

	client.Goto(ra, dec);
	std::vector<double> pos = client.getPos();
	std::cout << pos[0] << " " << pos[1] << std::endl;
	cout << "Press any key to terminate the client.\n";
	string term;
	cin >> term;

}

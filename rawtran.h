#include <string>
int rawtran(char *raw, char *fitsname, char *color,
	    int bitpix, int aborted, char *adds);
int rawtran_wrapper(std::string raw, std::string fitsname, std::string color,
		int bitpix, int aborted, std::string adds="");

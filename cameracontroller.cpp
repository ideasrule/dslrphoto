#include "cameracontroller.hpp"
#include "parameters.hpp"


void print_help(char* progname) {
	std::cout << "Usage: " << progname << " [OPTIONS] exp filepre\n";
	std::cout << "exp: exposure time in seconds, i.e. 12.4\n";
	std::cout << "filepre: output files will be this + .cr2, R.fits, etc\n";
	std::cout << "\t-i iso: ISO of the sensor. Default 1600.\n";
	std::cout << "\t-a aperture: Aperture as a number, e.g. 5.6. \n";
	std::cout << "\t\t0 for lowest possible aperture. Default 0.\n";
	std::cout << "\t-m{O|D}: mode; O for open shutter, D for closed \n";
	std::cout << "\t-t{zero|dark|flat|object}: Image type. Default 'object'.\n";
	std::cout << "\t-o filepre: output files will be this + .cr2, R.fits, etc\n";
	std::cout << "\t-f: focus the camera before beginning the exposure\n";
	std::cout << "\t-N: do not generate FITS files\n";
	std::cout << "\t-h print help\n";
}

int main(int argc, char** argv) {
	int opt;
	std::string iso="1600";
	//0 aperture means lowest possible
	std::string aperture = "0";
	std::string mode = "O"; //open shutter
	std::string type = "object";
	float exptime;
	std::string filepre = "image";
	bool focus = false;
	bool gen_fits = true;

	while ((opt = getopt(argc, argv, "i:a:m:t:o:fhN")) != -1) {
		switch(opt)
		{
		case 'i':
			iso = std::string(optarg);
			break;
		case 'a':
			aperture = std::string(optarg);
			break;
		case 'm':
			mode = std::string(optarg);
			break;
		case 't':
			mode = std::string(optarg);
			break;
		case 'o':
			filepre = std::string(optarg);
			break;
		case 'h':
			print_help(argv[0]);
			return 0;
		case 'f':
			focus = true;
			break;
		case 'N':
			gen_fits = false;
			break;
		case '?':
			return 1;
		default:
			abort();
		}
	}

	if (optind + 1 != argc) {
		//we accept exactly 1 non-option argument
		print_help(argv[0]);
		return -1;
	}
	exptime = atof(argv[argc-1]);
	CameraController c;
	int ret;
	if (focus) c.focus();
	ret = c.capture_to_file(filepre, exptime, iso, aperture, gen_fits);
	if (ret != GP_OK) {
		std::cout << "Error while capturing image" << std::endl;
		return ret;
	}
	return 0;
}

#include "parameters.hpp"
#include "rawtran.h"
#include <errno.h>
#include <string>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gphoto2/gphoto2.h>
#include <iostream>
#include "samples.h"
#include <signal.h>
#include <getopt.h>
#include "fitsio.h"
#include <vector>
#include <algorithm>
#include <math.h>

void handle_sigterm(int sig) {
	std::cout << "Aborting exposure (if started)..." << std::endl;
}

class CameraController {
private:
	Camera* camera;
	GPContext* context;

	static void errordumper(GPLogLevel level, const char *domain, const char *str,
			void *data) {
		struct timeval tv;
		gettimeofday (&tv, NULL);
		fprintf(stdout, "%ld.%ld: %s\n", tv.tv_sec, tv.tv_usec, str);
	}

	void produce_fits(std::string prefix, int aborted) {
		std::string raw = prefix + ".cr2";
		//precede all names by !, to make cfitsio overwrite existing files
		rawtran_wrapper(raw, "!"+prefix+"R.fits", "Ri", FITS_BITS, aborted);
		rawtran_wrapper(raw, "!"+prefix+"G.fits", "Gi", FITS_BITS, aborted);
		rawtran_wrapper(raw, "!"+prefix+"B.fits", "Bi", FITS_BITS, aborted);
	}

	int focus_quality(std::string filename) {
		fitsfile *fptr;   /* FITS file pointer, defined in fitsio.h */
		int status = 0;   /* CFITSIO status value MUST be initialized to zero! */
		int bitpix, naxis, ii, anynul;
		long naxes[2] = {1,1}, fpixel[2] = {1,1};
		double *pixels;
		char format[20], hdformat[20];

		std::vector<double> values; //stores the values of all pixels in image

		if (!fits_open_file(&fptr, filename.c_str(), READONLY, &status))
		{
			if (!fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status) )
			{
				if (naxis > 2 || naxis == 0)
					printf("Error: only 1D or 2D images are supported\n");
				else
				{
					/* get memory for 1 row */
					pixels = (double *) malloc(naxes[0] * sizeof(double));

					if (pixels == NULL) {
						printf("Memory allocation error\n");
						return(1);
					}

					if (bitpix > 0) {  /* set the default output format string */
						strcpy(hdformat, " %7d");
						strcpy(format,   " %7.0f");
					} else {
						strcpy(hdformat, " %15d");
						strcpy(format,   " %15.5f");
					}

					/* loop over all the rows in the image, top to bottom */
					for (fpixel[1] = naxes[1]; fpixel[1] >= 1; fpixel[1]--)
					{
						if (fits_read_pix(fptr, TDOUBLE, fpixel, naxes[0], NULL,
								pixels, NULL, &status) )  /* read row of pixels */
							break;  /* jump out of loop on error */
						for (ii = 0; ii < naxes[0]; ii++) {
							values.push_back(pixels[ii]);
						}
					}
					free(pixels);
				}
			}
			fits_close_file(fptr, &status);
		}

		if (status) fits_report_error(stderr, status); /* print any error message */

		std::sort(values.begin(), values.end(), std::greater<double>());
		return values[FOCUS_INDEX];
	}

public:
	CameraController() {
		int	retval;
		context = sample_create_context();

		gp_log_add_func(GP_LOG_ERROR, errordumper, NULL);
		gp_camera_new(&camera);

		printf("Camera init.  Takes a few seconds.\n");
		signal(SIGTERM, handle_sigterm);
		retval = gp_camera_init(camera, context);
		if (retval != GP_OK) {
			printf("  Retval: %d\n", retval);
			exit (1);
		}

	}

	int capture_to_file(std::string file_prefix, float exposure,
			std::string iso="1600", std::string aperture="0") {
		int aborted = 0;
		int fd, retval;
		CameraFile *canonfile;
		CameraFilePath camera_file_path;
		set_config_value_string(camera, "iso", iso.c_str(), context);
		if (aperture=="0")
			set_config_value_index(camera, "aperture", 0, context);
		else
			set_config_value_string(camera, "aperture", aperture.c_str(), context);

		printf("Capturing.\n");
		//press shutter, wait for "exposure" seconds, release shutter
		CameraEventType eventtype;
		void* eventdata;
		set_config_value_string(camera, "eosremoterelease", "Press Full", context);
		usleep(exposure*1e6);
		if (errno == EINTR) aborted = 1;
		set_config_value_string(camera, "eosremoterelease", "Release Full", context);

		std::cout << "Waiting for file...\n";
		eventtype = GP_EVENT_UNKNOWN;
		while (eventtype != GP_EVENT_FILE_ADDED) {
			gp_camera_wait_for_event(camera, TIMEOUT, &eventtype, &eventdata, context);
		}
		camera_file_path = *((CameraFilePath*) eventdata);

		printf("Pathname on the camera: %s/%s\n", camera_file_path.folder,
				camera_file_path.name);

		fd = open((file_prefix+".cr2").c_str(),
				O_CREAT | O_WRONLY, 0644);
		retval = gp_file_new_from_fd(&canonfile, fd);
		if (retval != GP_OK) return retval;

		retval = gp_camera_file_get(camera, camera_file_path.folder,
				camera_file_path.name, GP_FILE_TYPE_NORMAL, canonfile,
				context);
		if (retval != GP_OK) return retval;

		retval = gp_camera_file_delete(camera, camera_file_path.folder,
				camera_file_path.name, context);
		if (retval != GP_OK) return retval;

		gp_file_free(canonfile);
		produce_fits(file_prefix, aborted);
		return GP_OK;
	}

	void focus() {
		std::cout << "Focusing\n";
		//first, focus to farthest possible
		set_config_value_string(camera, "output", "PC", context);
		for (int i=0; i < MAX_FOCUS_STEPS; i++)
			set_config_value_string(camera, "manualfocusdrive", "Far 3", context);

		int last_quality = -1;
		int quality = -1;
		char c='A';
		while(1) {
			capture_to_file(FOCUS_FILE+c, FOCUS_EXPTIME, FOCUS_ISO);
			quality = focus_quality(FOCUS_FILE +c+ "G.fits");
			std::cout << quality << std::endl;
			if (quality < 0.95*last_quality) break;
			set_config_value_string(camera, "manualfocusdrive", "Near 2", context);
			last_quality = quality;
			c++;
		}
		//we actually moved past focus by one step, so go back
		set_config_value_string(camera, "manualfocusdrive", "Far 2", context);
		capture_to_file("final_focus", FOCUS_EXPTIME, FOCUS_ISO);
		std::cout << focus_quality("final_focusG.fits") << std::endl;
		set_config_value_string(camera, "output", "TFT", context);
		
		std::cout << "Focused!\n";
		return;
	}

	~CameraController() {
		gp_camera_exit(camera, context);
	}
};


#include "parameters.hpp"
#include "cameracontroller.hpp"
#include <sstream>

double get_average(std::string filename) {
	fitsfile *fptr;   /* FITS file pointer, defined in fitsio.h */
	int status = 0;   /* CFITSIO status value MUST be initialized to zero! */
	int bitpix, naxis, ii;
	long naxes[2] = {1,1}, fpixel[2] = {1,1};
	double *pixels;

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

	//report average of values
	double sum = 0;
	for (int i=0; i < values.size(); i++)
		sum += values[i];
	return sum/values.size();
}
int main(int argc, char** argv) {
	int nflats = 3;
	std::stringstream ss1;
	srand(time(NULL));
	ss1 << "flat" << rand()%1000 << "_";
	std::string flatTestName = ss1.str() + "test";
	std::string flatName = ss1.str();
	
	double exptime = 0.5;
	std::string iso = "200";
	double satVal = 16384;
	//	double satVal=3000;
	double thresVal = satVal/3;
	int ret;

	CameraController c;
	//take a flat with minimal exposure time, double exposure until threshold exceeded
	while(1) {
		ret = c.capture_to_file(flatTestName, exptime, iso);
		if (ret != GP_OK) {
			std::cout << "Error while capturing image" << std::endl;
			return ret;
		}
		double rval = get_average(flatTestName+"R.fits");
		double gval = get_average(flatTestName+"G.fits");
		double bval = get_average(flatTestName+"B.fits");
		std::cout << rval << " " << gval << " " << bval << std::endl;
		if (rval > thresVal && gval > thresVal && bval > thresVal) break;
		exptime *= 2;
	}
	//we've found a good value, so take flats
	for (int i=0; i < nflats; i++) {
		std::stringstream ss;
		ss << flatName << i;
		ret = c.capture_to_file(ss.str(), exptime, iso);
		if (ret != GP_OK) {
			std::cout << "Error while capturing image" << std::endl;
			return ret;
		}
	}
	return 0;
}

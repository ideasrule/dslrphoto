/*
Most source code copied from rawtran
 */
#include "parameters.hpp"
#include "rawtran.h"
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <stdlib.h>
#include <fitsio.h>

#include <assert.h>

typedef struct struct_pixmap {
	int width, height;
	int maxval;
	int colors;
	unsigned short int *data;
	/* Data contains one or tree color bands. The bands are stored in
     a plain one-dimensional array. Single bands are stored sequently
     i order: R G B (X Y Z). Any sub-array has width*height elements.
     Star positions of every band is on width*height*(order) place.
     An origin is left bottom corner (as usuall in FITS). 
	 */
} PIXMAP;

/* begin of decode of PGM (gray) or PPM (color) pixmap file */
/* http://netpbm.sourceforge.net/doc/pgm.html */
/* http://netpbm.sourceforge.net/doc/ppm.html */

int head_num(FILE *f)
{
	const int len = FLEN_CARD;
	int i,c,cycle,anum;
	char l[len+1];

	/* skip blanks */
	while( (c = fgetc(f)) != EOF && isspace(c) );
	if( ferror(f) || feof(f) )
		return(-1);

	/* width, height or maxval */
	i = 0;
	cycle= 1;
	while (cycle) {
		l[i] = (unsigned char) c;
		i++;
		l[i] = '\0';
		cycle = (c = fgetc(f)) != EOF && isdigit(c) && i < len;
	}
	if( ferror(f) || feof(f) )
		return(-1);

	if( sscanf(l,"%d",&anum) != 1 )
		return(-1);

	return(anum);
}


PIXMAP *pixmap(FILE *f)
{
	PIXMAP *p;
	int i,j,k,m,lbytes,esize,width,height,maxval,colors,npixels,step;
	const int mlen = 2;
	char magic[mlen+1];
	unsigned char *data;
	unsigned short int *pdata;

	/* magic number, recognize pgm and ppm */
	magic[0] = '\0'; magic[1] = '\0'; magic[2] = '\0';
	if( !(fread(magic,mlen,1,f) == 2 ) ) {
		if( strcmp(magic,"P5") == 0 )
			colors = 1;
		else if ( strcmp(magic,"P6") == 0 )
			colors = 3;
		else
			return(NULL);
	}

	/* width, height, max value */
	width = head_num(f);
	height = head_num(f);
	maxval = head_num(f);
	/*  fprintf(stderr,"%d %d %d\n",width,height,maxval);*/
	if( width <= 0 || height <= 0 || !(0 < maxval && maxval < 65536) ) {
		return(NULL);
	}

	/* PGM structure */
	p = (PIXMAP*) malloc(sizeof(PIXMAP));
	p->width = width;
	p->height = height;
	p->maxval = maxval;
	p->colors = colors;
	if( (p->data = (unsigned short*)
			malloc(width*height*colors*sizeof(short))) == NULL ) {
		return(NULL);
	}
	pdata = p->data;

	/* identify data */
	if( maxval < 256 )
		esize = 1;
	else
		esize = 2;

	/* load image data */
	lbytes = width*esize*colors;
	npixels = width*height;
	step = esize*colors;
	data = (unsigned char*) malloc(lbytes);
	for( i = 0; i < height; i++) {

		if( fread(data,1,lbytes,f) != lbytes ) {
			free(p->data);
			free(p);
			return(NULL);
		}

		k = width*(height - i - 1);

		if( esize == 1 ) {

			for( j = 0; j < lbytes; j=j+step, k++ )
				for( m = 0; m < colors; m++)
					pdata[k+m*npixels] = data[j+(colors - 1 - m)];

		}
		else {

			unsigned char *d = data;
			unsigned short int *p = pdata + k - 1;
			for( j = 0; j < width; j++ ) {
				p++;
				for( m = (colors-1)*npixels; m >= 0; m -= npixels){
					p[m] = 256 * *d++;
					p[m] += *d++;
				}
			}
			k += width;
		}

	}
	free(data);
	return(p);
}

void pixmap_free(PIXMAP *p)
{
	free(p->data);
	free(p);
	p = NULL;
}
/* end of decode of PIXMAP file */


inline unsigned short array(int xdim,int ydim, unsigned short *pole, int i, int j)
{
	assert((0 <= i && i < xdim) && (0 <= j && j < ydim));
	return pole[j*xdim + i];
}

inline void matmul(float cmatrix[][3], float x[], float y[])
{
	int i,j;
	float s;

	for(i = 0; i < 3; i++) {
		s = 0.0;
		for(j = 0; j < 3; j++)
			s += cmatrix[i][j]*x[j];
		y[i] = s;
	}
}


int rawtran(const char *raw, const char *fitsname, const char *color,
		int bitpix, int aborted, const char *adds)
{
	fitsfile *file;
	int status, npixels, iso, naxis;
	long naxes[3];
	float *flux,*f;
	char *filter = NULL;
	int i,j,k,m,jj,width,height,nexif,idx,ix,jx;
	float maxval,minval,g1,g2,x,y,exp,focus=-1.0;
	unsigned short int *data,*d;
	const int maxexif = 21;
	const int nline = FLEN_CARD;
	char exif[maxexif][nline+1], date[nline+1], time[nline+1],
		stime[nline+1], etime[nline+1], camera[nline+1];
	char line[nline+1], aperture[nline+1];
	const char *ver = "Created by rawtran: http://integral.physics.muni.cz/rawtran";

	/*  matrix for XYZ to BVR transformation, D65 */
	float cmatrix[][3] = {{ 0.200308,  -0.017427,   0.012508},
			{ 0.013093,   0.789019,  -0.328110},
			{-0.224010,  -0.465060,   1.381969}};
	/* "matrix" from XYZ D65 to scotopic curve */
	float scovec[3] = { 0.36169, 1.18214, -0.80498 };
	float bvr[3],zyx[3];

	PIXMAP *p;
	FILE *dcraw;
	struct tm tm;

	const char *dcraw_info_command = "dcraw -i -v -c ";
	const char *dcraw_convert_command;
	char *com;

	dcraw_convert_command = "./dcraw -4 -D -c ";

	/* run dcraw to get PIXMAP */
	com = (char*) malloc(
			strlen(dcraw_convert_command)+strlen(adds)+strlen(raw)+2);
	strcpy(com,dcraw_convert_command);
	strcat(com,adds);
	strcat(com," ");
	strcat(com,raw);
	if( (dcraw = popen(com,"r")) == NULL ) {
		fprintf(stderr,"Failed to invoke: `%s'\n",dcraw_convert_command);
		free(com);
		return(1);
	}

	if( (p = pixmap(dcraw)) == NULL ) {
		pclose(dcraw);
		free(com);
		fprintf(stderr,"Failed to import data by `%s'.\n",dcraw_convert_command);
		fprintf(stderr,"Please, check that dcraw can decode of file `%s'.\n",raw);
		return(1);
	}
	pclose(dcraw);
	free(com);

	/* run dcraw to get EXIF info */
	com = (char*) malloc(strlen(dcraw_info_command)+strlen(raw)+1);
	strcpy(com,dcraw_info_command);
	strcat(com,raw);
	if( (dcraw = popen(com,"r")) == NULL ) {
		fprintf(stderr,"Failed to invoke: `%s'\n",dcraw_info_command);
		free(com);
		return(1);
	}
	for( nexif = 0, i = 0; i < maxexif && fgets(exif[i],nline,dcraw); i++) {
		for( j = 0; exif[i][j] != '\n'; j++);
		exif[i][j] = '\0';
	}
	nexif = i;
	pclose(dcraw);
	free(com);

	/* decode exif data */
	for( i = 0; i < nexif; i++) {
		/* time of start of exposure ? */
		if( strstr(exif[i],"Timestamp:") ) {
			strncpy(stime,exif[i]+strlen("Timestamp: "),nline);
			if( strptime(stime,"%a %b %d %T %Y",&tm) != NULL ) {
				strftime(date, sizeof(date), "%Y-%m-%d", &tm);
				strftime(time, sizeof(time), "%T", &tm);
			}
			else {
				strcpy(stime,"");
				strcpy(date,"");
				strcpy(time,"");
			}
		}

		/* exposure time */
		if( strstr(exif[i],"Shutter:") ) {
			strncpy(etime,exif[i]+strlen("Shutter: "),nline);
			if( strstr(etime,"/") ){
				sscanf(etime,"%f/%f",&x,&y);
				exp = x/y;
			}
			else
				sscanf(etime,"%f",&exp);
		}

		/* camera */
		if( strstr(exif[i],"Camera:") )
			strncpy(camera,exif[i]+strlen("Camera: "),nline);

		/* ISO speed */
		if( strstr(exif[i],"ISO speed:") ) {
			strncpy(line,exif[i]+strlen("ISO speed:"),nline);
			sscanf(line,"%d",&iso);
		}

		/* aperture */
		if( strstr(exif[i],"Aperture:") )
			strncpy(aperture,exif[i]+strlen("Aperture: "),nline);

		/* focus */
		if( strstr(exif[i],"Focal length:") ) {
			strncpy(line,exif[i]+strlen("Focal length:"),nline);
			if( sscanf(line,"%f",&focus) != 1 )
				focus = -1.0;
		}

	}



	if( strcmp(color,"Ri") == 0 || strcmp(color,"Gi") == 0 ||
			strcmp(color,"Bi") == 0 || strcmp(color,"Gi1") == 0 ||
			strcmp(color,"Gi2") == 0 ) {

		width = p->width/2;
		height = p->height/2;

		naxis = 2;
		naxes[0] = width;
		naxes[1] = height;
		flux = (float*) malloc(width*height*sizeof(float));
		npixels = width*height;

		if( strcmp(color,"Gi") == 0 ) {

			k = 0;
			for(j = 0; j < 2*height; j = j + 2 )
				for(i = 0; i < 2*width; i = i + 2 ) {
					g1 = array(p->width,p->height,p->data,i+1,j+1);
					g2 = array(p->width,p->height,p->data,i,j);
					flux[k++] = (g1 + g2)/2.0;
				}
			filter = "G_instr";
		}
		else {

			ix = 0; jx = 0; filter = "R_instr";

			if( strcmp(color,"Ri") == 0 ) { ix = 0; jx = 1; filter = "R_instr"; }
			if( strcmp(color,"Bi") == 0 ) { ix = 1; jx = 0; filter = "B_instr"; }
			if( strcmp(color,"Gi1") == 0 ){ ix = 1; jx = 1; filter = "G1_instr"; }
			if( strcmp(color,"Gi2") == 0 ){ ix = 0; jx = 0; filter = "G2_instr"; }

			k = 0;
			for(j = 0; j < 2*height; j = j + 2)
				for(i = 0; i < 2*width; i = i + 2)
					flux[k++] = array(p->width,p->height,p->data,i+ix,j+jx);
		}
	}

	else {
		pixmap_free(p);
		fprintf(stderr,"Unrecognized filter. Choices: Ri,Gi1,Gi2,Gi,Bi.\n");
		return(1);
	}



	pixmap_free(p);

	/* output to FITS file */
	npixels = 1;
	for(k = 0; k < naxis; k++)
		npixels = npixels*naxes[k];


	/* cutoff values to a specified range */
	if( bitpix > 0 ) {
		maxval = pow(2.0,1.0*bitpix) - 1.0 - DBL_EPSILON;
		minval = DBL_EPSILON;
		for(i = 0; i < npixels; i++) {
			if( flux[i] > maxval )
				flux[i] = maxval;
			if( flux[i] < minval )
				flux[i] = minval;
		}
	}

	/* select output data representation */
	switch(bitpix) {
	case 8:   bitpix = BYTE_IMG;  break;
	case 16:  bitpix = USHORT_IMG; break;
	case 32:  bitpix = ULONG_IMG;  break;
	case -32: bitpix = FLOAT_IMG;  break;
	default:  bitpix = USHORT_IMG; break;
	}

	status = 0;
	fits_create_file(&file, fitsname, &status);

	fits_create_img(file, bitpix, naxis, naxes, &status);


	if( filter != NULL )
		fits_update_key(file,TSTRING,"FILTER",filter,
				"Spectral filter or colorspace component",&status);

	fits_update_key_str(file, "TIMESYS", "UTC", "Time system in UTC",
				&status);
	fits_update_key(file,TSTRING,"DATE-OBS",date,
			"date of start of observation", &status);
	fits_update_key(file,TSTRING,"TIME-OBS",time,
			"time of start of observation",&status);

	fits_update_key_fixflt(file,"EXPTIME",exp,5,"[s] Exposure time",&status);
	fits_update_key_lng(file, "ABORTED",aborted,"Was exposure aborted?",&status);
	fits_update_key_lng(file,"CMBITDEP",CAMERA_BITDEPTH,"Camera bitdepth",&status);
	fits_update_key_lng(file,"CMX",CMX,"Camera X direction pixelnum",&status);
	fits_update_key_lng(file,"CMY",CMY,"Camera Y direction pixelnum",&status);
	fits_update_key_lng(file,"CMY",CMY,"Camera Y direction pixelnum",&status);
	fits_update_key_fixflt(file,"CMXSIZE",CMXSIZE,2,"Camera X pixelsize [mu]",&status);
	fits_update_key_fixflt(file,"CMYSIZE",CMYSIZE,2,"Camera Y pixelsize [mu]",&status);

	fits_update_key(file,TSHORT,"ISO",&iso,"ISO speed",&status);
	fits_update_key(file,TSTRING,"INSTRUME",camera,
			"Camera manufacturer and model",&status);
	fits_update_key(file,TSTRING,"APERTURE",aperture,"Aperture",&status);
	if( focus > 0.0 )
		fits_update_key_fixflt(file,"FOCUS",focus,1,"[mm] Focal length",&status);
	fits_update_key(file,TSTRING,"CREATOR",(void*) "rawtran ",
			"Created by rawtran $Date: 2012-04-23 19:23:48 $",&status);
	fits_write_comment(file,ver,&status);

	fits_write_comment(file,"EXIF data info - begin",&status);
	for( i = 0; i < nexif; i++)
		fits_write_comment(file,exif[i],&status);
	fits_write_comment(file,"EXIF data info - end",&status);

	fits_write_img(file, TFLOAT, 1, npixels, flux, &status);

	fits_close_file(file, &status);

	fits_report_error(stderr,status);

	free(flux);

	return(status);
}

int rawtran_wrapper(std::string raw, std::string fitsname, std::string color,
		int bitpix, int aborted, std::string adds) {
	return rawtran(raw.c_str(), fitsname.c_str(), color.c_str(),
			bitpix, aborted, adds.c_str());
}


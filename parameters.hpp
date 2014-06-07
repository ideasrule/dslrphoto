#pragma once
#include <string>

const int FITS_BITS = 16;
const int TIMEOUT = 5000; //milliseconds to wait for camera events

//camera X, Y direction pixelnums
const int CMX = 5184;
const int CMY = 3456;

const double CMXSIZE = 2.16;
const double CMYSIZE = 2.16;
const int CAMERA_BITDEPTH = 14;

const int FOCUS_EXPTIME = 5;
//use the nth brightest pixel for autofocus
const int FOCUS_INDEX = 100;
const std::string FOCUS_ISO = "1600";
//where to save images taken during focusing
const std::string FOCUS_FILE = "tmp_focus";
//number of steps from nearest to farthest focus
const int MAX_FOCUS_STEPS = 20;

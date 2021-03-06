#include <ciolib.h>

#include "key.h"
#include "miscfunctions.h"

#define HelpLines 52

static unsigned char   helpansi[3360] = {
	0, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 32, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 32,
	1, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 220, 1, 32, 1, 220, 1, 220,
	1, 220, 1, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 1, 220, 1, 220, 1, 220,
	1, 220, 1, 220, 1, 220, 1, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 220, 1, 223, 1, 32, 1, 220, 8, 220, 8, 32, 8, 32, 8, 220, 8, 220, 8, 32,
	8, 223, 1, 32, 1, 220, 8, 32, 8, 220, 8, 220, 8, 220, 8, 32, 8, 223, 1, 32,
	1, 220, 8, 32, 8, 223, 1, 220, 1, 32, 1, 220, 1, 223, 1, 32, 1, 220, 8, 32,
	8, 220, 8, 220, 8, 220, 8, 32, 8, 223, 1, 220, 1, 0, 7, 0, 7, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 1, 223,
	1, 32, 1, 220, 8, 220, 7, 223, 7, 223, 7, 32, 7, 32, 7, 223, 7, 223, 7, 220,
	7, 220, 8, 223, 8, 223, 7, 219, 7, 223, 7, 223, 7, 223, 7, 223, 8, 32, 8, 223,
	8, 223, 7, 219, 7, 220, 8, 32, 8, 223, 1, 220, 1, 32, 1, 223, 8, 223, 7, 220,
	7, 223, 7, 223, 7, 223, 7, 220, 7, 220, 8, 32, 8, 219, 1, 0, 7, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 223, 8, 32, 8, 220, 8, 32, 8, 219, 15, 32, 15, 223, 8, 32, 8, 32, 8, 220,
	8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 1, 32,
	1, 219, 8, 219, 7, 219, 8, 223, 15, 223, 15, 223, 15, 223, 15, 223, 15, 223, 15, 219,
	8, 219, 7, 219, 8, 32, 8, 219, 8, 219, 7, 223, 15, 223, 15, 223, 15, 223, 15, 223,
	8, 222, 8, 219, 15, 219, 15, 219, 8, 220, 8, 32, 8, 219, 1, 32, 1, 32, 1, 219,
	15, 219, 15, 220, 15, 220, 15, 223, 15, 223, 8, 32, 8, 219, 1, 0, 7, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 8, 32, 8, 220,
	7, 32, 7, 220, 7, 223, 15, 220, 7, 219, 15, 220, 7, 223, 15, 220, 7, 32, 7, 32,
	7, 220, 8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 223, 1, 220,
	1, 32, 1, 223, 8, 223, 7, 220, 7, 220, 7, 220, 8, 220, 8, 220, 7, 220, 7, 223,
	7, 223, 8, 220, 8, 220, 7, 223, 15, 219, 7, 220, 7, 220, 7, 220, 8, 32, 8, 220,
	8, 220, 7, 219, 7, 223, 7, 223, 7, 223, 7, 220, 7, 220, 8, 32, 8, 220, 7, 219,
	7, 223, 8, 223, 8, 223, 8, 32, 8, 220, 1, 223, 1, 32, 1, 32, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 7, 32,
	7, 223, 15, 220, 7, 220, 15, 223, 127, 220, 127, 223, 127, 220, 15, 220, 7, 223, 15, 32,
	15, 220, 8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 223, 1, 223, 1, 32, 1, 223, 8, 223, 8, 32, 8, 32, 8, 223, 8, 223, 8, 32,
	8, 32, 8, 32, 8, 223, 8, 32, 8, 223, 8, 223, 8, 223, 8, 72, 1, 76, 1, 80,
	1, 223, 8, 32, 8, 32, 8, 223, 1, 32, 1, 223, 8, 32, 8, 32, 8, 223, 8, 32,
	8, 223, 1, 223, 1, 223, 1, 223, 1, 32, 1, 32, 1, 32, 1, 32, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 223, 15, 223, 15, 223, 15, 223, 15, 219,
	15, 32, 15, 219, 15, 219, 7, 219, 15, 219, 15, 219, 127, 32, 127, 219, 15, 223, 15, 219,
	15, 223, 15, 223, 15, 32, 15, 223, 15, 32, 15, 223, 15, 32, 15, 223, 15, 223, 15, 223,
	15, 223, 15, 223, 15, 223, 15, 223, 15, 223, 15, 223, 15, 223, 7, 223, 15, 223, 7, 223,
	7, 223, 7, 223, 7, 223, 7, 223, 7, 223, 7, 223, 7, 223, 7, 223, 7, 223, 7, 223,
	8, 223, 7, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223,
	8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 223, 8, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 8, 32, 8, 223,
	7, 220, 15, 32, 15, 223, 15, 220, 127, 223, 127, 220, 127, 223, 15, 223, 7, 220, 15, 32,
	15, 220, 8, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 8, 32, 8, 32,
	8, 32, 8, 223, 7, 220, 15, 220, 7, 219, 15, 223, 7, 220, 15, 220, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 220, 8, 32, 8, 32, 8, 32, 8, 220, 15, 32, 15, 220, 8, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 32, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 32, 7, 32, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 32, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 220, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 32, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 32, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 32, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 32, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 15, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 32, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 32, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 8, 32, 8, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 0, 7, 32, 7, 0, 7, 0, 7, 0,
	7, 0, 7, 0, 7, 0, 7, 0, 7, 219, 7, 220, 7, 220, 7, 220, 7, 220, 7, 220,
	7, 220, 7, 220, 7, 220, 7, 220, 7, 220, 7, 220, 7, 220, 7, 220, 8, 220, 8, 220,
	7, 220, 7, 220, 7, 220, 8, 220, 7, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220,
	8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 220, 8, 219, 8, 32, 8
	,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

void 
help(void)
{
	struct text_info ti;
	char           *Help[HelpLines + 1] = {
		"EDITING KEYS :",
		" CURSOR KEYS - Move cursor",
		" PGDWN/UP    - Page DOWN/UP",
		" POS 1       - Jump to begin of line",
		" END         - Jump to end of line",
		" TAB         - Go to next tab",
		" ALT+TAB     - Go to last tab",
		" INSERT      - Toggle insert mode ON/OFF",
		" SHIFT+F1-F10- Change chartable 1-10",
		" F1-F10      - Write character",
		" ALT+Y       - Delte a line",
		" ALT+I       - Insert a line",
		" ALT+1       - Delte a row",
		" ALT+2       - Insert a row",
		" PRINTSCREEN - Toggle fullscreen mode ON/OFF",
		"",
		"PALETTE KEYS :",
		" ALT+A       - Change color",
		" ALT+U       - Pick up color",
		" ALT+Z       - BLINK ON/OFF",
		"",
		"SETUP KEYS :",
		" ALT+T       - TAB setup",
		" ALT+W       - Set outline font type",
		" CTRL+ S     - View/edit SAUCE",
		" ALT+M       - Select effect style/colors",
		"",
		"SCREEN KEYS :",
		" ALT+V       - View screen in 320x200",
		" ALT+P       - Select page",
		" ALT+C       - Clear screen",
		" ALT+G       - Global commands",
		"",
		"FONT KEYS :",
		" ALT+E       - Toggle elite mode ON/OFF",
		" ALT+F       - Select font",
		" ALT+N       - Font mode ON/OFF",
		"",
		"BLOCK KEYS :",
		" ALT+B    - Block mode (=Left button)",
		" ALT+R    - UNDO last block operation",
		"",
		"MISCELEANEOUS KEYS :",
		" ALT+L    - Load",
		" ALT+S    - Save",
		" ALT+X    - Exit",
		" ALT+D    - Draw line mode",
		" ALT+-    - Draw mode",
		" ALT+K    - Ascii table",
		" ALT+H    - This screen",
		" ESC      - Enter menue mode(=right buton)",
		"",
		"<EOH>"
	};
	int             x, y = 1, ch, page = 0;
	struct			mouse_event	me;

	gettextinfo(&ti);
	clrscr();
	do {
		if (y != page) {
			puttext(1,1,80,21,helpansi);
			for (x = 0; x <= 12; x++) {
				CoolWrite(23, x+8, Help[x + page]);
			}
		}
		y = page;
		gotoxy(61,22);
		textattr(7);
		putch('[');
		textattr(15);
		cprintf("%03d%%", (page * 100) / (HelpLines - 12));
		textattr(7);
		putch(']');
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			break;
		case CIO_KEY_UP:
			if (page > 0)
				page--;
			break;
		case CIO_KEY_DOWN:
			if (page < (HelpLines - 12))
				page++;
			break;
		case CIO_KEY_PPAGE:
			if (page >= 12)
				page -= 12;
			else
				page = 0;
			break;
		case CIO_KEY_NPAGE:
			if (page < (HelpLines - 24))
				page += 12;
			else
				page = HelpLines - 12;
			break;
		}
	} while ((ch != 13) & (ch != 27));
}

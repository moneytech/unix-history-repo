/* Copyright (c) 1982 Regents of the University of California */

static char sccsid[] = "@(#)SIN.c 1.2 %G%";

#include <math.h>
extern int errno;

double
SIN(value)
	double	value;
{
	double result;

	errno = 0;
	result = sin(value);
	if (errno != 0) {
		ERROR("Cannot compute sin(%e)\n", value);
	}
	return result;
}

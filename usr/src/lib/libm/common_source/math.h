/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 * All recipients should regard themselves as participants in an ongoing
 * research project and hence should feel obligated to report their
 * experiences (good or bad) with these elementary function codes, using
 * the sendbug(8) program, to the authors.
 *
 *	@(#)math.h	5.2 (Berkeley) %G%
 */

extern double asinh(), acosh(), atanh();
extern double erf(), erfc();
extern double exp(), expm1(), log(), log10(), log1p(), pow();
extern double fabs(), floor(), ceil(), rint();
extern double lgamma();
extern double hypot(), cabs();
extern double copysign(), drem(), logb(), scalb();
extern int finite();
#if defined(vax)||defined(tahoe)
extern double infnan();
#endif	/* defined(vax)||defined(tahoe) */
extern double j0(), j1(), jn(), y0(), y1(), yn();
extern double sin(), cos(), tan(), asin(), acos(), atan(), atan2();
extern double sinh(), cosh(), tanh();
extern double cbrt(), sqrt();
extern double modf(), ldexp(), frexp(), atof();

#define HUGE	1.701411733192644270e38

/* Host environment for the tti "Unicom" PBB 68020 boards */

#include "xm-sparc.h"

#define USG
#define bcopy(a,b,c) memcpy (b,a,c)
#define bzero(a,b) memset (a,0,b)
#define bcmp(a,b,c) memcmp (a,b,c)

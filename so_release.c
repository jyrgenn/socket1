#include "globals.h"

char *so_release (void)
{
    static char release[] =
	"Socket-1.2 (1999-08-06 by jnickelsen@acm.org)";
    
    return release ;
}

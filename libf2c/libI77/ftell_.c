#include "config.h"
#include "f2c.h"
#include "fio.h"

 static FILE *
#ifdef KR_headers
unit_chk(Unit, who) integer Unit; char *who;
#else
unit_chk(integer Unit, char *who)
#endif
{
	if (Unit >= MXUNIT || Unit < 0)
		f__fatal(101, who);
	return f__units[Unit].ufd;
	}

 integer
#ifdef KR_headers
G77_ftell_0 (Unit) integer *Unit;
#else
G77_ftell_0 (integer *Unit)
#endif
{
	FILE *f;
	return (f = unit_chk(*Unit, "ftell")) ? ftell(f) : -1L;
	}

 integer
#ifdef KR_headers
G77_fseek_0 (Unit, offset, xwhence) integer *Unit, *offset, *xwhence;
#else
G77_fseek_0 (integer *Unit, integer *offset, integer *xwhence)
#endif
{
	FILE *f;
	int w = (int)*xwhence;
#ifdef SEEK_SET
	static int wohin[3] = { SEEK_SET, SEEK_CUR, SEEK_END };
#endif
	if (w < 0 || w > 2)
		w = 0;
#ifdef SEEK_SET
	w = wohin[w];
#endif
	return	!(f = unit_chk(*Unit, "fseek"))
		|| fseek(f, *offset, w) ? 1 : 0;
	}

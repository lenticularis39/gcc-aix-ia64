static char junk[] = "\n@(#) LIBU77 VERSION 19980709\n";

char __G77_LIBU77_VERSION__[] = "0.5.26 20010819 (release)";

#include <stdio.h>

void
g77__uvers__ ()
{
  fprintf (stderr, "__G77_LIBU77_VERSION__: %s", __G77_LIBU77_VERSION__);
  fputs (junk, stderr);
}

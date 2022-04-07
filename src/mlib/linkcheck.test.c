#include <stdio.h>

extern void *
get_func_addr_1 ();
extern void *
get_func_addr_2 ();

int
main ()
{
   if (get_func_addr_1 () != get_func_addr_2 ()) {
      fputs ("Multiply-defined symbols were not properly merged.", stderr);
      return 1;
   }
}

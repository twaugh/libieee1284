#include <stdio.h>
#include <string.h>

#include <ieee1284.h>

static char *class (char *id)
{
  char *p;
  id += 2;
  p = strstr (id, "CLASS:");
  if (!p)
    p = strstr (id, "class:");
  if (!p)
    p = strstr (id, "CLS:");
  if (!p)
    p = strstr (id, "cls:");

  if (p)
    {
      char *q;
      p = strchr (p, ':') + 1;
      q = strchr (p, ';');
      if (q)
	*q = '\0';
    }

  return p ? p : "(?)";
}

int main ()
{
  int i, j;
  struct parport_list pl;
  ieee1284_find_ports (&pl, NULL, 0);
  printf ("Found %d ports:\n", pl.portc);
  for (i = 0; i < pl.portc; i++)
    {
      char id[500];
      printf ("  %s: ", pl.portv[i]->name);
      if (ieee1284_get_deviceid (pl.portv[i], -1, F1284_FRESH, id, 500) > -1)
	printf ("%s", class (id));
      printf ("\n");
      for (j = 0; j < 4; j++)
	if (ieee1284_get_deviceid (pl.portv[i], j, 0, id, 500) > -1)
	  printf ("    Daisy chain address %d: %s\n", j, class (id));
    }
  ieee1284_free_ports (&pl);
  return 0;
}

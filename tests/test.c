#include <stdio.h>
#include <string.h>

#include <ieee1284.h>

enum devid_field { devid_cls, devid_mfg, devid_mdl };
static char *field (char *id, enum devid_field f)
{
  char *result = "(?)";
  char *p = NULL;
  id += 2;
  switch (f)
    {
    case devid_cls:
      p = strstr (id, "CLASS:");
      if (!p)
	p = strstr (id, "class:");
      if (!p)
	p = strstr (id, "CLS:");
      if (!p)
	p = strstr (id, "cls:");
      break;
    case devid_mfg:
      p = strstr (id, "MANUFACTURER:");
      if (!p)
	p = strstr (id, "manufacturer:");
      if (!p)
	p = strstr (id, "MFG:");
      if (!p)
	p = strstr (id, "mfg:");
      break;
    case devid_mdl:
      p = strstr (id, "MODEL:");
      if (!p)
	p = strstr (id, "model:");
      if (!p)
	p = strstr (id, "MDL:");
      if (!p)
	p = strstr (id, "mdl:");
      break;
    }

  if (p)
    {
      char *q;
      char c;
      p = strchr (p, ':') + 1;
      q = strchr (p, ';');
      if (q)
	{
	  c = *q;
	  *q = '\0';
	}
      result = strdup (p); // leaks, but this is just a test harness
      if (q)
	*q = c;
    }

  return result;
}

static void test_deviceid (struct parport_list *pl)
{
  int i, j;
  printf ("Found %d ports:\n", pl->portc);
  for (i = 0; i < pl->portc; i++)
    {
      char id[500];
      printf ("  %s: ", pl->portv[i]->name);

      if (ieee1284_get_deviceid (pl->portv[i], -1, F1284_FRESH, id, 500) > -1)
	printf ("%s, %s %s", field (id, devid_cls), field (id, devid_mfg),
		field (id, devid_mdl));
      else if (ieee1284_get_deviceid (pl->portv[i], -1, 0, id, 500) > -1)
	printf ("(may be cached) %s, %s %s", field (id, devid_cls),
		field (id, devid_mfg), field (id, devid_mdl));
      printf ("\n");
      for (j = 0; j < 4; j++)
	if (ieee1284_get_deviceid (pl->portv[i], j, 0, id, 500) > -1)
	  printf ("    Daisy chain address %d: (may be cached) %s, %s %s\n", j,
		  field (id, devid_cls), field (id, devid_mfg),
		  field (id, devid_mdl));
    }
  putchar ('\n');
}

static int show_capabilities (unsigned int cap)
{
#define CAP(x)					\
  if (cap & CAP1284_##x)			\
    printf (#x " ");

  CAP(RAW);
  CAP(NIBBLE);
  CAP(BYTE);
  CAP(COMPAT);
  CAP(BECP);
  CAP(ECP);
  CAP(ECPRLE);
  CAP(ECPSWE);
  CAP(EPP);
  CAP(EPPSL);
  CAP(EPPSWE);
  CAP(IRQ);
  CAP(DMA);
  putchar ('\n');
  return 0;
}

void test_open (struct parport_list *pl)
{
  int i;
  for (i = 0; i < pl->portc; i++)
    {
      struct parport *port = pl->portv[i];
      unsigned int cap;
      /* Just try to open the port, then close it. */
      if (ieee1284_open (port, 0, &cap))
        printf ("%s: inaccessible\n", port->name);
      else
	{
	  printf ("%s: %#lx", port->name, port->base_addr);
	  if (port->hibase_addr)
	    printf (" (ECR at %#lx)", port->hibase_addr);
	  printf ("\n  ");
	  show_capabilities (cap);
	  if (cap & CAP1284_IRQ)
	    {
	      int fd = ieee1284_get_irq_fd (port);
	      if (fd < 0)
		printf ("Couldn't get IRQ fd: %d\n", fd);
	      else
		{
		  int r = ieee1284_claim (port);
		  if (r != E1284_OK)
		    printf ("Couldn't claim port: %d\n", r);
		  else
		    r = ieee1284_clear_irq (port, NULL);
		  if (r != E1284_OK)
		    printf ("Couldn't clear IRQ: %d\n", r);
		  ieee1284_release (port);
		}
	    }
	  ieee1284_close (port);
	}
    }
}

int main ()
{
  struct parport_list pl;
  ieee1284_find_ports (&pl, 0);
  test_deviceid (&pl);
  test_open (&pl);
  ieee1284_free_ports (&pl);
  return 0;
}

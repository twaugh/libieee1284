/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2001, 2002, 2003  Tim Waugh <twaugh@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined __unix__ || defined __MINGW32__
#include <dirent.h>
#include <unistd.h>
#endif

#include "config.h"
#include "conf.h"
#include "ieee1284.h"
#include "debug.h"
#include "detect.h"

#ifdef HAVE_CYGWIN_NT
#ifdef __CYGWIN__
#include <w32api/windows.h>
#else /* Not cygwin really */
/* Don't include windows.h if it isn't necessary. That's why this is here and
*  not in the #if defined __MINGW32__ || defined _MSC_VER below. - dbjh */
#include <windows.h>
#endif /* __CYGWIN__ */
#endif /* HAVE_CYGWIN_NT */

#if defined __MINGW32__ || defined _MSC_VER
#define O_NOCTTY 0
#endif

#define MAX_PORTS 20

static int
compare_port (const void *a, const void *b)
{
  const struct parport * const*p1 = a, * const*p2 = b;
  return strcmp ((*p1)->name, (*p2)->name);
}

static void
sort_ports (struct parport_list *list)
{
  qsort (list->portv, list->portc, sizeof (struct parport *), compare_port);
}

static int
add_port (struct parport_list *list, int flags,
	  const char *name, const char *device, const char *udevice,
	  unsigned long base, unsigned long hibase, int interrupt)
{
  struct parport *p;
  struct parport_internal *priv;

  if (list->portc == (MAX_PORTS - 1))
    /* Ridiculous. */
    return E1284_NOMEM;

  p = malloc (sizeof *p);
  if (!p)
    return E1284_NOMEM;
  memset (p, 0, sizeof *p);

  p->name = strdup (name);
  if (!p->name)
    {
      free (p);
      return E1284_NOMEM;
    }

  p->filename = strdup(device);
  if (!p->filename)
    {
      free ((char *) (p->name));
      free (p);
      return E1284_NOMEM;
    }

  p->base_addr = base;
  p->hibase_addr = hibase;

  priv = malloc (sizeof *priv);
  if (!priv)
    {
      free ((char *) (p->name));
      free ((char *) (p->filename));
      free (p);
      return E1284_NOMEM;
    }
  memset (priv, 0, sizeof *priv);

  priv->fn = malloc (sizeof *priv->fn);
  if (!priv->fn)
    {
      free ((char *) (p->name));
      free ((char *) (p->filename));
      free (p);
      free (priv);
      return E1284_NOMEM;
    }
  memset (priv->fn, 0, sizeof *priv->fn);

  p->priv = priv;
  priv->device = (char *) (p->filename);

  if (udevice)
    {
      priv->udevice = strdup (udevice);

      if (!priv->udevice)
	{
	  free ((char *) (p->name));
	  free ((char *) (p->filename));
	  free (p);
	  free (priv->fn);
	  free (priv);
	  return E1284_NOMEM;
	}
    }

  priv->base = base;
  priv->base_hi = 0;
  if (interrupt < -1)
    interrupt = -1;
  priv->interrupt = interrupt;
  priv->fd = -1;
  priv->type = 0;
  priv->opened = 0;
  priv->claimed = 0;
  priv->ref = 1;

  list->portv[list->portc++] = p;
  sort_ports (list);
  return 0;
}

static int
populate_from_parport (struct parport_list *list, int flags)
{
#ifdef _MSC_VER
  return E1284_SYS;
#else
  struct dirent *de;
  DIR *parport = opendir ("/proc/parport");
  if (!parport)
    return E1284_SYS;

  de = readdir (parport);
  while (de)
    {
      if (strcmp (de->d_name, ".") && strcmp (de->d_name, ".."))
	{
	  char device[50];
	  char udevice[50];
	  char hardware[50];
	  unsigned long base = 0, hibase = 0;
	  int interrupt = -1;
	  int fd;

	  /* Device */
	  if (capabilities & PPDEV_CAPABLE)
	    {
	      sprintf (device, "/dev/parport%s", de->d_name);
	      sprintf (udevice, "/dev/parports/%s", de->d_name);
	    }
	  else
	    {
	      if (capabilities & IO_CAPABLE)
		device[0] = '\0';
	      else if (capabilities & DEV_PORT_CAPABLE)
		strcpy (device, "/dev/port");
	    }

	  /* Base and interrupt */
	  sprintf (hardware, "/proc/parport/%s/hardware", de->d_name);
	  fd = open (hardware, O_RDONLY | O_NOCTTY);
	  if (fd >= 0)
	    {
	      char contents[500];
	      ssize_t got = read (fd, contents, sizeof contents - 1);
	      close (fd);
	      if (got > 0)
		{
		  char *p;

		  contents[got - 1] = '\0';
		  p = strstr (contents, "base:");
		  if (p)
		    {
		      base = strtoul (p + strlen("base:"), NULL, 0);
		      /* FIXME: read hibase too */
		    }

		  p = strstr (contents, "irq:");
		  if (p)
		    {
		      interrupt = strtol (p + strlen("irq:"), NULL, 0);
		    }
		}
	    }

	  add_port (list, flags, de->d_name, device, udevice, base, hibase, interrupt);
	}

      de = readdir (parport);
    }

  closedir (parport);
  return 0;
#endif
}

static int
populate_from_sys_dev_parport (struct parport_list *list, int flags)
{
#ifdef _MSC_VER
  return E1284_SYS;
#else
  struct dirent *de;
  DIR *parport = opendir ("/proc/sys/dev/parport");
  if (!parport)
    return E1284_SYS;

  de = readdir (parport);
  while (de)
    {
      if (strcmp (de->d_name, ".") &&
	  strcmp (de->d_name, "..") &&
	  strcmp (de->d_name, "default"))
	{
	  char device[50];
	  char udevice[50];
	  unsigned long base = 0, hibase = 0;
	  int interrupt = -1;
	  size_t len = strlen (de->d_name) - 1;
	  char filename[50];
	  int fd;
	  char *p;

	  while (len > 0 && isdigit (de->d_name[len]))
	    len--;

	  p = de->d_name + len + 1;

	  /* Device */
	  if (*p && capabilities & PPDEV_CAPABLE)
	    {
	      sprintf (device, "/dev/parport%s", p);
	      sprintf (udevice, "/dev/parports/%s", p);
	    }
	  else
	    {
	      if (capabilities & IO_CAPABLE)
		device[0] = '\0';
	      else if (capabilities & DEV_PORT_CAPABLE)
		strcpy (device, "/dev/port");
	      else
		device[0] = '\0';

	      udevice[0] = '\0';
	    }

	  /* Base */
	  sprintf (filename, "/proc/sys/dev/parport/%s/base-addr", de->d_name);
	  fd = open (filename, O_RDONLY | O_NOCTTY);
	  if (fd >= 0)
	    {
	      char contents[20];
	      char *endptr;
	      ssize_t got = read (fd, contents, sizeof contents - 1);
	      close (fd);
	      if (got > 0)
	        {
		  base = strtoul (contents, &endptr, 0);
		  if (contents != endptr)
		    hibase = strtoul (endptr, NULL, 0);
		}
	    }
      
	  /* Interrupt */
	  sprintf (filename, "/proc/sys/dev/parport/%s/irq", de->d_name);
	  fd = open (filename, O_RDONLY | O_NOCTTY);
	  if (fd >= 0)
	    {
	      char contents[20];
	      ssize_t got = read (fd, contents, sizeof contents - 1);
	      close (fd);
	      if (got > 0)
		interrupt = strtol (contents, NULL, 0);
	    }
      
	  add_port (list, flags, de->d_name, device, udevice, base, hibase, interrupt);
	}

      de = readdir (parport);
    }

  closedir (parport);
  return 0;
#endif
}

static int
populate_nt_ports (struct parport_list *list, int flags)
{
#ifdef HAVE_CYGWIN_NT
  char vdmname[] = "\\\\.\\$VDMLPT*";
  HANDLE hf;
  int i;
  
  for (i = 1; i <= 3; i++)
  {
    /* Name is \\.\$VDMLPT[1,2,3] */
    vdmname[11] = i + '0';
    hf = CreateFile(vdmname, GENERIC_READ | GENERIC_WRITE, 
        0, NULL, OPEN_EXISTING, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE) continue;
    CloseHandle(hf);
    /* Friendly name is LPT1, LPT2, etc */
    add_port (list, flags, vdmname+8, vdmname, NULL, 0, 0, -1);
  }

#endif
  return 0;
}


static int
populate_by_guessing (struct parport_list *list, int flags)
{
#if defined(HAVE_LINUX) || defined(HAVE_CYGWIN_9X) || defined(HAVE_OBSD_I386)
  add_port (list, flags, "0x378", "/dev/port", NULL, 0x378, 0, -1);
  add_port (list, flags, "0x278", "/dev/port", NULL, 0x278, 0, -1);
  add_port (list, flags, "0x3bc", "/dev/port", NULL, 0x3bc, 0, -1);
#elif defined(HAVE_FBSD_I386)
  add_port (list, flags, "0x378", "/dev/io", NULL, 0x378, 0, -1);
  add_port (list, flags, "0x278", "/dev/io", NULL, 0x278, 0, -1);
  add_port (list, flags, "0x3bc", "/dev/io", NULL, 0x3bc, 0, -1);
#elif defined(HAVE_SOLARIS)
  add_port (list, flags, "0x378", "/devices/pseudo/iop@0:iop", NULL, 0x378, 0, -1);
  add_port (list, flags, "0x278", "/devices/pseudo/iop@0:iop", NULL, 0x278, 0, -1);
  add_port (list, flags, "0x3bc", "/devices/pseudo/iop@0:iop", NULL, 0x3bc, 0, -1);
#endif
  return 0;
}

/* Find out what ports there are. */
int
ieee1284_find_ports (struct parport_list *list, int flags)
{
  read_config_file ();

  list->portc = 0;
  list->portv = malloc (sizeof(char*) * MAX_PORTS);
  if (!list->portv)
    return E1284_NOMEM;

  detect_environment (0);
  if (capabilities & PROC_SYS_DEV_PARPORT_CAPABLE)
    populate_from_sys_dev_parport (list, flags);
  else if (capabilities & PROC_PARPORT_CAPABLE)
    populate_from_parport (list, flags);
  else if (capabilities & LPT_CAPABLE)
    populate_nt_ports (list, flags);
  else 
    populate_by_guessing (list, flags);

  if (list->portc == 0)
    {
      free (list->portv);
      list->portv = NULL;
    }

  return 0;
}

/* Free up a parport_list structure. */
void
ieee1284_free_ports (struct parport_list *list)
{
  int i;

  for (i = 0; i < list->portc; i++)
    deref_port (list->portv[i]);
  
  if (list->portv)
    free (list->portv);

  list->portv = NULL;
  list->portc = 0;
}

int
deref_port (struct parport *p)
{
  struct parport_internal *priv = p->priv;
  int count = --priv->ref;
  if (!count)
    {
      debugprintf ("Destructor for port '%s'\n", p->name);
      if (priv->fn)
	free (priv->fn);
      if (p->name)
	free ((char *) (p->name));
      if (priv->device)
	free (priv->device);
      if (priv->udevice)
	free (priv->udevice);
      free (priv);
      free (p);
    }
  return count;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

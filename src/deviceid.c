/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 1999-2002  Tim Waugh <twaugh@redhat.com>
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !(defined __MINGW32__ || defined _MSC_VER)
#include <sys/ioctl.h>
#else
#include <io.h> /* read(), open(), close() */
#define O_NOCTTY 0
#endif
#include <sys/stat.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifdef __unix__
#include <unistd.h>
#endif

#include "config.h"
#include "debug.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"

#define ETRYNEXT	100
#define ENODEVID	101

static ssize_t
get_fresh (struct parport *port, int daisy,
	   char *buffer, size_t len)
{
  ssize_t got;
  size_t idlen;

  debugprintf ("==> get_fresh\n");

  if (daisy > -1)
    {
      /* No implementation yet for IEEE 1284.3 devices. */
      debugprintf ("<== E1284_NOTIMPL (IEEE 1284.3)\n");
      return E1284_NOTIMPL;
    }

  ieee1284_terminate (port);
  if (ieee1284_negotiate (port,
			  M1284_NIBBLE | M1284_FLAG_DEVICEID) != E1284_OK)
    {
      debugprintf ("<== E1284_NOTAVAIL (couldn't negotiate)\n");
      return E1284_NOTAVAIL;
    }

  got = ieee1284_nibble_read (port, 0, buffer, 2);
  if (got < 2)
    {
      debugprintf ("<== E1284_NOID (no data)\n");
      return E1284_NOID;
    }

  idlen = buffer[0] * 256 + buffer[1];
  if (idlen >= len - 2)
    idlen = len - 2;
  got += ieee1284_nibble_read (port, 0, buffer + 2, idlen);
  if ((size_t) got < len)
    buffer[got] = '\0';

  ieee1284_terminate (port);
  debugprintf ("<== %d\n", got);
  return got;
}

static ssize_t
get_from_proc_parport (struct parport *port, int daisy,
		       char *buffer, size_t len)
{
  int fd;
  char *name;

  if (strchr (port->name, '/') || port->name[0] == '.')
    /* Hmm, suspicious. */
    return -ETRYNEXT;

  name = malloc (strlen (port->name) + 50);
  if (!name)
    return -ETRYNEXT;

  if (daisy > -1)
    sprintf (name, "/proc/parport/%s/autoprobe%d", port->name, daisy);
  else
    sprintf (name, "/proc/parport/%s/autoprobe", port->name);

  fd = open (name, O_RDONLY | O_NOCTTY);
  free (name);
  if (fd >= 0)
    {
      ssize_t got = read (fd, buffer + 2, len - 2);
      close (fd);
      if (got < 1)
	return -ETRYNEXT;

      if ((size_t) (2 + got) < len)
        buffer[2 + got] = '\0';
      buffer[0] = (unsigned char)(got / (1<<8));
      buffer[1] = (unsigned char)(got % (1<<8));

      return got;
    }

  return -ETRYNEXT;
}

static ssize_t
get_from_sys_dev_parport (struct parport *port, int daisy,
			  char *buffer, size_t len)
{
  int fd;
  char *name;

  if (strchr (port->name, '/') || port->name[0] == '.')
    /* Hmm, suspicious. */
    return -ETRYNEXT;

  name = malloc (strlen (port->name) + 50);
  if (!name)
    return -ETRYNEXT;

  if (daisy > -1)
    sprintf (name, "/proc/sys/dev/parport/%s/deviceid%d", port->name, daisy);
  else
    sprintf (name, "/proc/sys/dev/parport/%s/deviceid", port->name);

  fd = open (name, O_RDONLY | O_NOCTTY);
  if (fd >= 0)
    {
      ssize_t got = read (fd, buffer, len);
      free (name);
      close (fd);
      if (got < 1)
	return -ETRYNEXT;
      if ((size_t) got < len)
	buffer[got] = '\0';
      return got;
    }

  if (daisy > -1)
    sprintf (name, "/proc/sys/dev/parport/%s/autoprobe%d", port->name, daisy);
  else
    sprintf (name, "/proc/sys/dev/parport/%s/autoprobe", port->name);

  fd = open (name, O_RDONLY | O_NOCTTY);
  free (name);
  if (fd >= 0)
    {
      ssize_t got = read (fd, buffer + 2, len - 3);
      close (fd);
      if (got < 1)
	return -ETRYNEXT;

      buffer[2 + got] = '\0';
      buffer[0] = (unsigned char)(got / (1<<8));
      buffer[1] = (unsigned char)(got % (1<<8));

      return got;
    }

  return -ETRYNEXT;
}

ssize_t
ieee1284_get_deviceid (struct parport *port, int daisy, int flags,
		       char *buffer, size_t len)
{
  int ret = -1;

  debugprintf ("==> libieee1284_get_deviceid\n");

  if (flags & ~(F1284_FRESH))
    {
      debugprintf ("<== E1284_NOTIMPL (flags)\n");
      return E1284_NOTIMPL;
    }

  //  detect_environment (0);

  if (!(flags & F1284_FRESH))
    {
      if (capabilities & PROC_SYS_DEV_PARPORT_CAPABLE)
	{
	  ret = get_from_sys_dev_parport (port, daisy, buffer, len);
	  debugprintf ("Trying /proc/sys/dev/parport: %s\n",
		   ret < 0 ? "failed" : "success");
	}
      else if (capabilities & PROC_PARPORT_CAPABLE)
	{
	  ret = get_from_proc_parport (port, daisy, buffer, len);
	  debugprintf ("Trying /proc/parport: %s\n",
		   ret < 0 ? "failed" : "success");
	}

      if (ret > -1)
	{
	  debugprintf ("<== %d\n", ret);
	  return ret;
	}

      if (ret == -ENODEVID)
	{
	  debugprintf ("<== E1284_NOTAVAIL (got -ENODEVID)\n");
	  return E1284_NOTAVAIL;
	}
    }

  debugprintf ("Trying device...\n");
  if ((ret = ieee1284_open (port, 0, NULL)) != E1284_OK)
    {
      debugprintf ("<== %d (from ieee1284_open)\n", ret);
      return ret;
    }

  if ((ret = ieee1284_claim (port)) != E1284_OK)
    {
      debugprintf ("<== %d (from ieee1284_claim)\n", ret);
      return ret;
    }

  ret = get_fresh (port, daisy, buffer, len);

  ieee1284_release (port);
  ieee1284_close (port);
  debugprintf ("<== %d (from get_fresh)\n", ret);
  return ret;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

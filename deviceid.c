/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 1999-2001  Tim Waugh <twaugh@redhat.com>
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
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "config.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"
#include "ppdev.h"

#define ETRYNEXT	100
#define ENODEVID	101

#if 0
static void reset (unsigned long base)
{
  unsigned long ctr = base + 2;
  struct timeval tv;
  OUTB (0xc, ctr);
  OUTB (0x8, ctr);
  tv.tv_sec = 0;
  tv.tv_usec = 5000;
  select (0, NULL, NULL, NULL, &tv);
  OUTB (0xc, ctr);
  tv.tv_sec = 0;
  tv.tv_usec = 5000;
  select (0, NULL, NULL, NULL, &tv);
}

static void terminate (unsigned base)
{
  unsigned long ctr = base + 2;
  struct timeval tv;
  OUTB ((INB (ctr) & ~2) | 8, ctr);
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  select (0, NULL, NULL, NULL, &tv);
  OUTB (INB (ctr) | 2, ctr);
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  select (0, NULL, NULL, NULL, &tv);
  OUTB (INB (ctr) & ~2, ctr);
}

static int negotiate (unsigned long base, unsigned char mode)
{
  struct timeval tv;
  unsigned long data = base, status = base + 1, ctr = base + 2;
  unsigned char val;
  val = INB (status);
  reset (base);
  OUTB ((INB (ctr) & ~(1|2)) | 8, ctr);
  OUTB (mode, data);
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  select (0, NULL, NULL, NULL, &tv);
  OUTB ((INB (ctr) & ~8) | 2, ctr);
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  select (0, NULL, NULL, NULL, &tv);
  val = INB (status);
  if ((val & 0x78) != 0x38)
    {
      OUTB ((INB (ctr) & ~2) | 8, ctr);
      fprintf (stderr, "status was %#02x\n", val);
      return 1;
    }
  OUTB (INB (ctr) | 1, ctr);
  OUTB (INB (ctr) & ~(1|2), ctr);
  val = INB (status);
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  select (0, NULL, NULL, NULL, &tv);
  val = INB (status);
  return !(val & 0x10);
}

static size_t read_nibble (unsigned long base, char *buffer, size_t len)
{
  unsigned long status = base + 1, ctr = base + 2;
  unsigned char byte = 0;
  char *buf = buffer;
  struct timeval tv;
  size_t i;

  len *= 2; /* in nibbles */
  for (i = 0; i < len; i++)
    {
      unsigned char nibble;
      unsigned char val = INB (status);

      if ((i & 1) == 0 && (val & 8))
	{
	  OUTB (INB (ctr) | 2, ctr);
	  return i/2;
	}

      OUTB (INB (ctr) | 2, ctr);
      tv.tv_sec = 0;
      tv.tv_usec = 1000;
      select (0, NULL, NULL, NULL, &tv);
      nibble = INB (status) >> 3;
      nibble &= ~8;
      if ((nibble & 0x10) == 0)
	nibble |= 8;
      nibble &= 0xf;
      OUTB (INB (ctr) & ~2, ctr);
      tv.tv_sec = 0;
      tv.tv_usec = 1000;
      select (0, NULL, NULL, NULL, &tv);
      if (i & 1)
	{
	  byte |= nibble << 4;
	  *buf++ = byte;
	}
      else
	{
	  byte = nibble;
	}
    }

  return i/2;
}

static ssize_t get_using_io (struct parport *port, int daisy,
			     char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  if (daisy > -1)
    return -1;

  if (negotiate (priv->base, 4))
    {
      fprintf (stderr, "Couldn't go to nibble mode\n");
      return -ENODEVID;
    }

  len = read_nibble (priv->base, buffer, len);
  terminate (priv->base);
  return len;
}
#endif

static ssize_t
get_fresh (struct parport *port, int daisy,
	   char *buffer, size_t len)
{
  ssize_t got;
  size_t idlen;

  if (daisy > -1)
    /* No implementation yet for IEEE 1284.3 devices. */
    return -ETRYNEXT;

  ieee1284_terminate (port);
  if (ieee1284_negotiate (port,
			  M1284_NIBBLE | M1284_FLAG_DEVICEID) != E1284_OK)
    return -ENODEVID;

  got = ieee1284_nibble_read (port, buffer, 2);
  if (got < 2)
    return -ENODEVID;

  idlen = buffer[0] * 256 + buffer[1];
  if (idlen >= len - 2)
    idlen = len - 2;
  got += ieee1284_nibble_read (port, buffer + 2, idlen);
  if (got < len)
    buffer[got] = '\0';

  ieee1284_terminate (port);
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

      if ((2 + got) < len)
	buffer[2 + got] = '\0';
      buffer[0] = got / (1<<8);
      buffer[1] = got % (1<<8);

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
      if (got < len)
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
      buffer[0] = got / (1<<8);
      buffer[1] = got % (1<<8);

      return got;
    }

  return -ETRYNEXT;
}

ssize_t
ieee1284_get_deviceid (struct parport *port, int daisy, int flags,
		       char *buffer, size_t len)
{
  int ret = -1;

  if (flags & ~(F1284_FRESH))
    return E1284_NOTIMPL;

  detect_environment (0);

  if (!(flags & F1284_FRESH))
    {
      if (capabilities & PROC_SYS_DEV_PARPORT_CAPABLE)
	ret = get_from_sys_dev_parport (port, daisy, buffer, len);
      else if (capabilities & PROC_PARPORT_CAPABLE)
	ret = get_from_proc_parport (port, daisy, buffer, len);

      if (ret > -1)
	return ret;

      if (ret == -ENODEVID)
	return -1;
    }

  if (ieee1284_claim (port))
    return -1;

#if 0
  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ret = get_using_ppdev (port, daisy, buffer, len);
      if (ret > -1)
	{
	  ieee1284_release (port);
	  return ret;
	}

      if (ret == -ENODEVID)
	{
	  ieee1284_release (port);
	  return -1;
	}
      break;

    case IO_CAPABLE:
    case DEV_PORT_CAPABLE:
      use_dev_port (priv->type == DEV_PORT_CAPABLE);
      ret = get_using_io (port, daisy, buffer, len);
      if (ret > -1)
	{
	  ieee1284_release (port);
	  return ret;
	}

      if (ret == -ENODEVID)
	{
	  ieee1284_release (port);
	  return -1;
	}
      break;
    }
#else
  ret = get_fresh (port, daisy, buffer, len);
#endif

  ieee1284_release (port);
  return ret;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

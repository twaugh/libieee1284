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

#include <fcntl.h>
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

int ieee1284_negotiate (struct parport *port, int mode)
{
  struct parport_internal *priv = port->priv;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      return ioctl (priv->fd, PPNEGOT, &mode);

    case IO_CAPABLE:
      use_dev_port (0);
      return negotiate (priv->base, mode);

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      return negotiate (priv->base, mode);
    }

  return -1;
}

void ieee1284_terminate (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_COMPAT;

  if (!priv->claimed)
    return;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPNEGOT, &mode);
      break;

    case IO_CAPABLE:
      use_dev_port (0);
      terminate (priv->base);
      break;

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      terminate (priv->base);
      break;
    }
}

ssize_t ieee1284_nibble_read (struct parport *port, char *buffer,
			      size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_NIBBLE;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return read (priv->fd, buffer, len);

    case IO_CAPABLE:
      use_dev_port (0);
      return read_nibble (priv->base, buffer, len);

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      return read_nibble (priv->base, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_compat_write (struct parport *port,
			       const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_COMPAT;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return write (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_byte_read (struct parport *port, char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_BYTE;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return read (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_epp_read_data (struct parport *port, char *buffer,
				size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return read (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_epp_write_data (struct parport *port,
				 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_EPP | IEEE1284_DATA;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return write (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_epp_read_addr (struct parport *port, char *buffer,
				size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return read (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_epp_write_addr (struct parport *port,
				 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_EPP | IEEE1284_ADDR;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return write (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_ecp_read_data (struct parport *port, char *buffer,
				size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_ECP | IEEE1284_DATA;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return read (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_ecp_write_data (struct parport *port,
				 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_ECP | IEEE1284_DATA;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return write (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_ecp_read_addr (struct parport *port, char *buffer,
				size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_ECP | IEEE1284_ADDR;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return read (priv->fd, buffer, len);
    }

  return -1;
}

ssize_t ieee1284_ecp_write_addr (struct parport *port,
				 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;
  int mode = IEEE1284_MODE_ECP | IEEE1284_ADDR;
  if (!priv->claimed)
    return -1;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPSETMODE, &mode);
      return write (priv->fd, buffer, len);
    }

  return -1;
}

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

#include "config.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"
#include "ppdev.h"

unsigned char ieee1284_read_data (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  unsigned char reg;

  if (!priv->claimed)
    return 0xff;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPRDATA, &reg);
      return reg;

    case IO_CAPABLE:
      use_dev_port (0);
      return INB (priv->base);

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      return INB (priv->base);
    }

  return 0xff;
}

void ieee1284_write_data (struct parport *port, unsigned char reg)
{
  struct parport_internal *priv = port->priv;
  if (!priv->claimed)
    return;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPWDATA, &reg);
      break;

    case IO_CAPABLE:
      use_dev_port (0);
      OUTB (reg, priv->base);
      break;

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      OUTB (reg, priv->base);
    }
}

void ieee1284_data_dir (struct parport *port, int reverse)
{
  struct parport_internal *priv = port->priv;
  if (!priv->claimed)
    return;

  if (priv->type == PPDEV_CAPABLE)
    ioctl (priv->fd, PPDATADIR, &reverse);
}

unsigned char ieee1284_read_status (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  unsigned char reg;

  if (!priv->claimed)
    return 0xff;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPRSTATUS, &reg);
      return reg;

    case IO_CAPABLE:
      use_dev_port (0);
      return INB (priv->base + 1);

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      return INB (priv->base + 1);
    }

  return 0xff;
}

unsigned char ieee1284_read_control (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  unsigned char reg;

  if (!priv->claimed)
    return 0xff;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPRCONTROL, &reg);
      return reg;

    case IO_CAPABLE:
      use_dev_port (0);
      return INB (priv->base + 2);

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      return INB (priv->base + 2);
    }

  return 0xff;
}

void ieee1284_write_control (struct parport *port, unsigned char reg)
{
  struct parport_internal *priv = port->priv;
  if (!priv->claimed)
    return;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ioctl (priv->fd, PPWCONTROL, &reg);
      break;

    case IO_CAPABLE:
      use_dev_port (0);
      OUTB (reg, priv->base + 2);
      break;

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      OUTB (reg, priv->base + 2);
    }
}

void ieee1284_frob_control (struct parport *port, unsigned char mask,
			    unsigned char val)
{
  struct parport_internal *priv = port->priv;
  struct ppdev_frob_struct ppfs;

  if (!priv->claimed)
    return;

  switch (priv->type)
    {
    case PPDEV_CAPABLE:
      ppfs.mask = mask;
      ppfs.val = val;
      ioctl (priv->fd, PPFCONTROL, &ppfs);
      break;

    case IO_CAPABLE:
      use_dev_port (0);
      OUTB ((INB (priv->base + 2) & ~mask) | val, priv->base + 2);
      break;

    case DEV_PORT_CAPABLE:
      use_dev_port (1);
      OUTB ((INB (priv->base + 2) & ~mask) | val, priv->base + 2);
    }
}


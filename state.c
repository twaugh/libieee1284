/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2001  Tim Waugh <twaugh@redhat.com>
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
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "access.h"
#include "config.h"
#include "debug.h"
#include "default.h"
#include "delay.h"
#include "detect.h"
#include "ieee1284.h"

#include "parport.h"
#include "ppdev.h"

static int
init_port (struct parport *port, int flags, int *caps)
{
  struct parport_internal *priv = port->priv;
  int ret = E1284_INIT;

  dprintf ("==> init_port\n");

  if ((capabilities & PPDEV_CAPABLE) && priv->device)
    {
      priv->type = PPDEV_CAPABLE;
      memcpy (priv->fn, &ppdev_access_methods, sizeof *priv->fn);
      ret = priv->fn->init (priv, flags, caps);
      dprintf ("Got %d from ppdev init\n", ret);
    }

  if (ret && (capabilities & IO_CAPABLE))
    {
      priv->type = IO_CAPABLE;
      memcpy (priv->fn, &ppdev_access_methods, sizeof *priv->fn);
      ret = priv->fn->init (priv, flags, caps);
      dprintf ("Got %d from IO init\n", ret);
    }

  if (ret && (capabilities & DEV_PORT_CAPABLE))
    {
      priv->type = DEV_PORT_CAPABLE;
      memcpy (priv->fn, &ppdev_access_methods, sizeof *priv->fn);
      ret = priv->fn->init (priv, flags, caps);
      dprintf ("Got %d from /dev/port init\n", ret);
    }

  dprintf ("<== %d\n", ret);
  return ret;
}

int
ieee1284_open (struct parport *port, int flags, int *capabilities)
{
  struct parport_internal *priv = port->priv;
  int ret;

  dprintf ("==> ieee1284_open\n");

  if (capabilities)
    *capabilities = (CAP1284_NIBBLE | CAP1284_BYTE | CAP1284_COMPAT |
		     CAP1284_ECPSWE);

  ret = init_port (port, flags, capabilities);
  if (ret)
    {
      dprintf ("<== %d (propagated)\n", ret);
      return ret;
    }

  priv->opened = 1;
  priv->ref++;
  return E1284_OK;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

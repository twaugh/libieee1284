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
#include "delay.h"
#include "detect.h"
#include "ieee1284.h"

#include "parport.h"
#include "ppdev.h"

static int
init_port (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  int ret = E1284_INIT;

  if ((capabilities & PPDEV_CAPABLE) && priv->device)
    {
      priv->type = PPDEV_CAPABLE;
      priv->fn = &ppdev_access_methods;
      ret = priv->fn->init (priv);
    }

  if (ret && (capabilities & IO_CAPABLE))
    {
      priv->type = IO_CAPABLE;
      priv->fn = &io_access_methods;
      ret = priv->fn->init (priv);
    }

  if (ret && (capabilities & DEV_PORT_CAPABLE))
    {
      priv->type = DEV_PORT_CAPABLE;
      priv->fn = &io_access_methods;
      ret = priv->fn->init (priv);
    }

  return ret;
}

int
ieee1284_claim (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  int ret;
  if (priv->type == 0)
    {
      ret = init_port (port);
      if (ret)
	return ret;
    }

  switch (priv->type)
    {
    case 0: /* no way to talk to port.  Shouldn't get here. */
      return E1284_INIT;

    case PPDEV_CAPABLE:
      if (!ioctl (priv->fd, PPCLAIM))
	priv->claimed = 1;
      break;

    default:
      priv->claimed = 1;
    }

  return !priv->claimed;
}

void
ieee1284_release (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  if (priv->type == PPDEV_CAPABLE)
    ioctl (priv->fd, PPRELEASE);
  priv->claimed = 0;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

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
#include <stdio.h>
#include <string.h>
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "access.h"
#include "config.h"
#include "default.h"
#include "delay.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"
#include "ppdev.h"

static unsigned char
raw_inb (struct parport_internal *port, unsigned long addr)
{
  return inb (addr);
}

static void
raw_outb (struct parport_internal *port, unsigned char val, unsigned long addr)
{
  outb_p (val, addr);
}

static unsigned char
port_inb (struct parport_internal *port, unsigned long addr)
{
  unsigned char byte = 0xff;

  if (lseek (port->fd, addr, SEEK_SET) != (off_t)-1)
    read (port->fd, &byte, 1);

  return byte;
}

static void
port_outb (struct parport_internal *port, unsigned char val,
	   unsigned long addr)
{
  if (lseek (port->fd, addr, SEEK_SET) != (off_t)-1)
    write (port->fd, &val, 1);
}

static int
init (struct parport_internal *port, int flags, int *capabilities)
{
  if (flags || capabilities)
    return E1284_NOTAVAIL;

  /* TODO: To support F1284_EXCL here we need to open the relevant
   * /dev/lp device. */

  switch (port->type)
    {
    case IO_CAPABLE:
      if (ioperm (port->base, 3, 1) || !ioperm (0x80, 1, 1))
	return E1284_INIT;
      break;

    case DEV_PORT_CAPABLE:
      port->fd = open ("/dev/port", O_RDWR | O_NOCTTY);
      if (port->fd < 0)
	return E1284_INIT;
      port->fn->inb = port_inb;
      port->fn->outb = port_outb;
      break;
    }

  /* Need to write this.
   * If we find an ECP port, we can adjust some of the access function
   * pointers in io_access_functions to point to functions that use
   * hardware assistance. */

  return E1284_OK;
}

static void
cleanup (struct parport_internal *port)
{
  if (port->type != IO_CAPABLE && port->fd >= 0)
    close (port->fd);

  free ((struct parport_access_methods *) (port->fn));
}

static int
read_data (struct parport_internal *port)
{
  return port->fn->inb (port, port->base);
}

static void
write_data (struct parport_internal *port, unsigned char reg)
{
  port->fn->outb (port, reg, port->base);
}

static int
read_status (struct parport_internal *port)
{
  return port->fn->inb (port, port->base + 1) ^ S1284_INVERTED;
}

static void
raw_frob_control (struct parport_internal *port,
		  unsigned char mask,
		  unsigned char val)
{
  unsigned char ctr = port->ctr;
  /* Deal with inversion issues. */
  val ^= mask & C1284_INVERTED;
  ctr = (ctr & ~mask) ^ val;
  port->fn->outb (port, ctr, port->base + 2);
  port->ctr = ctr;
}

static int
read_control (struct parport_internal *port)
{
  const unsigned char rm = (C1284_NSTROBE |
			    C1284_NAUTOFD |
			    C1284_NINIT |
			    C1284_NSELECTIN);
  return port->ctr & rm;
}

static void
data_dir (struct parport_internal *port, int reverse)
{
  raw_frob_control (port, 0x20, reverse ? 0x20 : 0x00);
}

static void
write_control (struct parport_internal *port, unsigned char reg)
{
  const unsigned char wm = (C1284_NSTROBE |
			    C1284_NAUTOFD |
			    C1284_NINIT |
			    C1284_NSELECTIN);
  if (reg & 0x20)
    {
      printf ("use ieee1284_data_dir to change data line direction!\n");
      data_dir (port, 1);
    }

  raw_frob_control (port, wm, reg & wm);
}

static void
frob_control (struct parport_internal *port,
	      unsigned char mask,
	      unsigned char val)
{
  const unsigned char wm = (C1284_NSTROBE |
			    C1284_NAUTOFD |
			    C1284_NINIT |
			    C1284_NSELECTIN);
  if (mask & 0x20)
    {
      printf ("use ieee1284_data_dir to change data line direction!\n");
      data_dir (port, val & 0x20);
    }

  mask &= wm;
  val &= wm;
  raw_frob_control (port, mask, val);
}

static int
wait_status (struct parport_internal *port,
	     unsigned char mask, unsigned char val,
	     struct timeval *timeout)
{
  /* Simple-minded polling.  TODO: Use David Paschal's method for this. */
  struct timeval deadline, now;
  gettimeofday (&deadline, NULL);
  deadline.tv_sec += timeout->tv_sec;
  deadline.tv_usec += timeout->tv_usec;
  deadline.tv_sec += deadline.tv_usec / 1000000;
  deadline.tv_usec %= 1000000;

  do
    {
      if ((read_status (port) & mask) == val)
	return E1284_OK;

      delay (IO_POLL_DELAY);
      gettimeofday (&now, NULL);
    }
  while (now.tv_sec < deadline.tv_sec ||
	 (now.tv_sec == deadline.tv_sec &&
	  now.tv_usec < deadline.tv_usec));

  return E1284_TIMEDOUT;
}

const struct parport_access_methods io_access_methods =
{
  init,
  cleanup,

  NULL, /* claim */
  NULL, /* release */

  raw_inb,
  raw_outb,

  NULL, /* get_irq_fd */

  read_data,
  write_data,
  default_wait_data,
  data_dir,

  read_status,
  wait_status,

  read_control,
  write_control,
  frob_control,

  default_do_nack_handshake,
  default_negotiate,
  default_terminate,
  default_ecp_fwd_to_rev,
  default_ecp_rev_to_fwd,
  default_nibble_read,
  default_compat_write,
  default_byte_read,
  default_epp_read_data,
  default_epp_write_data,
  default_epp_read_addr,
  default_epp_write_addr,
  default_ecp_read_data,
  default_ecp_write_data,
  default_ecp_read_addr,
  default_ecp_write_addr
};

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

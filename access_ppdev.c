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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "access.h"
#include "config.h"
#include "default.h"
#include "delay.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"
#include "ppdev.h"

static int
init (struct parport_internal *port)
{
  struct parport_access_methods *fn = malloc (sizeof *fn);

  if (!fn)
    return E1284_NOMEM;

  memcpy (fn, port->fn, sizeof *fn);

  port->fd = open (port->device, O_RDWR | O_NOCTTY);
  if (port->fd < 0)
    return E1284_INIT;

  if (port->flags & F1284_EXCL)
    {
      if (ioctl (port->fd, PPEXCL))
	return E1284_INIT;
    }

  if (port->interrupt == -1)
    /* Our implementation of do_nack_handshake relies on interrupts
     * being available.  They aren't, so use the default one instead. */
    fn->do_nack_handshake = default_do_nack_handshake;
  else *(port->selectable_fd) = port->fd;


  port->fn = fn;
  return E1284_OK;
}

static void
cleanup (struct parport_internal *port)
{
  if (port->fd >= 0)
    close (port->fd);
}

static int
read_data (struct parport_internal *port)
{
  unsigned char reg;
  if (ioctl (port->fd, PPRDATA, &reg))
    return E1284_NOTAVAIL;

  return reg;
}

static void
write_data (struct parport_internal *port, unsigned char reg)
{
  ioctl (port->fd, PPWDATA, &reg);
}

static int
read_status (struct parport_internal *port)
{
  unsigned char reg;
  if (ioctl (port->fd, PPRSTATUS, &reg))
    return E1284_NOTAVAIL;

  return reg ^ S1284_INVERTED;
}

static int
read_control (struct parport_internal *port)
{
  unsigned char reg;
  const unsigned char rm = (C1284_NSTROBE |
			    C1284_NAUTOFD |
			    C1284_NINIT |
			    C1284_NSELECTIN);
  if (ioctl (port->fd, PPRCONTROL, &reg))
    return E1284_NOTAVAIL;

  return (reg ^ C1284_INVERTED) & rm;
}

static void
data_dir (struct parport_internal *port, int reverse)
{
  ioctl (port->fd, PPDATADIR, &reverse);
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

  reg &= wm;
  reg ^= C1284_INVERTED;
  ioctl (port->fd, PPWCONTROL, &reg);
}

static void
frob_control (struct parport_internal *port,
			  unsigned char mask,
			  unsigned char val)
{
  struct ppdev_frob_struct ppfs;

  if (mask & 0x20)
    {
      printf ("use ieee1284_data_dir to change data line direction!\n");
      data_dir (port, val & 0x20);
    }

  /* Deal with inversion issues. */
  ppfs.mask = mask;
  ppfs.val = val ^ (mask & C1284_INVERTED);
  dprintf ("frob_control: ioctl(%d, PPFCONTROL, { mask:%#02x, val:%#02x }\n",
	   port->fd, ppfs.mask, ppfs.val);
  ioctl (port->fd, PPFCONTROL, &ppfs);
}

static int
wait_status (struct parport_internal *port,
	     unsigned char mask, unsigned char val,
	     struct timeval *timeout)
{
  /* This could be smarter: if we're just waiting for nAck, and we
   * have interrutps to work with, we can just wait for an interrupt
   * rather than polling. */

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

static int
do_nack_handshake (struct parport_internal *port,
		   unsigned char ct_before,
		   unsigned char ct_after,
		   struct timeval *timeout)
{
  fd_set rfds;
  int count;

  if (ioctl (port->fd, PPCLRIRQ, &count))
    return E1284_NOTAVAIL;

  if (ioctl (port->fd, PPWCTLONIRQ, &ct_after))
    return E1284_NOTAVAIL;

  write_control (port, ct_before);

  FD_ZERO (&rfds);
  FD_SET (port->fd, &rfds);

  switch (select (port->fd + 1, &rfds, NULL, NULL, timeout))
    {
    case 0:
      return E1284_TIMEDOUT;

    case -1:
      return E1284_NOTAVAIL;
    }

  ioctl (port->fd, PPCLRIRQ, &count);
  if (count != 1)
    {
      printf ("Multiple interrupts caught?\n");
    }

  return E1284_OK;
}

static int
set_mode (struct parport_internal *port, int mode, int flags, int addr)
{
  int m;
  int ret = 0;

  switch (mode)
    {
    case M1284_NIBBLE:
    case M1284_BYTE:
    case M1284_COMPAT:
      m = mode;
      break;

    case M1284_ECP:
      if (flags & F1284_RLE)
	m = IEEE1284_MODE_ECPRLE;
      else if (flags & F1284_SWE)
	m = IEEE1284_MODE_ECPSWE;
      else if (flags)
	return E1284_NOTIMPL;
      else m = IEEE1284_MODE_ECP;
      break;

    case M1284_EPP:
      if (flags & F1284_SWE)
	m = IEEE1284_MODE_EPPSWE;
      else if (flags)
	return E1284_NOTIMPL;
      else m = IEEE1284_MODE_EPP;
      break;

    default:
      return E1284_NOTIMPL;
    }

  m |= addr ? IEEE1284_ADDR : IEEE1284_DATA;
  if (port->current_mode != m)
    {
      ret = ioctl (port->fd, PPSETMODE, &m);
      if (!ret)
	port->current_mode = m;
    }

  return ret;
}

static ssize_t
translate_error_code (ssize_t e)
{
  if (e < 0)
    return E1284_SYS;
  return e;
}

static ssize_t
nibble_read (struct parport_internal *port, char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_NIBBLE, 0, 0);
  if (!ret)
    ret = translate_error_code (read (port->fd, buffer, len));
  return ret;
}

static ssize_t
compat_write (struct parport_internal *port, const char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_COMPAT, 0, 0);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

static ssize_t
byte_read (struct parport_internal *port, char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_BYTE, 0, 0);
  if (!ret)
    ret = translate_error_code (read (port->fd, buffer, len));
  return ret;
}

static ssize_t
epp_read_data (struct parport_internal *port, int flags,
	       char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_EPP, flags, 0);
  if (!ret)
    ret = translate_error_code (read (port->fd, buffer, len));
  return ret;
}

static ssize_t
epp_write_data (struct parport_internal *port, int flags,
		const char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_EPP, flags, 0);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

static ssize_t
epp_read_addr (struct parport_internal *port, int flags,
	       char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_EPP, flags, 1);
  if (!ret)
    ret = translate_error_code (read (port->fd, buffer, len));
  return ret;
}

static ssize_t
epp_write_addr (struct parport_internal *port, int flags,
		const char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_EPP, flags, 1);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

static ssize_t
ecp_read_data (struct parport_internal *port, int flags,
	       char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_ECP, flags, 0);
  if (!ret)
    ret = translate_error_code (read (port->fd, buffer, len));
  return ret;
}

static ssize_t
ecp_write_data (struct parport_internal *port, int flags,
		const char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_ECP, flags, 0);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

static ssize_t
ecp_write_addr (struct parport_internal *port, int flags,
		const char *buffer, size_t len)
{
  int ret;
  ret = set_mode (port, M1284_ECP, flags, 1);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

const struct parport_access_methods ppdev_access_methods =
{
  init,
  cleanup,

  NULL,
  NULL,

  read_data,
  write_data,
  data_dir,

  read_status,
  wait_status,

  read_control,
  write_control,
  frob_control,

  do_nack_handshake,

  default_negotiate,
  default_terminate,
  default_ecp_fwd_to_rev,
  default_ecp_rev_to_fwd,
  nibble_read,
  compat_write,
  byte_read,
  epp_read_data,
  epp_write_data,
  epp_read_addr,
  epp_write_addr,
  ecp_read_data,
  ecp_write_data,
  default_ecp_read_addr,
  ecp_write_addr
};

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

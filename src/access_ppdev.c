/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2001, 2002  Tim Waugh <twaugh@redhat.com>
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
#include "debug.h"
#include "default.h"
#include "delay.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"
#include "ppdev.h"

struct ppdev_priv 
{
  struct timeval inactivity_timer;
  int nonblock;
};

static void
find_capabilities (int fd, int *c)
{
  int m;

  /* Work around a 2.4.x kernel bug by claiming the port for this
   * even though we shouldn't have to. */
  if (ioctl (fd, PPCLAIM))
    goto guess;

  if (ioctl (fd, PPGETMODES, &m))
    {
      ioctl (fd, PPRELEASE);
 guess:
      *c |= CAP1284_ECP | CAP1284_ECPRLE | CAP1284_EPP;
      return;
    }
  ioctl (fd, PPRELEASE);

  if (m & PARPORT_MODE_PCSPP)
    *c |= CAP1284_RAW;
  if (m & PARPORT_MODE_EPP)
    *c |= CAP1284_EPP;
  if (m & PARPORT_MODE_ECP)
    *c |= CAP1284_ECP | CAP1284_ECPRLE;
  if (m & PARPORT_MODE_DMA)
    *c |= CAP1284_DMA;
}

static int
init (struct parport_internal *port, int flags, int *capabilities)
{
  if (flags & ~F1284_EXCL)
    return E1284_NOTAVAIL;

  port->access_priv = malloc (sizeof (struct ppdev_priv));
  if (!port->access_priv)
    return E1284_NOMEM;

  ((struct ppdev_priv *)port->access_priv)->nonblock = 0;
  port->fd = open (port->device, O_RDWR | O_NOCTTY);
  if (port->fd < 0)
    {
      free (port->access_priv);
      return E1284_INIT;
    }

  port->current_mode = M1284_COMPAT;
  if (flags & F1284_EXCL)
    {
      if (ioctl (port->fd, PPEXCL))
	{
	  close (port->fd);
	  free (port->access_priv);
	  return E1284_INIT;
	}
    }

  if (port->interrupt == -1)
    /* Our implementation of do_nack_handshake relies on interrupts
     * being available.  They aren't, so use the default one instead. */
    port->fn->do_nack_handshake = default_do_nack_handshake;
  else if (capabilities)
    *capabilities |= CAP1284_IRQ;

  if (capabilities)
    find_capabilities (port->fd, capabilities);

  return E1284_OK;
}

static void
cleanup (struct parport_internal *port)
{
  free (port->access_priv);
  if (port->fd >= 0)
    close (port->fd);
}

static int
claim (struct parport_internal *port)
{
  dprintf ("==> claim\n");
  if (ioctl (port->fd, PPCLAIM))
    {
      dprintf ("<== E1284_SYS\n");
      return E1284_SYS;
    }
  dprintf ("<== E1284_OK\n");
  return E1284_OK;
}

static void
release (struct parport_internal *port)
{
  ioctl (port->fd, PPRELEASE);
}

static int
get_irq_fd (struct parport_internal *port)
{
  /* We _don't_ dup the file descriptor because reference counting is
   * done at the port, and we don't want it to be valid after the port
   * has been closed. */
  return port->fd;
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

  return debug_display_status (reg ^ S1284_INVERTED);
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

static int
data_dir (struct parport_internal *port, int reverse)
{
  if (ioctl (port->fd, PPDATADIR, &reverse))
    return E1284_SYS;
  return E1284_OK;
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
  debug_display_control (reg);
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
  debug_frob_control (mask, val);
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
      unsigned char st = debug_display_status (read_status (port));
      if ((st & mask) == val)
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
which_mode (int mode, int flags)
{
  int m;

  if (mode & (M1284_FLAG_DEVICEID | M1284_FLAG_EXT_LINK))
    return mode;

  switch (mode)
    {
    case M1284_NIBBLE:
    case M1284_BYTE:
    case M1284_COMPAT:
    case M1284_ECPRLE:
    case M1284_ECPSWE:
    case M1284_EPPSWE:
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

  return m;
}

static ssize_t
translate_error_code (ssize_t e)
{
  if (e < 0)
    return E1284_SYS;
  return e;
}

static int
set_mode (struct parport_internal *port, int mode, int flags, int addr)
{
  int m = which_mode (mode, flags);
  int ret = E1284_OK;

  if (m < 0)
    return m;

  m |= addr ? IEEE1284_ADDR : IEEE1284_DATA;
  if (port->current_mode != m)
    {
      ret = translate_error_code (ioctl (port->fd, PPSETMODE, &m));
      if (!ret)
	port->current_mode = m;
    }

  return ret;
}

static int do_nonblock (struct parport_internal *port, int flags)
{
  struct ppdev_priv *priv = port->access_priv;
  if ((flags & F1284_NONBLOCK) && !priv->nonblock)
    {
      /* Enable O_NONBLOCK */
      int f = fcntl (port->fd, F_GETFL);

      if (f == -1)
	{
	  dprintf ("do_nonblock: fcntl failed on F_GETFL\n");
	  return -1;
	}

      f |= O_NONBLOCK;
      if (fcntl (port->fd, F_SETFL, f))
	{
	  dprintf ("do_nonblock: fcntl failed on F_SETFL\n");
	  return -1;
	}
    }
  else if ((!(flags & F1284_NONBLOCK)) && priv->nonblock)
    {
      /* Disable O_NONBLOCK */
      int f = fcntl (port->fd, F_GETFL);

      if (f == -1)
	{
	  dprintf ("do_nonblock: fcntl failed on F_GETFL\n");
	  return -1;
	}

      f &= O_NONBLOCK;
      if (fcntl (port->fd, F_SETFL, f))
	{
	  dprintf ("do_nonblock: fcntl failed on F_SETFL\n");
	  return -1;
	}
    }

  return 0;
}

static int
negotiate (struct parport_internal *port, int mode)
{
  int m = which_mode (mode, 0);
  int ret = ioctl (port->fd, PPNEGOT, &m);
  if (!ret)
    port->current_mode = mode;
  return translate_error_code (ret);
}

static void
terminate (struct parport_internal *port)
{
  int m = IEEE1284_MODE_COMPAT;
  if (!ioctl (port->fd, PPNEGOT, &m))
    port->current_mode = IEEE1284_MODE_COMPAT;
}

static ssize_t
nibble_read (struct parport_internal *port, int flags,
	     char *buffer, size_t len)
{
  int ret;
  ret = do_nonblock (port, flags);
  if (!ret)
    ret = set_mode (port, M1284_NIBBLE, 0, 0);
  if (!ret)
    ret = translate_error_code (read (port->fd, buffer, len));
  return ret;
}

static ssize_t
compat_write (struct parport_internal *port, int flags,
	      const char *buffer, size_t len)
{
  int ret;
  ret = do_nonblock (port, flags);
  if (!ret)
    ret = set_mode (port, M1284_COMPAT, 0, 0);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

static ssize_t
byte_read (struct parport_internal *port, int flags,
	   char *buffer, size_t len)
{
  int ret;
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
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
  ret = do_nonblock (port, flags);
  if (!ret)
    ret = set_mode (port, M1284_ECP, flags, 1);
  if (!ret)
    ret = translate_error_code (write (port->fd, buffer, len));
  return ret;
}

static struct timeval *
set_timeout (struct parport_internal *port, struct timeval *timeout)
{
  struct ppdev_priv *priv = port->access_priv;
  ioctl (port->fd, PPGETTIME, &priv->inactivity_timer);
  ioctl (port->fd, PPSETTIME, timeout);
  return &priv->inactivity_timer;
}

const struct parport_access_methods ppdev_access_methods =
{
  init,
  cleanup,

  claim,
  release,

  NULL, /* inb */
  NULL, /* outb */

  get_irq_fd,

  read_data,
  write_data,
  default_wait_data,
  data_dir,

  read_status,
  wait_status,

  read_control,
  write_control,
  frob_control,

  do_nack_handshake,

  negotiate,
  terminate,
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
  ecp_write_addr,
  set_timeout
};

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */
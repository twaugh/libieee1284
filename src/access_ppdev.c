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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#if !(defined __MINGW32__ || defined _MSC_VER)
#include <sys/ioctl.h>
#endif
#include <sys/stat.h>
#ifdef __unix__
#include <unistd.h>
#endif

#include "access.h"
#include "config.h"
#include "debug.h"
#include "default.h"
#include "delay.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"

#ifdef HAVE_LINUX

#include "ppdev.h"

struct ppdev_priv 
{
  struct timeval inactivity_timer;
  int nonblock;
  int current_flags;
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
  if (!(m & PARPORT_MODE_TRISTATE))
    *c &= ~(CAP1284_BYTE | CAP1284_ECPSWE);
}

static int
init (struct parport *pport, int flags, int *capabilities)
{
  struct parport_internal *port = pport->priv;

  if (flags & ~F1284_EXCL)
    return E1284_NOTAVAIL;

  port->access_priv = malloc (sizeof (struct ppdev_priv));
  if (!port->access_priv)
    return E1284_NOMEM;

  ((struct ppdev_priv *)port->access_priv)->nonblock = 0;
  ((struct ppdev_priv *)port->access_priv)->current_flags = 0;
  port->fd = open (port->device, O_RDWR | O_NOCTTY);

  /* Retry with udev/devfs naming, if available */
  if (port->fd < 0)
    {
      if (port->udevice)
	port->fd = open (port->udevice, O_RDWR | O_NOCTTY);

      if (port->fd < 0)
	{
	  free (port->access_priv);
	  return E1284_INIT;
	}
      else
	{
	  pport->filename = port->udevice;
	}
    }
  else
    {
      pport->filename = port->device;
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
  debugprintf ("==> claim\n");
  if (ioctl (port->fd, PPCLAIM))
    {
      debugprintf ("<== E1284_SYS\n");
      return E1284_SYS;
    }
  debugprintf ("<== E1284_OK\n");
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
clear_irq (struct parport_internal *port, unsigned int *count)
{
  int c;

  if (ioctl (port->fd, PPCLRIRQ, &c))
    return E1284_SYS;

  if (count)
    *count = c;

  return E1284_OK;
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
  debugprintf ("frob_control: ioctl(%d, PPFCONTROL, { mask:%#02x, val:%#02x }\n",
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
      else if (flags & ~F1284_NONBLOCK)
	{
	  debugprintf ("flags is %x, but only F1284_RLE, F1284_SWE "
		   "and F1284_NONBLOCK are implemented\n", flags);
	  return E1284_NOTIMPL;
	}
      else m = IEEE1284_MODE_ECP;
      break;

    case M1284_EPP:
      if (flags & F1284_SWE)
	m = IEEE1284_MODE_EPPSWE;
      else if (flags & ~(F1284_FASTEPP | F1284_NONBLOCK))
	{
	  debugprintf ("flags is %x, but only F1284_SWE and F1284_NONBLOCK "
		   "are implemented\n", flags);
	  return E1284_NOTIMPL;
	}
      else m = IEEE1284_MODE_EPP;
      break;

    default:
      debugprintf ("Unknown mode %x\n", mode);
      return E1284_NOTIMPL;
    }

  return m;
}

static ssize_t
translate_error_code (ssize_t e)
{
  if (e == -EAGAIN)
    return E1284_TIMEDOUT;
  if (e < 0)
    return E1284_SYS;
  return e;
}

static int
set_mode (struct parport_internal *port, int mode, int flags, int addr)
{
  struct ppdev_priv *priv = port->access_priv;
  int m = which_mode (mode, flags);
  int f = 0;
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

  if (mode == M1284_EPP && (flags & F1284_FASTEPP))
    f |= PP_FASTREAD | PP_FASTWRITE;

  if (priv->current_flags != f
      && mode == M1284_EPP) /* flags are only relevant for EPP right now */
    {
      ret = translate_error_code (ioctl (port->fd, PPSETFLAGS, &f));
      if (!ret)
	priv->current_flags = f;
    }

  return ret;
}

static int
do_nonblock (struct parport_internal *port, int flags)
{
  struct ppdev_priv *priv = port->access_priv;
  if ((flags & F1284_NONBLOCK) && !priv->nonblock)
    {
      /* Enable O_NONBLOCK */
      int f = fcntl (port->fd, F_GETFL);

      if (f == -1)
	{
	  debugprintf ("do_nonblock: fcntl failed on F_GETFL\n");
	  return -1;
	}

      f |= O_NONBLOCK;
      if (fcntl (port->fd, F_SETFL, f))
	{
	  debugprintf ("do_nonblock: fcntl failed on F_SETFL\n");
	  return -1;
	}
    }
  else if ((!(flags & F1284_NONBLOCK)) && priv->nonblock)
    {
      /* Disable O_NONBLOCK */
      int f = fcntl (port->fd, F_GETFL);

      if (f == -1)
	{
	  debugprintf ("do_nonblock: fcntl failed on F_GETFL\n");
	  return -1;
	}

      f &= O_NONBLOCK;
      if (fcntl (port->fd, F_SETFL, f))
	{
	  debugprintf ("do_nonblock: fcntl failed on F_SETFL\n");
	  return -1;
	}
    }

  return 0;
}

static int
negotiate (struct parport_internal *port, int mode)
{
  int m = which_mode (mode, 0);
  int ret;

  debugprintf ("==> negotiate (to %#02x)\n", mode);

  ret = ioctl (port->fd, PPNEGOT, &m);
  if (!ret)
  {
    port->current_mode = mode;
  } else {
    if (errno == EIO)
      {
	debugprintf ("<== E1284_NEGFAILED\n");
	return E1284_NEGFAILED;
      }

    if (errno == ENXIO)
      {
	debugprintf ("<== E1284_REJECTED\n");
	return E1284_REJECTED;
      }
  }

  m = translate_error_code (ret);
  debugprintf ("<== %d\n", m);
  return m;
}

static void
terminate (struct parport_internal *port)
{
  int m = IEEE1284_MODE_COMPAT;
  if (!ioctl (port->fd, PPNEGOT, &m))
    port->current_mode = IEEE1284_MODE_COMPAT;

  /* Seems to be needed before negotiation. */
  delay (IO_POLL_DELAY);
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
  clear_irq,

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

#else /* Not Linux, no ppdev */

/* Null struct to keep the compiler happy */
const struct parport_access_methods ppdev_access_methods =
{
  NULL,
  NULL,

  NULL,
  NULL,

  NULL, /* inb */
  NULL, /* outb */

  NULL,
  NULL,

  NULL,
  NULL,
  NULL,
  NULL,

  NULL,
  NULL,

  NULL,
  NULL,
  NULL,

  NULL,

  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL 
};



#endif /* HAVE_LINUX */

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
 *
 *
 * This file defines access for VDMLPT on NT kernels.  Basically a copy of 
 * access_io.c with some slight differences.
 */

#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#if !(defined __MINGW32__ || defined _MSC_VER)
#include <sys/ioctl.h>
#else
#include <sys/timeb.h> /* ftime() */
#endif
#include <sys/stat.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifdef __unix__
#include <unistd.h>
#endif

#include "access.h"
#include "debug.h"
#include "default.h"
#include "delay.h"
#include "ieee1284.h"
#include "detect.h"
#include "parport.h"


#ifdef HAVE_CYGWIN_NT

#ifdef __CYGWIN__
#include <w32api/windows.h>
#else
#include <windows.h>
#endif
#include "par_nt.h"


static int
init (struct parport *pport, int flags, int *capabilities)
{ 
  struct parport_internal *port = pport->priv;

  /* Note: We can only ever provide exclusive access on NT. */
  if (flags & ~F1284_EXCL) /* silently ignore F1284_EXCL - dbjh */
    return E1284_NOTAVAIL;

  port->fd = (int)CreateFile(port->device, GENERIC_READ | GENERIC_WRITE,
    0, NULL, OPEN_EXISTING, 0, NULL);
  if (port->fd == (int)INVALID_HANDLE_VALUE) 
  {
    if (port->device != NULL) 
      debugprintf("Failed opening %s\n", port->device);
    return E1284_SYS;
  }

  if (capabilities)
  {
    *capabilities |= CAP1284_RAW;
    /* Can't do bidir mode with this port */
    *capabilities &= ~(CAP1284_ECPSWE | CAP1284_BYTE);
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
  CloseHandle((HANDLE)(port->fd));
}

static void
write_data (struct parport_internal *port, unsigned char reg)
{
  unsigned int dummy;

  if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_DATA, &reg, sizeof(reg), 
          NULL, 0, (LPDWORD)&dummy, NULL)))
      debugprintf("raw_outb: DeviceIoControl failed!\n");
}

static int
read_status (struct parport_internal *port)
{
  char ret;
  unsigned int dummy;

  if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_STATUS, NULL, 0, &ret, 
          sizeof(ret), (LPDWORD)&dummy, NULL)))
      debugprintf("read_status: DeviceIoControl failed!\n");

  return debug_display_status ((unsigned char)(ret ^ S1284_INVERTED));
}

static void
raw_frob_control (struct parport_internal *port,
		  unsigned char mask,
		  unsigned char val)
{
  unsigned char ctr = port->ctr;
  unsigned char dummyc;
  unsigned int dummy;
  /* Deal with inversion issues. */
  val ^= mask & C1284_INVERTED;
  ctr = (ctr & ~mask) ^ val;
  if (!(DeviceIoControl((HANDLE)(port->fd), NT_IOCTL_CONTROL, &ctr, 
          sizeof(ctr), &dummyc, sizeof(dummyc), (LPDWORD)&dummy, NULL)))
      debugprintf("frob_control: DeviceIoControl failed!\n");
  port->ctr = ctr;
  debug_frob_control (mask, val);
}

static int
read_control (struct parport_internal *port)
{
  const unsigned char rm = (C1284_NSTROBE |
			    C1284_NAUTOFD |
			    C1284_NINIT |
			    C1284_NSELECTIN);
  return (port->ctr ^ C1284_INVERTED) & rm;
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
      /* Note: data direction not supported by LPT driver */
      printf ("error: setting data dir is invalid in this mode!\n");
    }

  raw_frob_control (port, wm, (unsigned char)(reg & wm));
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
      /* Note: data direction not supported by LPT driver */
      printf ("error: setting data dir is invalid in this mode!\n");
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
#if !(defined __MINGW32__ || defined _MSC_VER)
  struct timeval deadline, now;
  gettimeofday (&deadline, NULL);
  deadline.tv_sec += timeout->tv_sec;
  deadline.tv_usec += timeout->tv_usec;
  deadline.tv_sec += deadline.tv_usec / 1000000;
  deadline.tv_usec %= 1000000;
#else
  struct timeb tb;
  int deadline, now;
  ftime (&tb);
  deadline = tb.time * 1000 + tb.millitm +
             timeout->tv_sec * 1000 + timeout->tv_usec / 1000;
#endif

  do
    {
      if ((debug_display_status ((unsigned char)(read_status (port))) & mask) == val)
	return E1284_OK;

      delay (IO_POLL_DELAY);
#if !(defined __MINGW32__ || defined _MSC_VER)
      gettimeofday (&now, NULL);
    }
  while (now.tv_sec < deadline.tv_sec ||
    (now.tv_sec == deadline.tv_sec &&
	  now.tv_usec < deadline.tv_usec));
#else
      ftime (&tb);
      now = tb.time * 1000 + tb.millitm;
    }
  while (now < deadline);
#endif

  return E1284_TIMEDOUT;
}

const struct parport_access_methods lpt_access_methods =
{
  init,
  cleanup,

  NULL, /* claim */
  NULL, /* release */

  NULL, /* raw_inb */
  NULL, /* raw_outb */

  NULL, /* get_irq_fd */
  NULL, /* clear_irq */

  NULL, /* read_data */
  write_data,
  default_wait_data,
  NULL, /* data_dir */

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
  default_ecp_write_addr,
  default_set_timeout
};
#else

/* Null struct to keep the compiler happy */
const struct parport_access_methods lpt_access_methods =
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

#endif /* HAVE_CYGWIN_NT */

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

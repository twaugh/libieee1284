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

#include "config.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#if !(defined __MINGW32__ || defined _MSC_VER)
#include <sys/ioctl.h>
#else
#include <io.h> /* lseek(), read(), write(), open(), close() */
#include <sys/timeb.h>
#define O_NOCTTY 0
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
#include "ppdev.h"

#ifdef HAVE_LINUX

#ifdef HAVE_SYS_IO_H
#include <sys/io.h>
#endif

#elif defined(HAVE_SOLARIS)

#define IOPREAD 1
#define IOPWRITE 2
struct iopbuf {
  unsigned int port;
  unsigned char port_value;
};

#elif defined(HAVE_CYGWIN_9X)

#include "io.h"

#elif defined(HAVE_FBSD_I386)

/* don't use machine/cpufunc.h here because it redefines inb and outb as 
   macros, which breaks our port->fn->inb calls */
#include "io.h"

#elif defined(HAVE_OBSD_I386)

#include "io.h"
/* for i386_get_ioperm and i386_set_ioperm */
#include <machine/sysarch.h>

/* 
OpenBSD makes us modify our own bitmap, so this is copied from Linux
kernel-source-2.4.18/arch/i386/kernel/ioport.c 

Set EXTENT bits starting at BASE in BITMAP to new_value.
0 = allowed
1 = denied
*/
static void 
set_bitmap(unsigned long *bitmap, short base, short extent, int new_value)
{
  int mask;
  unsigned long *bitmap_base = bitmap + (base >> 5);
  unsigned short low_index = base & 0x1f;
  int length = low_index + extent;

  if (low_index != 0) {
    mask = (~0 << low_index);
    if (length < 32)
      mask &= ~(~0 << length);
    if (new_value)
      *bitmap_base++ |= mask;
    else
      *bitmap_base++ &= ~mask;
    length -= 32;
  }

  mask = (new_value ? ~0 : 0);
  while (length >= 32) {
    *bitmap_base++ = mask;
    length -= 32;
  }

  if (length > 0) {
    mask = ~(~0 << length);
    if (new_value)
      *bitmap_base++ |= mask;
    else
      *bitmap_base++ &= ~mask;
  }
}

#endif


static unsigned char
raw_inb (struct parport_internal *port, unsigned long addr)
{
#if (defined(HAVE_LINUX) && defined(HAVE_SYS_IO_H)) || defined(HAVE_CYGWIN_9X) \
	|| defined(HAVE_OBSD_I386) || defined(HAVE_FBSD_I386)
  return inb ((unsigned short)addr);

#elif defined(HAVE_SOLARIS)
  struct iopbuf tmpbuf;
  tmpbuf.port = addr;
  if(ioctl(port->fd, IOPREAD, &tmpbuf))
    debugprintf("IOP IOCTL failed on read\n");
  return tmpbuf.port_value;

#else
  return E1284_SYS; /* might not be the best error code to use */
#endif
}

static void
raw_outb (struct parport_internal *port, unsigned char val, unsigned long addr)
{
#if (defined(HAVE_LINUX) && defined(HAVE_SYS_IO_H)) || defined(HAVE_CYGWIN_9X) \
	|| defined(HAVE_OBSD_I386) || defined(HAVE_FBSD_I386)
#if defined(__i386__) || defined(__x86_64__) || defined(_MSC_VER)
  outb_p (val, (unsigned short)addr);
#else
  outb (val, addr);
#endif
  
#elif defined(HAVE_SOLARIS)
  struct iopbuf tmpbuf;
  tmpbuf.port = addr;
  tmpbuf.port_value = val;
  if(ioctl(port->fd, IOPWRITE, &tmpbuf))
    debugprintf("IOP IOCTL failed on write\n");
#endif
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
init (struct parport *pport, int flags, int *capabilities)
{
  struct parport_internal *port = pport->priv;
#ifdef HAVE_SOLARIS
  struct iopbuf tmpbuf;
#elif defined(HAVE_OBSD_I386)
  u_long *iomap;
#endif

  if (flags)
    return E1284_NOTAVAIL;

  /* TODO: To support F1284_EXCL here we need to open the relevant
   * /dev/lp device. */

  switch (port->type)
    {
    case IO_CAPABLE:
#ifdef HAVE_LINUX
#ifdef HAVE_SYS_IO_H
      if (ioperm (port->base, 3, 1) || ioperm (0x80, 1, 1))
        return E1284_INIT;
#else
      return E1284_SYS; /* might not be the best error code to use */
#endif /* HAVE_SYS_IO_H */

#elif defined(HAVE_FBSD_I386)
	/* open the special io device which does the ioperm change for us */
      if ((port->fd = open("/dev/io", O_RDONLY)) < 0)
      {
	debugprintf("Open on /dev/io failed\n");
        return E1284_INIT;
      }
#elif defined(HAVE_OBSD_I386)
      if ((iomap = malloc(1024 / 8)) == NULL)
	return E1284_NOMEM;
      if (i386_get_ioperm(iomap))
	{
	  free(iomap);
          return E1284_INIT;
	}
      /* set access on port 80 and ports base...base+3 */
      set_bitmap(iomap, port->base, 3, 0);
      set_bitmap(iomap, 0x80, 1, 0);

      if (i386_set_ioperm(iomap))
	{
	  free(iomap);
          return E1284_INIT;
	}

      free(iomap);
#elif defined(HAVE_SOLARIS)
      if((port->fd=open("/devices/pseudo/iop@0:iop", O_RDWR)) < 0)
      {
        debugprintf("IOP Device open failed\n");
        return E1284_INIT;
      } else {
        tmpbuf.port = 0x80;
        tmpbuf.port_value = 0xFF;
        if(ioctl(port->fd, IOPREAD, &tmpbuf))
        {
          debugprintf("IOP IOCTL failed on read\n");
          return E1284_INIT;
        }
      }

#endif

      break;

    case DEV_PORT_CAPABLE:
      port->fd = open ("/dev/port", O_RDWR | O_NOCTTY);
      if (port->fd < 0)
	return E1284_INIT;
      port->fn->do_inb = port_inb;
      port->fn->do_outb = port_outb;
      break;
    }

  if (capabilities)
    *capabilities |= CAP1284_RAW;

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
#if defined(HAVE_FBSD_I386) || defined (HAVE_SOLARIS)
  if (port->fd >= 0)
    close(port->fd);
#endif
}

static int
read_data (struct parport_internal *port)
{
  return port->fn->do_inb (port, port->base);
}

static void
write_data (struct parport_internal *port, unsigned char reg)
{
  port->fn->do_outb (port, reg, port->base);
}

static int
read_status (struct parport_internal *port)
{
  return debug_display_status ((unsigned char)
    (port->fn->do_inb (port, port->base + 1) ^ S1284_INVERTED));
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
  port->fn->do_outb (port, ctr, port->base + 2);
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

static int
data_dir (struct parport_internal *port, int reverse)
{
  raw_frob_control (port, 0x20, (unsigned char)(reverse ? 0x20 : 0x00));
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

const struct parport_access_methods io_access_methods =
{
  init,
  cleanup,

  NULL, /* claim */
  NULL, /* release */

  raw_inb,
  raw_outb,

  NULL, /* get_irq_fd */
  NULL, /* clear_irq */

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
  default_ecp_write_addr,
  default_set_timeout
};

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2000-2001 Hewlett-Packard Company
 * Integrated into libieee1284:
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

#include <sys/time.h>
#include <sys/types.h>

#include "access.h"
#include "debug.h"
#include "delay.h"
#include "detect.h"
#include "ieee1284.h"

int
default_wait_data (struct parport_internal *port, unsigned char mask,
		   unsigned char val, struct timeval *timeout)
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
      if ((port->fn->read_data (port) & mask) == val)
	return E1284_OK;

      delay (IO_POLL_DELAY);
      gettimeofday (&now, NULL);
    }
  while (now.tv_sec < deadline.tv_sec ||
	 (now.tv_sec == deadline.tv_sec &&
	  now.tv_usec < deadline.tv_usec));

  return E1284_TIMEDOUT;
}

int
default_do_nack_handshake (struct parport_internal *port,
			   unsigned char ct_before,
			   unsigned char ct_after,
			   struct timeval *timeout)
{
  /* There is a possible implementation using /proc/interrupts on Linux.. */
  return E1284_NOTIMPL;
}

int
default_negotiate (struct parport_internal *port, int mode)
{
  const struct parport_access_methods *fn = port->fn;
  int ret = E1284_NEGFAILED;
  struct timeval tv;
  int m = mode;

  dprintf ("==> default_negotiate (to %#02x)\n", mode);

  switch (mode)
    {
    case M1284_ECPSWE:
      m = M1284_ECP;
      break;
    case M1284_EPPSL:
    case M1284_EPPSWE:
      m = M1284_EPP;
      break;
    case M1284_BECP:
      m = 0x18;
      break;
    }

  if (mode & M1284_FLAG_EXT_LINK)
    m = 1<<7; /* Request extensibility link */

  /* Event 0: Write extensibility request to data lines. */
  fn->write_data (port, m);
  dprintf ("IEEE 1284 mode %#02x\n", m);

  /* Event 1: nSelectIn=1, nAutoFd=0, nStrobe=1, nInit=1. */
  fn->frob_control (port,
		    C1284_NSELECTIN|C1284_NSTROBE|C1284_NINIT
		    |C1284_NAUTOFD,
		    C1284_NSELECTIN|C1284_NSTROBE|C1284_NINIT);

  /* Event 2: PError=1, Select=1, nFault=1, nAck=0. */
  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  if (fn->wait_status (port,
		       S1284_PERROR|S1284_SELECT|S1284_NFAULT
		       |S1284_NACK,
		       S1284_PERROR|S1284_SELECT|S1284_NFAULT, &tv))
  {
    dprintf ("Failed at event 2\n");
    ieee1284_display_status (port);
    goto abort;
  }

  /* Event 3: nStrobe=0. */
  fn->frob_control (port, C1284_NSTROBE, 0);
  delay (TIMEVAL_STROBE_DELAY);

  /* Event 4: nStrobe=1, nAutoFd=1. */
  fn->frob_control (port, C1284_NSTROBE|C1284_NAUTOFD,
		    C1284_NSTROBE|C1284_NAUTOFD);

  /* Event 6: nAck=1. */
  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  if (fn->wait_status (port, S1284_NACK, S1284_NACK, &tv))
  {
    dprintf ("Failed at event 6\n");
    goto abort;
  }

  /* Event 5: Select=0 for nibble-0, =1 for other modes. */
  port->current_mode = !mode;
  if ((fn->read_status (port) & S1284_SELECT) !=
      (mode ? S1284_SELECT : 0))
    {
      ret = E1284_REJECTED;
      dprintf ("Mode rejected\n");
      goto abort;
    }
  port->current_mode = mode;

  /* Extra signalling for ECP mode. */
  if (m & M1284_ECP)
    {
      /* Event 30: nAutoFd=0. */
      fn->frob_control (port, C1284_NAUTOFD, 0);

      /* Event 31: PError=1. */
      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_PERROR, S1284_PERROR, &tv))
      {
	dprintf ("Failed at event 31\n");
	goto abort;
      }

      port->current_channel=0;
    }

  dprintf ("<== E1284_OK\n");
  return E1284_OK;

 abort:
  fn->terminate(port);
  dprintf ("<== %d\n", ret);
  return ret;
}

void
default_terminate (struct parport_internal *port)
{
  const struct parport_access_methods *fn = port->fn;
  struct timeval tv;

  fn->write_control (port, C1284_NINIT | C1284_NAUTOFD | C1284_NSTROBE);

  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  if (fn->wait_status (port, S1284_NACK | S1284_SELECT, 
		       S1284_SELECT, &tv) != E1284_OK)
    return;
	
  fn->write_control (port, C1284_NINIT | C1284_NSTROBE);

  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  if (fn->wait_status (port, S1284_NACK, S1284_NACK, 
		       &tv) != E1284_OK)
    return;

  fn->write_control (port, C1284_NINIT | C1284_NAUTOFD | C1284_NSTROBE);

  return;
}

int
default_ecp_fwd_to_rev (struct parport_internal *port)
{
  return E1284_NOTIMPL;
}

int
default_ecp_rev_to_fwd (struct parport_internal *port)
{
  return E1284_NOTIMPL;
}

ssize_t
default_nibble_read (struct parport_internal *port,
		     char *buffer, size_t len)
{
  const struct parport_access_methods *fn = port->fn;
  size_t count = 0;
  int datain;
  int low, high;
  struct timeval tv;

  dprintf ("==> default_nibble_read\n");

  /* start of reading data from the scanner */
  while (count < len)
    {
      fn->write_control (port, C1284_NSTROBE | C1284_NINIT | C1284_NSELECTIN);

      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_NACK, 0, &tv) 
	  != E1284_OK)
	goto error;

      low = fn->read_status (port) >> 3;
      low = (low & 0x07) + ((low & 0x10) >> 1);

      fn->write_control (port, C1284_NSTROBE | C1284_NINIT | C1284_NSELECTIN
			 | C1284_NAUTOFD);

      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_NACK, S1284_NACK, &tv) 
	  != E1284_OK)
	goto error;

      fn->write_control (port, C1284_NSTROBE | C1284_NINIT | C1284_NSELECTIN);

      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_NACK, 0, &tv) 
	  != E1284_OK)
	goto error;

      high = fn->read_status (port) >> 3;
      high = (high & 0x07) | ((high & 0x10) >> 1);

      fn->write_control (port, C1284_NSTROBE | C1284_NINIT | C1284_NSELECTIN
			 | C1284_NAUTOFD);

      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_NACK, S1284_NACK, &tv) 
	  != E1284_OK)
	goto error;

      datain = (high << 4) + low;

      buffer[count] = datain & 0xff;
      count++;
    }

  dprintf ("<== %d\n", len);
  return len; 

 error:
  fn->terminate (port);
  dprintf ("<== %d (terminated on error)\n", count);
  return count;
}

ssize_t
default_compat_write (struct parport_internal *port,
		      const char *buffer, size_t len)
{
  const struct parport_access_methods *fn = port->fn;
  size_t count = 0;
  struct timeval tv;

  dprintf ("==> default_compat_write\n");

  while (count < len)
    {		
      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_BUSY, 0, &tv) != E1284_OK)
	goto error;

      /* Tsetup: 750ns min. */
      delay (TIMEVAL_STROBE_DELAY);

      /* Get the data byte ready */
      fn->write_data (port, buffer[count]);

      /* Pulse nStrobe low */
      fn->write_control (port, C1284_NINIT | C1284_NAUTOFD);

      /* Tstrobe: 750ns - 500us */
      delay (TIMEVAL_STROBE_DELAY);

      /* And raise it */
      fn->write_control (port, C1284_NINIT | C1284_NAUTOFD | C1284_NSTROBE);

      /* Thold: 750ns min. */
      delay (TIMEVAL_STROBE_DELAY);

      count++;
    }

  dprintf ("<== %d\n", len);
  return len;

 error:
  fn->terminate (port);
  dprintf ("<== %d (terminated on error)\n", count);
  return count;  
}

ssize_t
default_byte_read (struct parport_internal *port,
		   char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_epp_read_data (struct parport_internal *port, int flags,
		       char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_epp_write_data (struct parport_internal *port, int flags,
			const char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_epp_read_addr (struct parport_internal *port, int flags,
		       char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_epp_write_addr (struct parport_internal *port, int flags,
			const char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_ecp_read_data (struct parport_internal *port, int flags,
		       char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_ecp_write_data (struct parport_internal *port, int flags,
			const char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_ecp_read_addr (struct parport_internal *port, int flags,
		       char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

ssize_t
default_ecp_write_addr (struct parport_internal *port, int flags,
			const char *buffer, size_t len)
{
  return E1284_NOTIMPL;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

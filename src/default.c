/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2000-2001 Hewlett-Packard Company
 * Integrated into libieee1284:
 * Copyright (C) 2001-2003  Tim Waugh <twaugh@redhat.com>
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

#include <string.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <sys/types.h>
#ifdef __unix__
#include <unistd.h>
#endif
#if defined __MINGW32__ || defined _MSC_VER
#include <sys/timeb.h>
#endif

#include "access.h"
#include "debug.h"
#include "delay.h"
#include "detect.h"
#include "ieee1284.h"

static const char *no_default = "no default implementation of %s\n";

int
default_wait_data (struct parport_internal *port, unsigned char mask,
		   unsigned char val, struct timeval *timeout)
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
      if ((port->fn->read_data (port) & mask) == val)
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

int
default_do_nack_handshake (struct parport_internal *port,
			   unsigned char ct_before,
			   unsigned char ct_after,
			   struct timeval *timeout)
{
  /* There is a possible implementation using /proc/interrupts on Linux.. */
  debugprintf (no_default, "no_nack_handshake");
  return E1284_NOTIMPL;
}

int
default_negotiate (struct parport_internal *port, int mode)
{
  const struct parport_access_methods *fn = port->fn;
  int ret = E1284_NEGFAILED;
  struct timeval tv;
  int m = mode;

  debugprintf ("==> default_negotiate (to %#02x)\n", mode);

  if (mode == port->current_mode)
    {
      debugprintf ("<== E1284_OK (nothing to do!)\n");
      return E1284_OK;
    }

  if (mode == M1284_COMPAT)
    {
      ret = E1284_OK;
      goto abort;
    }

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
  fn->write_data (port, (unsigned char)m);
  debugprintf ("IEEE 1284 mode %#02x\n", m);

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
    debugprintf ("Failed at event 2\n");
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
    debugprintf ("Failed at event 6\n");
    goto abort;
  }

  /* Event 5: Select=0 for nibble-0, =1 for other modes. */
  port->current_mode = !mode;
  if ((fn->read_status (port) & S1284_SELECT) !=
      (mode ? S1284_SELECT : 0))
    {
      ret = E1284_REJECTED;
      debugprintf ("Mode rejected\n");
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
	debugprintf ("Failed at event 31\n");
	goto abort;
      }

      port->current_channel=0;
      port->current_phase = PH1284_FWD_IDLE;
    }

  debugprintf ("<== E1284_OK\n");
  return E1284_OK;

 abort:
  fn->terminate(port);
  debugprintf ("<== %d\n", ret);
  return ret;
}

void
default_terminate (struct parport_internal *port)
{
  const struct parport_access_methods *fn = port->fn;
  struct timeval tv;

  /* Termination may only be accomplished from the forward phase */
  if (port->current_phase == PH1284_REV_IDLE) 
    /* even if this fails we're trucking on */
    fn->ecp_rev_to_fwd(port);

  fn->write_control (port, C1284_NINIT | C1284_NAUTOFD | C1284_NSTROBE);

  /* Even if this fails we are now implicitly back in compat mode because we 
   * have dropped nSelectIn */
  port->current_mode = M1284_COMPAT;

  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  if (fn->wait_status (port, S1284_NACK, 0, &tv) != E1284_OK)
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
  const struct parport_access_methods *fn = port->fn;
  int retval;
  struct timeval tv;

  debugprintf ("==> default_ecp_fwd_to_rev\n");

  /* Event 38: Set nAutoFd low */
  fn->frob_control (port, C1284_NAUTOFD, 0);

  /* This will always work. If it won't then this method isn't available */
  fn->data_dir (port, 1);
  udelay (5);

  /* Event 39: Set nInit low to initiate bus reversal */
  fn->frob_control (port, C1284_NINIT, 0);

  /* Event 40: PError goes low */
  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  retval = fn->wait_status (port, S1284_PERROR, 0, &tv);

  if (retval) {
    debugprintf ("ECP direction: failed to reverse\n");
    port->current_phase = PH1284_ECP_DIR_UNKNOWN;
  } else {
    port->current_phase = PH1284_REV_IDLE;
  }

  debugprintf ("<== %d default_ecp_fwd_to_rev\n", retval);
  return retval;
}

int
default_ecp_rev_to_fwd (struct parport_internal *port)
{
  const struct parport_access_methods *fn = port->fn;
  int retval;
  struct timeval tv;

  debugprintf ("==> default_ecp_rev_to_fwd\n");

  /* Event 47: Set nInit high */
  fn->frob_control (port, C1284_NINIT | C1284_NAUTOFD, 
	            C1284_NINIT | C1284_NAUTOFD);

  /* Event 49: PError goes high */
  lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
  retval = fn->wait_status (port, S1284_PERROR, S1284_PERROR, &tv);

  if (!retval) {
    fn->data_dir (port, 0);
    port->current_phase = PH1284_FWD_IDLE;
  } else {
    debugprintf ("ECP direction: failed to switch forward\n");
    port->current_phase = PH1284_ECP_DIR_UNKNOWN;
  }

  debugprintf ("<== %d default_ecp_rev_to_fwd\n", retval);
  return retval;
}

ssize_t
default_nibble_read (struct parport_internal *port, int flags,
		     char *buffer, size_t len)
{
  const struct parport_access_methods *fn = port->fn;
  size_t count = 0;
  int datain;
  int low, high;
  struct timeval tv;

  debugprintf ("==> default_nibble_read\n");

  /* start of reading data from the scanner */
  while (count < len)
    {
      /* More data? */
      if ((count & 1) == 0 &&
	  (fn->read_status (port) & S1284_NFAULT))
	{
	  debugprintf ("No more data\n");
	  fn->frob_control (port, C1284_NAUTOFD, 0);
	  break;
	}

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

  debugprintf ("<== %d\n", len);
  return len; 

 error:
  fn->terminate (port);
  debugprintf ("<== %d (terminated on error)\n", count);
  return count;
}

ssize_t
default_compat_write (struct parport_internal *port, int flags,
		      const char *buffer, size_t len)
{
  const struct parport_access_methods *fn = port->fn;
  size_t count = 0;
  struct timeval tv;

  debugprintf ("==> default_compat_write\n");

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

  debugprintf ("<== %d\n", len);
  return len;

 error:
  fn->terminate (port);
  debugprintf ("<== %d (terminated on error)\n", count);
  return count;  
}

ssize_t
default_byte_read (struct parport_internal *port, int flags,
		   char *buffer, size_t len)
{

  const struct parport_access_methods *fn = port->fn;
  unsigned char *buf = buffer;
  size_t count = 0;
  struct timeval tv;

  /* FIXME: Untested as yet, copied from ieee1284_op.c,
   * inverted appropriate signals  */

  debugprintf ("==> default_byte_read\n");

  for (count = 0; count < len; count++) {
    unsigned char byte;

    /* Data available? */
    if (fn->read_status (port) & S1284_PERROR) {
      /* Go to reverse idle phase. */
      fn->frob_control (port, C1284_NAUTOFD, C1284_NAUTOFD);
      break;
    }

    /* Event 14: Place data bus in high impedance state. */
    fn->data_dir (port, 1);

    /* Event 7: Set nAutoFd low. */
    fn->frob_control (port, C1284_NAUTOFD, 0);

    /* Event 9: nAck goes low. */
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    if (fn->wait_status (port, S1284_NACK, 0, &tv)) {
      /* Timeout -- no more data? */
      fn->frob_control (port, C1284_NAUTOFD, C1284_NAUTOFD);
      debugprintf ("Byte timeout at event 9\n");
      break;
    }

    byte = fn->read_data (port);
    *buf++ = byte;

    /* Event 10: Set nAutoFd high */
    fn->frob_control (port, C1284_NAUTOFD, C1284_NAUTOFD);

    /* Event 11: nAck goes high. */
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    if (fn->wait_status (port, S1284_NACK, S1284_NACK, &tv)) {
      /* Timeout -- no more data? */
      debugprintf ("Byte timeout at event 11\n");
      break;
    }

    /* Event 16: Set nStrobe low. */
    fn->frob_control (port, C1284_NSTROBE, 0);
    udelay (5);

    /* Event 17: Set nStrobe high. */
    fn->frob_control (port, C1284_NSTROBE, C1284_NSTROBE);
  }

  debugprintf ("<== %d default_byte_read\n", count);

  return count;

}

ssize_t
default_epp_read_data (struct parport_internal *port, int flags,
		       char *buffer, size_t len)
{
  const struct parport_access_methods *fn = port->fn;
  unsigned char *buf = buffer;
  ssize_t count = 0;
  struct timeval tv;

  /* FIXME: Untested as yet, copied from ieee1284_op.c, 
   * inverted appropriate signals  */

  debugprintf ("==> default_epp_read_data\n");

  /* set EPP idle state (just to make sure) with strobe high */
  fn->frob_control (port, C1284_NSTROBE | C1284_NAUTOFD | 
	                  C1284_NSELECTIN | C1284_NINIT,
	                  C1284_NSTROBE | C1284_NINIT);
  fn->data_dir (port, 1);

  for (; len > 0; len--, buf++) {
    /* Event 67: set nAutoFd (nDStrb) low */
    fn->frob_control (port, C1284_NAUTOFD, 0);
    /* Event 58: wait for Busy to go high */
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    if (fn->wait_status (port, S1284_BUSY, S1284_BUSY, &tv)) {
      break;
    }

    *buf = fn->read_data (port);

    /* Event 63: set nAutoFd (nDStrb) high */
    fn->frob_control (port, C1284_NAUTOFD, C1284_NAUTOFD);

    /* Event 60: wait for Busy to go low */
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    if (fn->wait_status (port, S1284_BUSY, 0, &tv)) {
      break;
    }

    count++;
  }
  fn->data_dir (port, 0);

  debugprintf ("<== default_epp_read_data\n");
  return count;
}

static int poll_port (struct parport_internal *port, unsigned char mask,
		      unsigned char result, int usec)
{
  const struct parport_access_methods *fn = port->fn;
  int count = usec / 5 + 2;
  int i;

  for (i = 0; i < count; i++)
    {
      unsigned char status = fn->read_status (port);

      if ((status & mask) == result)
	return E1284_OK;

      if (i >= 2)
	udelay (5);
    }

  return E1284_TIMEDOUT;
}

ssize_t
default_epp_write_data (struct parport_internal *port, int flags,
			const char *buffer, size_t len)
{
  const struct parport_access_methods *fn = port->fn;
  ssize_t ret = 0;

  debugprintf ("==> default_epp_write_data\n");

  /* Set EPP idle state (just to make sure).  Also set nStrobe low. */
  fn->frob_control (port,
		    C1284_NSTROBE | C1284_NAUTOFD
		    | C1284_NSELECTIN | C1284_NINIT,
		    C1284_NAUTOFD | C1284_NSELECTIN | C1284_NINIT);

  fn->data_dir (port, 0);

  for (; len > 0; len--, buffer++)
    {
      /* Event 62: Write data and set nAutoFd low */
      fn->write_data (port, *buffer);
      fn->frob_control (port, C1284_NAUTOFD, 0);

      /* Event 58: wait for busy (nWait) to go high */
      if (poll_port (port, S1284_BUSY, S1284_BUSY, 10) != E1284_OK)
	{
	  debugprintf ("Failed at event 58\n");
	  break;
	}

      /* Event 63: set nAutoFd (nDStrb) high */
      fn->frob_control (port, C1284_NAUTOFD, C1284_NAUTOFD);

      /* Event 60: wait for busy (nWait) to go low */
      if (poll_port (port, S1284_BUSY, 0, 5) != E1284_OK)
	{
	  debugprintf ("Failed at event 60\n");
	  break;
	}

      ret++;
    }

  debugprintf ("<== %d\n", ret);
  return ret;
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
  /* FIXME: RLE Not tested yet because it's not reported as being available
   * by the upper layers */

  const struct parport_access_methods *fn = port->fn;
  
  unsigned char *buf = buffer;
  size_t rle_count = 0; /* shut gcc up */
  int rle = 0;
  size_t count = 0;
  struct timeval tv;

  debugprintf ("==> default_ecp_read_data\n");

  if (port->current_phase != PH1284_REV_IDLE)
    if (fn->ecp_fwd_to_rev(port))
      return 0;
    
  port->current_phase = PH1284_REV_DATA;

  /* Event 46: Set HostAck (nAutoFd) low to start accepting data. */
  fn->frob_control (port, C1284_NAUTOFD | C1284_NSTROBE | C1284_NINIT, 
		  C1284_NSTROBE);

  while (count < len) {
    unsigned char byte;
    int command; 

    /* Event 43: Peripheral sets nAck low. It can take as long as it wants.. */
    /* FIXME: Should we impose some sensible limit here? */
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    while(fn->wait_status (port, S1284_NACK, 0, &tv)) { } 

    /* Is this a command? */
    if (rle)
      /* The last byte was a run-length count, so this can't be as well. */
      command = 0;
    else
      /* note: test reversed from kernel because BUSY pin is inverted */
      command = (fn->read_status (port) & S1284_BUSY) ? 0 : 1;


    /* Read the data. */
    byte = fn->read_data (port);

    /* If this is a channel command, rather than an RLE
     * command or a normal data byte, don't accept it. */
    if (command) {
      if (byte & 0x80) {
	debugprintf ("Stopping short at channel command (%02x)\n", byte);
	port->current_phase = PH1284_REV_IDLE;
	return count;
      }
      else if (!(flags & F1284_RLE))
	debugprintf ("Device illegally using RLE; accepting anyway\n");

      rle_count = byte + 1;

      /* Are we allowed to read that many bytes? */
      if (rle_count > (len - count)) {
	debugprintf ("Leaving %d RLE bytes for next time\n", 
	    rle_count);
	break;
      }

      rle = 1;
    }

    /* Event 44: Set HostAck high, acknowledging handshake. */
    fn->frob_control (port, C1284_NAUTOFD, C1284_NAUTOFD);

    /* Event 45: The peripheral has 35ms to set nAck high. */
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    if (fn->wait_status (port, S1284_NACK, S1284_NACK, &tv)) {
      /* It's gone wrong.  Return what data we have to the caller. */
      debugprintf ("ECP read timed out at 45\n");

      if (command)
	debugprintf ("Command ignored (%02x)\n", byte);

      break;
    }

    /* Event 46: Set HostAck low and accept the data. */
    fn->frob_control (port, C1284_NAUTOFD, 0);

    /* If we just read a run-length count, fetch the data. */
    if (command)
      continue;
    /* If this is the byte after a run-length count, decompress. */
    if (rle) {
      rle = 0;
      memset (buf, byte, rle_count);
      buf += rle_count;
      count += rle_count;
      debugprintf ("Decompressed to %d bytes\n", rle_count);
    } else {
      /* Normal data byte. */
      *buf = byte;
      buf++, count++;
    }
  }

  port->current_phase = PH1284_REV_IDLE;

  debugprintf ("<== default_ecp_read_data\n");

  return count;
}


ssize_t
default_ecp_write_data (struct parport_internal *port, int flags,
			const char *buffer, size_t len)
{

  const struct parport_access_methods *fn = port->fn;
  const unsigned char *buf = buffer;
  size_t written;
  int retry;
  struct timeval tv;

  debugprintf ("==> default_ecp_write_data\n");

  if (port->current_phase != PH1284_FWD_IDLE)
    if (fn->ecp_rev_to_fwd(port))
      return 0;

  port->current_phase = PH1284_FWD_DATA;

  /* HostAck high (data, not command) */
  fn->frob_control (port, C1284_NAUTOFD | C1284_NINIT, 
	           C1284_NAUTOFD | C1284_NINIT);

  for (written = 0; written < len; written++, buf++) {
    unsigned char byte;

    byte = *buf;
try_again:
    fn->write_data (port, byte);
    /* Event 35: Set NSTROBE low */
    fn->frob_control (port, C1284_NSTROBE, 0);
    udelay (5);
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    for (retry = 0; retry < 100; retry++) {
      /* Event 36: peripheral sets BUSY high */
      if (!fn->wait_status (port, S1284_BUSY, S1284_BUSY, &tv))
	goto success;
    }

    /* Time for Host Transfer Recovery (page 41 of IEEE1284) */
    debugprintf ("ECP transfer stalled!\n");

    fn->frob_control (port, C1284_NINIT, C1284_NINIT);
    udelay (50);
    if (fn->read_status (port) & S1284_PERROR) {
      /* It's buggered. */
      fn->frob_control (port, C1284_NINIT, 0);
      break;
    }

    fn->frob_control (port, C1284_NINIT, 0);
    udelay (50);
    if (!(fn->read_status (port) & S1284_PERROR))
      break;

    debugprintf ("Host transfer recovered\n");

    /* FIXME: Check for timeout here ? */
    goto try_again;
success:
    /* Event 37: HostClk (nStrobe) high */
    fn->frob_control (port, C1284_NSTROBE, C1284_NSTROBE);
    udelay (5);
    lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
    if (fn->wait_status (port, S1284_BUSY, 0, &tv))
      /* Peripheral hasn't accepted the data. */
      break;
  }

  debugprintf ("<== default_ecp_write_data\n");

  port->current_phase = PH1284_FWD_IDLE;

  return written;

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
  const struct parport_access_methods *fn = port->fn;
  const unsigned char *buf = buffer;
  size_t written;
  int retry;
  struct timeval tv;

  debugprintf ("==> default_ecp_write_addr\n");

  if (port->current_phase != PH1284_FWD_IDLE)
    if (fn->ecp_rev_to_fwd(port))
      return 0;
  port->current_phase = PH1284_FWD_DATA;

  /* HostAck (nAutoFd) low (command mode) */
  fn->frob_control (port, C1284_NAUTOFD | C1284_NINIT, 
		    C1284_NINIT);

  for (written = 0; written < len; written++, buf++)
    {
      unsigned char byte;
      byte = *buf;

      /* FIXME: should we do RLE here? */
    try_again:
      fn->write_data (port, byte);
      /* Event 35: Set NSTROBE low */
      fn->frob_control (port, C1284_NSTROBE, 0);
      udelay (5);
      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      for (retry = 0; retry < 100; retry++)
	{
	  /* Event 36: peripheral sets BUSY high */
	  if (!fn->wait_status (port, S1284_BUSY, S1284_BUSY, &tv))
	    goto success;
	}

      /* Time for Host Transfer Recovery (page 41 of IEEE1284) */
      debugprintf ("ECP address transfer stalled!\n");

      fn->frob_control (port, C1284_NINIT, C1284_NINIT);
      udelay (50);
      if (fn->read_status (port) & S1284_PERROR)
	{      
	  /* It's buggered. */
	  fn->frob_control (port, C1284_NINIT, 0);
	  break;
	}

      fn->frob_control (port, C1284_NINIT, 0);
      udelay (50);
      if (!(fn->read_status (port) & S1284_PERROR))
	break;

      debugprintf ("Host address transfer recovered\n");

      /* FIXME: Check for timeout here ? */
      goto try_again;
    success:
      /* Event 37: HostClk (nStrobe) high */
      fn->frob_control (port, C1284_NSTROBE, C1284_NSTROBE);
      udelay (5);
      lookup_delay (TIMEVAL_SIGNAL_TIMEOUT, &tv);
      if (fn->wait_status (port, S1284_BUSY, 0, &tv))
	/* Peripheral hasn't accepted the data. */
	break;
    }

  debugprintf ("<== default_ecp_write_addr\n");
  port->current_phase = PH1284_FWD_IDLE;
  return written;
}

struct timeval *
default_set_timeout (struct parport_internal *port, struct timeval *timeout)
{
  static struct timeval to;
  to.tv_sec = 9999;
  to.tv_usec = 0;
  return &to;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

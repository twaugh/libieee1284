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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _MSC_VER
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <time.h>

#include "debug.h"
#include "detect.h"
#include "ieee1284.h"

#define ENVAR "LIBIEEE1284_DEBUG"
static int debugging_enabled = -1;

static unsigned char soft_ctr = 0xff;

static const char *
timeofday (void)
{
  static char str[100];
  struct timeval tod;
#if !(defined __MINGW32__ || defined _MSC_VER)
  if (gettimeofday (&tod, NULL))
    {
      str[0] = '\0';
    }
  else
#endif
    {
      struct tm *tm = localtime (&tod.tv_sec);
      char *p = str + strftime (str, 50, "%H:%M:%S.", tm);
      sprintf (p, "%06ld", tod.tv_usec);
    }
  return str;
}

unsigned char
debug_display_status (unsigned char st)
{
  static unsigned char last_status = 0xff;

  if (!debugging_enabled)
    goto out;

  if (last_status == st)
    goto out;

  last_status = st;
  debugprintf ("%s STATUS: %cnFault %cSelect %cPError %cnAck %cBusy\n",
	   timeofday (),
	   st & S1284_NFAULT ? ' ' : '!',
	   st & S1284_SELECT ? ' ' : '!',
	   st & S1284_PERROR ? ' ' : '!',
	   st & S1284_NACK ? ' ' : '!',
	   st & S1284_BUSY ? ' ' : '!');

 out:
  return st;
}

unsigned char
debug_display_control (unsigned char ct)
{
  if (!debugging_enabled)
    goto out;

  if (soft_ctr == ct)
    goto out;

  soft_ctr = ct;
  debugprintf ("%s CONTROL: %cnStrobe %cnAutoFd %cnInit %cnSelectIn\n",
	   timeofday (),
	   ct & C1284_NSTROBE ? ' ' : '!',
	   ct & C1284_NAUTOFD ? ' ' : '!',
	   ct & C1284_NINIT ? ' ' : '!',
	   ct & C1284_NSELECTIN ? ' ' : '!');

 out:
  return ct;
}

void
debug_frob_control (unsigned char mask, unsigned char val)
{
  if (debugging_enabled)
    {
      unsigned char new_ctr = (soft_ctr & ~mask) ^ val;
      debug_display_control (new_ctr);
    }
}

void
debugprintf (const char *fmt, ...)
{
  if (!debugging_enabled)
    return;

  if (debugging_enabled == -1) {
    int dummy;
    (void) dummy; /* warning remover for MinGW and VC++ */

    if (!getenv (ENVAR))
      {
	debugging_enabled = 0;
	return;
      }

#if !(defined __MINGW32__ || defined _MSC_VER)
    /* Is stderr open? */
    if (fcntl (fileno (stderr), F_GETFL, &dummy) == -1 && errno == EBADF)
      {
	debugging_enabled = 0;
	return;
      }
#endif

    debugging_enabled = 1;
  }
  
  {
    va_list ap;
    va_start (ap, fmt);
    vfprintf (stderr, fmt, ap);
    va_end (ap);
  }
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

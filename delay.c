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

#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "delay.h"

// TODO: Do this with an array.
void lookup_delay (int which, struct timeval *tv)
{
  tv->tv_sec = 0;
  switch (which)
    {
    case IO_POLL_DELAY:
      tv->tv_usec = 1;
      break;
    case TIMEVAL_SIGNAL_TIMEOUT:
      tv->tv_usec = 100000;
      break;
    case TIMEVAL_STROBE_DELAY:
      tv->tv_usec = 1;
      break;
    default:
      printf ("Couldn't lookup delay %d\n", which);
    }
  return;
}

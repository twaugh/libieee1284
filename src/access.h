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

#include <stdlib.h>
#ifndef _MSC_VER
#include <sys/time.h>
#else
#include <winsock2.h>
#endif
#include <sys/types.h>
#if defined __MINGW32__ || defined _MSC_VER
#include <sys/timeb.h>
#endif

#include "delay.h"

extern const struct parport_access_methods io_access_methods;
extern const struct parport_access_methods ppdev_access_methods;
extern const struct parport_access_methods lpt_access_methods;

#ifdef _MSC_VER
/* Visual C++ doesn't allow inline in C source code */
#define inline __inline
#endif

static inline void
delay (int which)
{
  struct timeval tv;
  lookup_delay (which, &tv);
#if !(defined __MINGW32__ || defined _MSC_VER)
  select (0, NULL, NULL, NULL, &tv);
#else
	{
		struct timeb tb;
		long int t0, t1;
	
		ftime(&tb);
		t0 = tb.time * 1000 + tb.millitm;
		do {
			ftime(&tb);
			t1 = tb.time * 1000 + tb.millitm;
		} while (t1 - t0 < (tv.tv_sec * 1000 + tv.tv_usec / 1000));
	}
#endif
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

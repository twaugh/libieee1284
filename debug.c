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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "debug.h"

#define ENVAR "LIBIEEE1284_DEBUG"

void dprintf (const char *fmt, ...)
{
  static int debugging_enabled = -1;
  if (!debugging_enabled)
    return;

  if (debugging_enabled == -1) {
    if (!getenv (ENVAR))
      {
	debugging_enabled = 0;
	return;
      }

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

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

#ifndef _DEBUG_H_
#define _DEBUG_H_

struct parport_internal;
extern void dprintf (const char *fmt, ...);
extern unsigned char debug_display_status (unsigned char st);
extern unsigned char debug_display_control (unsigned char ct);
extern void debug_frob_control (unsigned char mask, unsigned char val);

#endif /* _DEBUG_H_ */

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

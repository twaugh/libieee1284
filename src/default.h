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

#include "detect.h"

extern int default_wait_data (struct parport_internal *port,
			      unsigned char mask, unsigned char val,
			      struct timeval *timeout);
extern int default_do_nack_handshake (struct parport_internal *port,
				      unsigned char ct_before,
				      unsigned char ct_after,
				      struct timeval *timeout);
extern int default_negotiate (struct parport_internal *port, int mode);
extern void default_terminate (struct parport_internal *port);
extern int default_ecp_fwd_to_rev (struct parport_internal *port);
extern int default_ecp_rev_to_fwd (struct parport_internal *port);
extern ssize_t default_nibble_read (struct parport_internal *port, int flags,
				    char *buffer, size_t len);
extern ssize_t default_compat_write (struct parport_internal *port, int flags,
				     const char *buffer, size_t len);
extern ssize_t default_byte_read (struct parport_internal *port, int flags,
				  char *buffer, size_t len);
extern ssize_t default_epp_read_data (struct parport_internal *port,
				      int flags, char *buffer, size_t len);
extern ssize_t default_epp_write_data (struct parport_internal *port,
				       int flags, const char *buffer,
				       size_t len);
extern ssize_t default_epp_read_addr (struct parport_internal *port,
				      int flags, char *buffer, size_t len);
extern ssize_t default_epp_write_addr (struct parport_internal *port,
				       int flags, const char *buffer,
				       size_t len);
extern ssize_t default_ecp_read_data (struct parport_internal *port,
				      int flags, char *buffer, size_t len);
extern ssize_t default_ecp_write_data (struct parport_internal *port,
				       int flags, const char *buffer,
				       size_t len);
extern ssize_t default_ecp_read_addr (struct parport_internal *port,
				      int flags, char *buffer, size_t len);
extern ssize_t default_ecp_write_addr (struct parport_internal *port,
				       int flags, const char *buffer,
				       size_t len);
extern struct timeval *default_set_timeout (struct parport_internal *port,
					    struct timeval *timeout);

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

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

#include <sys/types.h> // for size_t

// A parallel port.  You only need the name from this.
struct parport {
  char *name;

  // For internal use only:
  void *priv;
};

// Some parallel ports.
struct parport_list {
  int portc;
  struct parport **portv;
};

extern int ieee1284_find_ports (struct parport_list *list, int flags);
// No flags defined yet.

extern void ieee1284_free_ports (struct parport_list *list);

//
// Retrieving the Device ID of a device on a port.
//

#define F1284_FRESH	(1<<0) // Guarantee a fresh Device ID

extern ssize_t ieee1284_get_deviceid (struct parport *port, int daisy,
				      int flags, char *buffer, size_t len);
// daisy is the daisy chain address (0-3), or -1 for normal IEEE 1284.

//
// Sharing hooks
//

extern int ieee1284_claim (struct parport *port);
// Must be called before any function below

extern void ieee1284_release (struct parport *port);

//
// Raw port access (PC-style port registers)
//

extern unsigned char ieee1284_read_data (struct parport *port);
extern void ieee1284_write_data (struct parport *port, unsigned char st);
extern void ieee1284_data_dir (struct parport *port, int reverse);

extern unsigned char ieee1284_read_status (struct parport *port);

extern unsigned char ieee1284_read_control (struct parport *port);
// May be unreliable

extern void ieee1284_write_control (struct parport *port, unsigned char ct);
extern void ieee1284_frob_control (struct parport *port, unsigned char mask,
				   unsigned char val);
// frob is out ((in & ~mask) | val)

//
// IEEE 1284 operations
//

extern int ieee1284_negotiate (struct parport *port, int mode);
extern void ieee1284_terminate (struct parport *port);

extern ssize_t ieee1284_nibble_read (struct parport *port, char *buffer,
				     size_t len);
extern ssize_t ieee1284_compat_write (struct parport *port,
				      const char *buffer, size_t len);
extern ssize_t ieee1284_byte_read (struct parport *port, char *buffer,
				   size_t len);
extern ssize_t ieee1284_epp_read_data (struct parport *port, char *buffer,
				       size_t len);
extern ssize_t ieee1284_epp_write_data (struct parport *port,
					const char *buffer, size_t len);
extern ssize_t ieee1284_epp_read_addr (struct parport *port, char *buffer,
				       size_t len);
extern ssize_t ieee1284_epp_write_addr (struct parport *port,
					const char *buffer, size_t len);
extern ssize_t ieee1284_ecp_read_data (struct parport *port, char *buffer,
				       size_t len);
extern ssize_t ieee1284_ecp_write_data (struct parport *port,
					const char *buffer, size_t len);
extern ssize_t ieee1284_ecp_read_data (struct parport *port, char *buffer,
				       size_t len);

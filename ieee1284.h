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

// A parallel port.
struct parport {
  // An arbitrary name for the port
  const char *name;

  // A set of bits indicating the capabilities of the port that we
  // can use.  Whether hardware-assisted ECP is available; whether
  // that includes DMA, etc.
  int modes;

  // -1, or else a file descriptor that can be used by select() for
  // waiting for nFault.
  int selectable_fd;

  // The base address of the port, if that has any meaning, or zero.
  unsigned long base_addr;

  // The ECR address of the port, if that has any meaning, or zero.
  unsigned long hibase_addr;

  // For internal use only:
  void *priv;
};

// Some parallel ports.
struct parport_list {
  int portc;
  struct parport **portv;
};

// The first function to be called.  This gives the library a chance
// to look around and see what's available, and gives the program a
// chance to choose a port to use.
extern int ieee1284_find_ports (struct parport_list *list,
                                const char *config_file, int flags);
// No flags defined yet.  config_file may be NULL, but otherwise tells
// the library where its configuration file is. (No configuration
// options defined yet, but to include base addresses, access
// methods, timings and such.)
// Returns 0, or an error code.  No errors defined yet.

// The last function to be called.  After calling this, only
// ieee1284_find_ports may be used.
extern void ieee1284_free_ports (struct parport_list *list);

//
// Retrieving the Device ID of a device on a port.
// This is a special operation since there are some shortcuts on some
// operating systems (i.e. Linux) that allow us to elide any actual
// communications.
//

#define F1284_FRESH	(1<<0) // Guarantee a fresh Device ID

extern ssize_t ieee1284_get_deviceid (struct parport *port, int daisy,
				      int flags, char *buffer, size_t len);
// daisy is the daisy chain address (0-3), or -1 for normal IEEE 1284.

//
// Sharing hooks
//

extern int ieee1284_claim (struct parport *port);
// Must be called before any function below.  May fail?

extern void ieee1284_release (struct parport *port);

//
// Raw port access (PC-style port registers)
// Functions returning int may fail.
//

extern int ieee1284_read_data (struct parport *port);
extern void ieee1284_write_data (struct parport *port, unsigned char st);
extern void ieee1284_data_dir (struct parport *port, int reverse);

// The status pin functions operate in terms of these bits:
static const int STATUS_NFAULT          =0x08;
static const int STATUS_SELECT          =0x10;
static const int STATUS_PERROR          =0x20;
static const int STATUS_NACK            =0x40;
static const int STATUS_BUSY            =0x80;
// To convert those values into PC-style register values, use this:
#define STATUS_INVERTED (STATUS_BUSY)

extern int ieee1284_read_status (struct parport *port);

// Wait until those status pins in mask have the values in val.
// Return 0 when condition met, 1 on timeout.
// timeout may be modified.
extern int ieee1284_wait_status (struct parport *port,
                                 unsigned char mask,
				 unsigned char val,
				 struct timeval *timeout);

// The control pin functions operate in terms of these bits:
static const int CONTROL_NSTROBE        =0x01;
static const int CONTROL_NAUTOFD        =0x02;
static const int CONTROL_NINIT          =0x04;
static const int CONTROL_NSELECTIN      =0x08;
// To convert those values into PC-style register values, use this:
#define CONTROL_INVERTED (CONTROL_NSTROBE|	\
                          CONTROL_NAUTOFD|	\
                          CONTROL_NSELECTIN)

extern int ieee1284_read_control (struct parport *port);
// ieee1284_read_control may be unreliable

extern void ieee1284_write_control (struct parport *port, unsigned char ct);
// NOTE: This will not change the direction of the data lines; use
// ieee1284_data_dir for that.

extern void ieee1284_frob_control (struct parport *port, unsigned char mask,
				   unsigned char val);
// frob is "out ((in & ~mask) | val)"

// This function may or may not be available, depending on PPWCTLONIRQ
// availability.  Its operation is:
// If operation unavailable, return -1.  Otherwise:
// Set control pins to ct_before.
// Wait for nFault interrupt.  If timeout elapses, return 1.
// Otherwise, set control pins to ct_after and return 0.
// timeout may be modified.
extern int ieee1284_do_nfault_handshake (struct parport *port,
                                         unsigned char ct_before,
                                         unsigned char ct_after,
                                         struct timeval *timeout);

//
// IEEE 1284 operations
//

// Negotiation/termination
extern int ieee1284_negotiate (struct parport *port, int mode);
extern void ieee1284_terminate (struct parport *port);

// ECP direction switching
extern int ieee1284_ecp_fwd_to_rev (struct parport *port);
extern int ieee1284_ecp_rev_to_fwd (struct parport *port);

// Block I/O
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

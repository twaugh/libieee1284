/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2001, 2002, 2003  Tim Waugh <twaugh@redhat.com>
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

#ifndef HAVE_IEEE1284_H
#define HAVE_IEEE1284_H

#include <sys/types.h> /* for size_t */
#ifndef _MSC_VER
#include <sys/time.h> /* for struct timeval */
#else
#include <winsock2.h> /* for struct timeval */
#endif

#if (defined __MINGW32__ || defined _MSC_VER) && !defined OWN_SSIZE_T
#include <basetsd.h> /* for SSIZE_T */
#define OWN_SSIZE_T
typedef SSIZE_T ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Errors.  When a function returns a negative number, it's one of
 * these errors. */
enum E1284 {
  E1284_OK                 = 0,   /* Everything went fine */
  E1284_NOTIMPL            = -1,  /* Not implemented in libieee1284 */
  E1284_NOTAVAIL           = -2,  /* Not available on this system */
  E1284_TIMEDOUT           = -3,  /* Operation timed out */
  E1284_REJECTED           = -4,  /* IEEE 1284 negotiation rejected */
  E1284_NEGFAILED          = -5,  /* Negotiation went wrong */
  E1284_NOMEM              = -6,  /* No memory left */
  E1284_INIT               = -7,  /* Error initialising port */
  E1284_SYS                = -8,  /* Error interfacing system */
  E1284_NOID               = -9,  /* No IEEE 1284 ID available */
  E1284_INVALIDPORT        = -10  /* Invalid port */
};

/* A parallel port. */
struct parport {
  /* An arbitrary name for the port */
  const char *name;

  /* The base address of the port, if that has any meaning, or zero. */
  unsigned long base_addr;

  /* The ECR address of the port, if that has any meaning, or zero. */
  unsigned long hibase_addr;

  /* For internal use only: */
  void *priv;

  /* The filename associated with this port,
   * if that has any meaning, or NULL. */
  const char *filename;
};

/* Some parallel ports. */
struct parport_list {
  int portc;
  struct parport **portv;
};

/* The first function to be called.  This gives the library a chance
 * to look around and see what's available, and gives the program a
 * chance to choose a port to use. */
extern int ieee1284_find_ports (struct parport_list *list, int flags);

/* The last function to be called.  After calling this, only
 * ieee1284_find_ports may be used. */
extern void ieee1284_free_ports (struct parport_list *list);

/*
 * Retrieving the Device ID of a device on a port.
 * This is a special operation since there are some shortcuts on some
 * operating systems (i.e. Linux) that allow us to elide any actual
 * communications.
 */

enum ieee1284_devid_flags
{
  F1284_FRESH = (1<<1)  /* Guarantee a fresh Device ID */
};

extern ssize_t ieee1284_get_deviceid (struct parport *port, int daisy,
				      int flags, char *buffer, size_t len);
/* daisy is the daisy chain address (0-3), or -1 for normal IEEE 1284. */

/*
 * Sharing hooks
 */

enum ieee1284_open_flags
{
  F1284_EXCL = (1<<0)  /* Require exclusive access to the port */
};
enum ieee1284_capabilities
{
  CAP1284_RAW = (1<<0),  /* Pin-level access */
  CAP1284_NIBBLE = (1<<1),
  CAP1284_BYTE = (1<<2),
  CAP1284_COMPAT = (1<<3),
  CAP1284_BECP = (1<<4),
  CAP1284_ECP = (1<<5),
  CAP1284_ECPRLE = (1<<6),
  CAP1284_ECPSWE = (1<<7),
  CAP1284_EPP = (1<<8),
  CAP1284_EPPSL = (1<<9),
  CAP1284_EPPSWE = (1<<10),
  CAP1284_IRQ = (1<<11),
  CAP1284_DMA = (1<<12)
};
extern int ieee1284_open (struct parport *port, int flags, int *capabilities);

extern int ieee1284_close (struct parport *port);

extern int ieee1284_ref (struct parport *port);
extern int ieee1284_unref (struct parport *port);

extern int ieee1284_claim (struct parport *port);
/* Must be called before any function below.  May fail. */

extern void ieee1284_release (struct parport *port);

/*
 * Interrupt notification
 */
extern int ieee1284_get_irq_fd (struct parport *port);
extern int ieee1284_clear_irq (struct parport *port, unsigned int *count);

/*
 * Raw port access (PC-style port registers but within inversions)
 * Functions returning int may fail.
 */

extern int ieee1284_read_data (struct parport *port);
extern void ieee1284_write_data (struct parport *port, unsigned char dt);
extern int ieee1284_wait_data (struct parport *port, unsigned char mask,
			       unsigned char val, struct timeval *timeout);
extern int ieee1284_data_dir (struct parport *port, int reverse);

/* The status pin functions operate in terms of these bits: */
enum ieee1284_status_bits
{
  S1284_NFAULT = 0x08,
  S1284_SELECT = 0x10,
  S1284_PERROR = 0x20,
  S1284_NACK   = 0x40,
  S1284_BUSY   = 0x80,
  /* To convert those values into PC-style register values, use this: */
  S1284_INVERTED = S1284_BUSY
};

extern int ieee1284_read_status (struct parport *port);

/* Wait until those status pins in mask have the values in val.
 * Return E1284_OK when condition met, E1284_TIMEDOUT on timeout.
 * timeout may be modified. */
extern int ieee1284_wait_status (struct parport *port,
                                 unsigned char mask,
				 unsigned char val,
				 struct timeval *timeout);

/* The control pin functions operate in terms of these bits: */
enum ieee1284_control_bits
{
  C1284_NSTROBE   = 0x01,
  C1284_NAUTOFD   = 0x02,
  C1284_NINIT     = 0x04,
  C1284_NSELECTIN = 0x08,
  /* To convert those values into PC-style register values, use this: */
  C1284_INVERTED = (C1284_NSTROBE|
		    C1284_NAUTOFD|
		    C1284_NSELECTIN)
};

extern int ieee1284_read_control (struct parport *port);
/* ieee1284_read_control may be unreliable */

extern void ieee1284_write_control (struct parport *port, unsigned char ct);
/* NOTE: This will not change the direction of the data lines; use
 * ieee1284_data_dir for that. */

extern void ieee1284_frob_control (struct parport *port, unsigned char mask,
				   unsigned char val);
/* frob is "out ((in & ~mask) ^ val)" */

/* This function may or may not be available, depending on PPWCTLONIRQ
 * availability.  Its operation is:
 * If operation unavailable, return E1284_NOTAVAIL.  Otherwise:
 * Set control pins to ct_before.
 * Wait for nAck interrupt.  If timeout elapses, return E1284_TIMEDOUT.
 * Otherwise, set control pins to ct_after and return 0.
 * timeout may be modified. */
extern int ieee1284_do_nack_handshake (struct parport *port,
				       unsigned char ct_before,
				       unsigned char ct_after,
				       struct timeval *timeout);

/*
 * IEEE 1284 operations
 */

/* Negotiation/termination */
enum ieee1284_modes
{
  M1284_NIBBLE =  0,
  M1284_BYTE   = (1<<0),
  M1284_COMPAT = (1<<8),
  M1284_BECP   = (1<<9),
  M1284_ECP    = (1<<4),
  M1284_ECPRLE = ((1<<4) | (1<<5)),
  M1284_ECPSWE = (1<<10), /* Software emulated */
  M1284_EPP    = (1<<6),
  M1284_EPPSL  = (1<<11), /* EPP 1.7 */
  M1284_EPPSWE = (1<<12), /* Software emulated */
  M1284_FLAG_DEVICEID = (1<<2),
  M1284_FLAG_EXT_LINK = (1<<14)  /* Uses bits in 0x7f */
};

extern int ieee1284_negotiate (struct parport *port, int mode);
extern void ieee1284_terminate (struct parport *port);

/* ECP direction switching */
extern int ieee1284_ecp_fwd_to_rev (struct parport *port);
extern int ieee1284_ecp_rev_to_fwd (struct parport *port);

/* Block I/O
 * The return value is the number of bytes successfully transferred,
 * or an error code (only if no transfer took place). */
enum ieee1284_transfer_flags
{
  F1284_NONBLOCK = (1<<0),	/* Non-blocking semantics */
  F1284_SWE = (1<<2),		/* Don't use hardware assistance */
  F1284_RLE = (1<<3),		/* Use ECP RLE */
  F1284_FASTEPP = (1<<4)	/* Use faster EPP (counts are unreliable) */
};
extern ssize_t ieee1284_nibble_read (struct parport *port, int flags,
				     char *buffer, size_t len);
extern ssize_t ieee1284_compat_write (struct parport *port, int flags,
				      const char *buffer, size_t len);
extern ssize_t ieee1284_byte_read (struct parport *port, int flags,
				   char *buffer, size_t len);
extern ssize_t ieee1284_epp_read_data (struct parport *port, int flags,
				       char *buffer, size_t len);
extern ssize_t ieee1284_epp_write_data (struct parport *port, int flags,
					const char *buffer, size_t len);
extern ssize_t ieee1284_epp_read_addr (struct parport *port, int flags,
				       char *buffer, size_t len);
extern ssize_t ieee1284_epp_write_addr (struct parport *port, int flags,
					const char *buffer, size_t len);
extern ssize_t ieee1284_ecp_read_data (struct parport *port, int flags,
				       char *buffer, size_t len);
extern ssize_t ieee1284_ecp_write_data (struct parport *port, int flags,
					const char *buffer, size_t len);
extern ssize_t ieee1284_ecp_read_addr (struct parport *port, int flags,
				       char *buffer, size_t len);
extern ssize_t ieee1284_ecp_write_addr (struct parport *port, int flags,
					const char *buffer, size_t len);
extern struct timeval *ieee1284_set_timeout (struct parport *port,
					     struct timeval *timeout);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HAVE_IEEE1284_H */

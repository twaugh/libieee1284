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

#include "ieee1284.h"
#include "debug.h"
#include "detect.h"

/* ieee1284_open is in state.c */

static const char *needs_open_port = \
"%s called for port that wasn't opened (use ieee1284_open first)\n";
static const char *needs_claimed_port = \
"%s called for port that wasn't claimed (use ieee1284_claim first)\n";

int
ieee1284_ref (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  return ++priv->ref;
}

int
ieee1284_unref (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  if (priv->opened && priv->ref == 1)
    {
      int ret;
      debugprintf ("ieee1284_unref called for last reference to open port!\n");
      ret = ieee1284_close (port);
      if (ret == E1284_OK)
	return 0;

      return 1;
    }

  return deref_port (port);
}

int
ieee1284_close (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  if (!priv->opened)
    {
      debugprintf (needs_open_port, "ieee1284_close");
      return E1284_INVALIDPORT;
    }
  if (priv->fn->cleanup)
    priv->fn->cleanup (priv);
  priv->opened = 0;
  deref_port (port);
  return E1284_OK;
}

int
ieee1284_claim (struct parport *port)
{
  int ret = E1284_OK;
  struct parport_internal *priv = port->priv;

  if (!priv->opened)
    {
      debugprintf (needs_open_port, "ieee1284_claim");
      return E1284_INVALIDPORT;
    }

  if (priv->claimed)
    {
      debugprintf ("ieee1284_claim called for a port already claimed\n");
      return E1284_INVALIDPORT;
    }

  if (priv->fn->claim)
    ret = priv->fn->claim (priv);

  if (ret == E1284_OK)
    priv->claimed = 1;

  return ret;
}

int
ieee1284_get_irq_fd (struct parport *port)
{
  int ret = E1284_NOTAVAIL;
  struct parport_internal *priv = port->priv;

  if (!priv->opened)
    {
      debugprintf (needs_open_port, "ieee1284_get_irq_fd");
      return E1284_INVALIDPORT;
    }

  if (priv->fn->get_irq_fd)
    ret = priv->fn->get_irq_fd (priv);
  return ret;
}

int
ieee1284_clear_irq (struct parport *port, unsigned int *count)
{
  int ret = E1284_NOTAVAIL;
  struct parport_internal *priv = port->priv;

  if (priv->fn->clear_irq)
    {
      if (!priv->claimed)
	{
	  debugprintf (needs_claimed_port, "ieee1284_clear_irq");
	  return E1284_INVALIDPORT;
	}

      ret = priv->fn->clear_irq (priv, count);
    }

  return ret;
}

void
ieee1284_release (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  if (priv->claimed && priv->fn->release)
    priv->fn->release (priv);
  priv->claimed = 0;
}

int
ieee1284_read_data (struct parport *port)
{
  int ret = -E1284_NOTAVAIL;
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_read_data");
      return E1284_INVALIDPORT;
    }

  if (priv->fn->read_data)
    ret = priv->fn->read_data (priv);

  return ret;
}

void
ieee1284_write_data (struct parport *port, unsigned char st)
{
  struct parport_internal *priv = port->priv;
  if (priv->claimed)
    priv->fn->write_data (priv, st);
  else
    debugprintf (needs_claimed_port, "ieee1284_write_data");
}

int
ieee1284_wait_data (struct parport *port,
		    unsigned char mask,
		    unsigned char val,
		    struct timeval *timeout)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_wait_data");
      return E1284_INVALIDPORT;
    }

  return priv->fn->wait_data (priv, mask, val, timeout);
}

int
ieee1284_data_dir (struct parport *port, int reverse)
{
  int ret = E1284_NOTAVAIL;
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_data_dir");
      return E1284_INVALIDPORT;
    }

  if (priv->fn->data_dir)
    ret = priv->fn->data_dir (priv, reverse);

  return ret;
}

int
ieee1284_read_status (struct parport *port)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_read_status");
      return E1284_INVALIDPORT;
    }

  return priv->fn->read_status (priv);
}

int
ieee1284_wait_status (struct parport *port,
		      unsigned char mask,
		      unsigned char val,
		      struct timeval *timeout)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_wait_status");
      return E1284_INVALIDPORT;
    }

  return priv->fn->wait_status (priv, mask, val, timeout);
}

int
ieee1284_read_control (struct parport *port)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_read_control");
      return E1284_INVALIDPORT;
    }

  return priv->fn->read_control (priv);
}

void
ieee1284_write_control (struct parport *port, unsigned char ct)
{
  struct parport_internal *priv = port->priv;
  if (priv->claimed)
    priv->fn->write_control (priv, ct);
  else
    debugprintf (needs_claimed_port, "ieee1284_write_control");
}

void
ieee1284_frob_control (struct parport *port, unsigned char mask,
		       unsigned char val)
{
  struct parport_internal *priv = port->priv;

  if (priv->claimed)
    priv->fn->frob_control (priv, mask, val);
  else
    debugprintf (needs_claimed_port, "ieee1284_frob_control");
}

int
ieee1284_do_nack_handshake (struct parport *port,
			    unsigned char ct_before,
			    unsigned char ct_after,
			    struct timeval *timeout)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_do_nack_handshake");
      return E1284_INVALIDPORT;
    }

  return priv->fn->do_nack_handshake (priv, ct_before, ct_after, timeout);
}

int
ieee1284_negotiate (struct parport *port, int mode)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_negotiate");
      return E1284_INVALIDPORT;
    }

  return priv->fn->negotiate (priv, mode);
}

void
ieee1284_terminate (struct parport *port)
{
  struct parport_internal *priv = port->priv;
  if (priv->claimed)
    priv->fn->terminate (priv);
  else
    debugprintf (needs_claimed_port, "ieee1284_terminate");
}

int
ieee1284_ecp_fwd_to_rev (struct parport *port)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_ecp_fwd_to_rev");
      return E1284_INVALIDPORT;
    }

  return priv->fn->ecp_fwd_to_rev (priv);
}

int
ieee1284_ecp_rev_to_fwd (struct parport *port)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_ecp_rev_to_fwd");
      return E1284_INVALIDPORT;
    }

  return priv->fn->ecp_rev_to_fwd (priv);
}

ssize_t
ieee1284_nibble_read (struct parport *port, int flags,
		      char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_nibble_read");
      return E1284_INVALIDPORT;
    }

  return priv->fn->nibble_read (priv, flags, buffer, len);
}

ssize_t
ieee1284_compat_write (struct parport *port, int flags,
		       const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_compat_write");
      return E1284_INVALIDPORT;
    }

  return priv->fn->compat_write (priv, flags, buffer, len);
}

ssize_t
ieee1284_byte_read (struct parport *port, int flags, 
		    char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_byte_read");
      return E1284_INVALIDPORT;
    }

  return priv->fn->byte_read (priv, flags, buffer, len);
}

ssize_t
ieee1284_epp_read_data (struct parport *port, int flags, char *buffer,
			size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_epp_read_data");
      return E1284_INVALIDPORT;
    }

  return priv->fn->epp_read_data (priv, flags, buffer, len);
}

ssize_t
ieee1284_epp_write_data (struct parport *port, int flags,
			 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_epp_write_data");
      return E1284_INVALIDPORT;
    }

  return priv->fn->epp_write_data (priv, flags, buffer, len);
}

ssize_t
ieee1284_epp_read_addr (struct parport *port, int flags, char *buffer,
			size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_epp_read_addr");
      return E1284_INVALIDPORT;
    }

  return priv->fn->epp_read_addr (priv, flags, buffer, len);
}

ssize_t
ieee1284_epp_write_addr (struct parport *port, int flags,
			 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_epp_write_addr");
      return E1284_INVALIDPORT;
    }

  return priv->fn->epp_write_addr (priv, flags, buffer, len);
}

ssize_t
ieee1284_ecp_read_data (struct parport *port, int flags, char *buffer,
			size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_ecp_read_data");
      return E1284_INVALIDPORT;
    }

  return priv->fn->ecp_read_data (priv, flags, buffer, len);
}

ssize_t
ieee1284_ecp_write_data (struct parport *port, int flags,
			 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_ecp_write_data");
      return E1284_INVALIDPORT;
    }

  return priv->fn->ecp_write_data (priv, flags, buffer, len);
}

ssize_t
ieee1284_ecp_read_addr (struct parport *port, int flags,
			char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_ecp_read_addr");
      return E1284_INVALIDPORT;
    }

  return priv->fn->ecp_read_addr (priv, flags, buffer, len);
}

ssize_t
ieee1284_ecp_write_addr (struct parport *port, int flags,
			 const char *buffer, size_t len)
{
  struct parport_internal *priv = port->priv;

  if (!priv->claimed)
    {
      debugprintf (needs_claimed_port, "ieee1284_ecp_write_addr");
      return E1284_INVALIDPORT;
    }

  return priv->fn->ecp_write_addr (priv, flags, buffer, len);
}

struct timeval *
ieee1284_set_timeout (struct parport *port, struct timeval *timeout)
{
  struct parport_internal *priv = port->priv;
  return priv->fn->set_timeout (priv, timeout);
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

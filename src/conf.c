/*
 * libieee1284 - IEEE 1284 library
 * Copyright (C) 2002  Tim Waugh <twaugh@redhat.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "conf.h"
#include "debug.h"

struct config_variables conf;

static const char *const ieee1284conf = "ieee1284.conf";
static const size_t max_line_len = 1000;
static const char *ifs = " \t\n";
static const char *tokenchar = "{}=";

/* Get the next token.  Caller frees returned zero-terminated string. */
static char *
get_token (FILE *f)
{
  static char *current_line = NULL;
  static size_t current_line_len = 0;
  static size_t at = 0;
  size_t end, i;
  char *this_token;

  for (;;)
    {
      int quotes = 0;

      if (at == current_line_len)
	{
	  if (current_line)
	    free (current_line);

	  current_line = NULL;
	  current_line_len = 0;
	  at = 0;
	}

      if (!current_line)
	{
	  current_line = malloc (sizeof (char) * max_line_len);
	  if (!current_line)
	    return NULL;

	  /* Ideally we'd use getline here, but that isn't available
	   * everywhere. In fact, *ideally* we'd use wordexp for this
	   * whole function, but that isn't widely available either. */
	  if (!fgets (current_line, max_line_len, f))
	    {
	      free (current_line);
	      current_line = NULL;
	      current_line_len = 0;
	      at = 0;
	      return NULL;
	    }

	  current_line_len = strlen (current_line);
	  at = 0;
	}

      /* Skip whitespace. */
      at += strspn (current_line + at, ifs);

      if (current_line[at] == '#')
	{
	  /* Ignore the rest of the line. */
	  at = current_line_len;
	  continue;
	}

      /* Find the end of the token. */
      for (end = at; end < current_line_len; end++)
	{
	  char ch = current_line[end];

	  if (ch == '\\' && quotes != 1)
	    {
	      end++;
	      continue;
	    }

	  if (ch == '\'')
	    {
	      if (quotes == 0)
		{
		  quotes = 1;
		  continue;
		}

	      if (quotes == 1)
		quotes = 0;
	    }

	  if (ch == '"')
	    {
	      if (quotes == 0)
		{
		  quotes = 2;
		  continue;
		}

	      if (quotes == 2)
		quotes = 0;
	    }

	  if (!quotes && strchr (ifs, ch))
	    break;

	  if (!quotes && strchr (tokenchar, ch))
	    {
	      if (end == at)
		end++;

	      break;
	    }
	}

      if (at == end)
	/* Next line. */
	continue;

      /* Copy this token. */
      this_token = malloc (sizeof (char) * (end - at + 1)); /* worst case */
      if (!this_token)
	return NULL;

      quotes = 0;
      for (i = 0; at < end; at++)
	{
	  char ch = current_line[at];

	  if (ch == '\\' && quotes != 1)
	    {
	      if (at < end - 1)
		this_token[i++] = current_line[++at];

	      continue;
	    }

	  if (ch == '\'')
	    {
	      if (quotes == 0)
		{
		  quotes = 1;
		  continue;
		}

	      if (quotes == 1)
		{
		  quotes = 0;
		  continue;
		}
	    }

	  if (ch == '"')
	    {
	      if (quotes == 0)
		{
		  quotes = 2;
		  continue;
		}

	      if (quotes == 2)
		{
		  quotes = 0;
		  continue;
		}
	    }

	  this_token[i++] = ch;
	}

      this_token[i] = '\0';
      break;
    }

  return this_token;
}

static char *
disallow (FILE *f)
{
  char *token = NULL;

  token = get_token (f);
  if (!token || strcmp (token, "method"))
    {
      debugprintf ("'disallow' requires 'method'\n");
      return token;
    }

  free (token);
  token = get_token (f);
  if (!token || strcmp (token, "ppdev"))
    {
      debugprintf ("'disallow method' requires a method name (e.g. ppdev)\n");
      return token;
    }

  debugprintf ("* Disallowing method: ppdev\n");
  conf.disallow_ppdev = 1;
  free (token);
  return get_token (f);
}

static int
try_read_config_file (const char *path)
{
  FILE *f = fopen (path, "r");
  char *token;

  if (!f)
    return 1;

  debugprintf ("Reading configuration from %s:\n", path);

  token = get_token (f);
  while (token)
    {
      char *next_token;
      if (!strcmp (token, "disallow"))
	{
	  next_token = disallow (f);
	}
      else
	{
	  debugprintf ("Skipping unknown word: %s\n", token);
	  next_token = get_token (f);
	}

      free (token);
      token = next_token;
    }

  fclose (f);
  debugprintf ("End of configuration\n");
  return 0;
}

void
read_config_file (void)
{
  static int config_read = 0;
  size_t rclen;
  char *path;

  if (config_read)
    return;

  conf.disallow_ppdev = 0;

  rclen = strlen (ieee1284conf);
  path = malloc (1 + 5 + rclen);
  if (!path)
    return;

  memcpy (path, "/etc/", 5);
  memcpy (path + 5, ieee1284conf, rclen + 1);
  if (try_read_config_file (path))
    config_read = 1;

  free (path);
  return;
}

/*
 * Local Variables:
 * eval: (c-set-style "gnu")
 * End:
 */

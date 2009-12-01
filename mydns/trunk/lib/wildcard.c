/**************************************************************************************************
	$Id: wildcard.c,v 1.7 2005/04/20 16:49:11 bboy Exp $

	wildcard.c: Wildcard matching routines.

	Originally authored and placed in the public domain by J. Kercheval.

	Copyright (C) 2002-2005  Don Moore <bboy@bboy.net>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at Your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**************************************************************************************************/

#include "mydnsutil.h"

#define _WC_MATCH_PATTERN  6    /* bad pattern */
#define _WC_MATCH_LITERAL  5    /* match failure on literal match */
#define _WC_MATCH_RANGE    4    /* match failure on [..] construct */
#define _WC_MATCH_ABORT    3    /* premature end of text string */
#define _WC_MATCH_END      2    /* premature end of pattern string */
#define _WC_MATCH_VALID    1    /* valid match */


/**************************************************************************************************
	WILDCARD_VALID
	Returns 1 if the specified string is valid, 0 if it contains a syntax error.
**************************************************************************************************/
int
wildcard_valid(char *p) {
  /* loop through pattern to EOS */
  while (*p) {
    switch (*p) {
    case '\\':
      if (!*++p)
	return 0;
      p++;
      break;

    case '[':
      p++;
      if (*p == ']')
	return 0;
      if (!*p)
	return 0;

      while (*p != ']') {
	if (*p == '\\')	{
	  p++;

	  if (!*p++)
	    return 0;
	} else
	  p++;

	if (!*p)
	  return 0;

	if (*p == '-') {
	  if (!*++p || *p == ']')
	    return 0;
	  else {
	    if (*p == '\\')
	      p++;
	    if (!*p++)
	      return 0;
	  }
	}
      }
      break;

    case '*':
    case '?':
    default:
      p++;
      break;
    }
  }
  return 1;
}
/*--- wildcard_valid() --------------------------------------------------------------------------*/


/**************************************************************************************************
	MATCH_AFTER_STAR
	Recursively call wildcard_match() with final segment of PATTERN and of TEXT.
**************************************************************************************************/
static int
match_after_star(register char *p, register char *t) {
  register int match = 0, nextp;

  while (*p == '?' || *p == '*') {
    if (*p == '?') {
      if (!*t++)
	return _WC_MATCH_ABORT;
    }
    p++;
  }

  if (!*p)
    return _WC_MATCH_VALID;

  nextp = *p;
  if (nextp == '\\') {
    nextp = p[1];

    if (!nextp)
      return _WC_MATCH_PATTERN;
  }

  do {
    if (nextp == *t || nextp == '[')
      match = wildcard_match(p, t);

    if (!*t++)
      match = _WC_MATCH_ABORT;
  } while (match != _WC_MATCH_VALID && match != _WC_MATCH_ABORT && match != _WC_MATCH_PATTERN);

  return match;
}
/*--- match_after_star() ------------------------------------------------------------------------*/


/**************************************************************************************************
	_WILDCARD_MATCH
	Match pattern (p) against text (t).
**************************************************************************************************/
static int
_wildcard_match(register char *p, register char *t) {
  register char range_start, range_end;		/* start and end in range */

  int invert;					/* is this [..] or [!..] */
  int member_match;				/* have I matched the [..] construct? */
  int loop;					/* should I terminate? */

  for (; *p; p++, t++) {
    /* if this is the end of the text
       then this is the end of the match */

    if (!*t) {
      return (*p == '*' && *++p == '\0') ? _WC_MATCH_VALID : _WC_MATCH_ABORT;
    }

    /* determine and react to pattern type */

    switch (*p) {
    case '?':		/* single any character match */
      break;

    case '*':		/* multiple any character match */
      return match_after_star(p, t);

      /* [..] construct, single member/exclusion character match */
    case '[':
      {
	/* move to beginning of range */

	p++;

	/* check if this is a member match or exclusion match */

	invert = 0;
	if (*p == '!' || *p == '^') {
	  invert = 1;
	  p++;
	}

	/* if closing bracket here or at range start then we have a
	   malformed pattern */

	if (*p == ']') {
	  return _WC_MATCH_PATTERN;
	}

	member_match = 0;
	loop = 1;

	while (loop) {
	  /* if end of construct then loop is done */

	  if (*p == ']') {
	    loop = 0;
	    continue;
	  }

	  /* matching a '!', '^', '-', '\' or a ']' */

	  if (*p == '\\') {
	    range_start = range_end = *++p;
	  } else
	    range_start = range_end = *p;

	  /* if end of pattern then bad pattern (Missing ']') */

	  if (!*p)
	    return _WC_MATCH_PATTERN;

	  /* check for range bar */
	  if (*++p == '-') {
	    /* get the range end */

	    range_end = *++p;

	    /* if end of pattern or construct
	       then bad pattern */

	    if (range_end == '\0' || range_end == ']')
	      return _WC_MATCH_PATTERN;

	    /* special character range end */
	    if (range_end == '\\') {
	      range_end = *++p;

	      /* if end of text then
		 we have a bad pattern */
	      if (!range_end)
		return _WC_MATCH_PATTERN;
	    }

	    /* move just beyond this range */
	    p++;
	  }

	  /* if the text character is in range then match found.
	     make sure the range letters have the proper
	     relationship to one another before comparison */

	  if (range_start < range_end) {
	    if (*t >= range_start && *t <= range_end) {
	      member_match = 1;
	      loop = 0;
	    }
	  } else {
	    if (*t >= range_end && *t <= range_start) {
	      member_match = 1;
	      loop = 0;
	    }
	  }
	}

	/* if there was a match in an exclusion set then no match */
	/* if there was no match in a member set then no match */

	if ((invert && member_match) || !(invert || member_match))
	  return _WC_MATCH_RANGE;

	/* if this is not an exclusion then skip the rest of
	   the [...] construct that already matched. */

	if (member_match) {
	  while (*p != ']') {
	    /* bad pattern (Missing ']') */
	    if (!*p)
	      return _WC_MATCH_PATTERN;

	    /* skip exact match */
	    if (*p == '\\') {
	      p++;

	      /* if end of text then
		 we have a bad pattern */

	      if (!*p)
		return _WC_MATCH_PATTERN;
	    }

	    /* move to next pattern char */

	    p++;
	  }
	}
	break;
      }
    case '\\':		/* next character is quoted and must match exactly */

      /* move pattern pointer to quoted char and fall through */

      p++;

      /* if end of text then we have a bad pattern */

      if (!*p)
	return _WC_MATCH_PATTERN;

      /* must match this character exactly */

    default:
      if (*p != *t)
	return _WC_MATCH_LITERAL;
    }
  }
  /* if end of text not reached then the pattern fails */

  if (*t)
    return _WC_MATCH_END;
  else
    return _WC_MATCH_VALID;
}
/*--- _wildcard_match() -------------------------------------------------------------------------*/


/**************************************************************************************************
	WILDCARD_MATCH
	Match pattern (p) against text (t).
**************************************************************************************************/
int
wildcard_match(register char *p, register char *t) {
  return (_wildcard_match(p, t) == _WC_MATCH_VALID);
}
/*--- wildcard_match() --------------------------------------------------------------------------*/

/* vi:set ts=3: */

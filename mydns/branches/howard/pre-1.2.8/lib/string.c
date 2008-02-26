/**************************************************************************************************
	$Id: string.c,v 1.22 2005/04/20 16:49:11 bboy Exp $

	string.c: Typical generic string manipulation routines.

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


/**************************************************************************************************
	STRTRIMLEAD
	Remove trailing spaces, etc.
**************************************************************************************************/
char *
strtrimlead(char *str) {
  char *obuf;

  if (str) {
    for (obuf = str; *obuf && isspace((int)(*obuf)); ++obuf)
      ;
    if (str != obuf)
      memmove(str, obuf, strlen(obuf) + 1);
  }
  return (str);
}
/*--- strtrimlead() -----------------------------------------------------------------------------*/


/**************************************************************************************************
	STRTRIMTRAIL
**************************************************************************************************/
char *
strtrimtrail(char *str) {
  int i;

  if (str && 0 != (i = strlen(str))) {
    while (--i >= 0) {
      if (!isspace((int)(str[i])))
	break;
    }
    str[++i] = '\0';
  }
  return (str);
}
/*--- strtrimtrail() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	STRTRIM
	Removes leading and trailing whitespace from a string.  Converts tabs and newlines to spaces.
**************************************************************************************************/
char *
strtrim(char *str) {
  strtrimlead(str);
  strtrimtrail(str);
  return (str);
}
/*--- strtrim() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	STRTOUPPER
	Converts a string to uppercase.
**************************************************************************************************/
char *
strtoupper(char *str) {
  register char *c;

  if (!str || !*str)
    return (NULL);
  for (c = str; *c; c++)
    *c = toupper(*c);
  return (str);
}  
/*--- strtoupper() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	STRTOLOWER
	Converts a string to lowercase.
**************************************************************************************************/
char *
strtolower(char *str) {
  register char *c;

  if (!str || !*str)
    return (NULL);
  for (c = str; *c; c++)
    *c = tolower(*c);
  return (str);
}  
/*--- strtolower() ------------------------------------------------------------------------------*/


/**************************************************************************************************
	STRSECS
	Outputs a number of seconds in a more human-friendly format.
**************************************************************************************************/
char *
strsecs(time_t seconds) {
  int weeks, days, hours, minutes;
  static char *str = NULL;
  char *weekstr = NULL, *daystr = NULL, *hourstr = NULL, *minutestr = NULL, *secondstr = NULL;

  weeks = seconds / 604800; seconds -= (weeks * 604800);
  days = seconds / 86400; seconds -= (days * 86400);
  hours = seconds / 3600; seconds -= (hours * 3600);
  minutes = seconds / 60; seconds -= (minutes * 60);

  if (weeks) { ASPRINTF(&weekstr, "%dw", weeks); }
  if (days) { ASPRINTF(&daystr, "%dd", days); }
  if (hours) { ASPRINTF(&hourstr, "%dh", hours); }
  if (minutes) { ASPRINTF(&minutestr, "%dm", minutes); }
  if (seconds) { ASPRINTF(&secondstr, "%ds", (int)seconds); }

  RELEASE(str);
  ASPRINTF(&str,
	   "%s%s%s%s%s",
	   (weekstr)?weekstr:"",
	   (daystr)?daystr:"",
	   (hourstr)?hourstr:"",
	   (minutestr)?minutestr:"",
	   (secondstr)?secondstr:(!(weekstr||daystr||hourstr||minutestr))?"0s":"");

  RELEASE(weekstr);
  RELEASE(daystr);
  RELEASE(hourstr);
  RELEASE(minutestr);
  RELEASE(secondstr);
  return (str);
}
/*--- strsecs() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	STRDCAT
	Dynamically-allocated strcat(3).
**************************************************************************************************/
char *
strdcat(char **dest, const char *src) {
  register int	srclen;							/* Length of src */
  register int	destlen;						/* Length of dest */
  char		*d = *dest;						/* Ptr to dest */

  /* If we pass a length of 0 to realloc, it frees memory: just return */
  if ((srclen = strlen(src)) == 0)
    return (d);
  destlen = (d) ? strlen(d) : 0;

  /* Allocate/reallocate the storage in dest */
  if (!d) {
    d = ALLOCATE(destlen + srclen + 1, char[]);
  } else {
    d = REALLOCATE(d, destlen + srclen + 1, char[]);
  }

  memcpy(d + destlen, src, srclen);
  d[destlen + srclen] = '\0';

  *dest = d;
  return (d);
}
/*--- strdcat() ---------------------------------------------------------------------------------*/


/**************************************************************************************************
	SDPRINTF
	Dynamically-allocated sprintf(3).
**************************************************************************************************/
int
sdprintf(char **dest, const char *fmt, ...) {
  va_list ap;
  int len;

  va_start(ap, fmt);
  len = VASPRINTF(dest, fmt, ap);
  va_end(ap);

  return (len);	
}
/*--- sdprintf() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	Given a string such as "10MB" returns the size represented, in bytes.
**************************************************************************************************/
size_t
human_file_size(const char *str) {
  size_t numeric = 0;						/* Numeric part of `str' */
  register char *c;						/* Ptr to first nonalpha char */

  numeric = (size_t)strtoul(str, (char **)NULL, 10);

  for (c = (char *)str; *c && isdigit((int)(*c)); c++)
    /* DONOTHING */;

  if (!*c)
    return (numeric);

  switch (tolower(*c)) {
  case 'k': return (numeric * 1024);
  case 'm': return (numeric * 1048576);
  case 'g': return (numeric * 1073741824);
  default:
    break;
  }
  return (numeric);
}
/*--- human_file_size() -------------------------------------------------------------------------*/

/* vi:set ts=3: */

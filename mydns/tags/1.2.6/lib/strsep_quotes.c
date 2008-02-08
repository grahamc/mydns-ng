/**************************************************************************************************
	$Id: strsep_quotes.c,v 1.1 2005/04/28 18:43:14 bboy Exp $

	strsep_quotes.c: strsep()-style function that obeys quoted strings.

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
	STRSEP_QUOTES
	Like strsep(), but considers strings inside quotes to be single tokens, even if there are
	spaces inside.  The delimiters are assumed to be isspace() characters.
**************************************************************************************************/
char *
strsep_quotes(char **stringp, char *dest, size_t destlen)
{
	register char *end;
	char	*begin = *stringp;
	char	*d = dest;
	char	quote;													/* Quote character found */

	/* Advance 'begin' past leading spaces */
	for (; *begin && isspace(*begin); begin++)
		/* DONOTHING */;
	if (!*begin)
		return (NULL);

	/* If 'begin' is a quote, advance searching for matching quote, otherwise advance looking for
		whitespace */
	if (*begin == '\'' || *begin == '"')
	{
		quote = *begin++;

		/* We need to be sure to ignore back-quoted quotation marks in here */
		for (end = begin; *end && ((d - dest) < destlen - 1); end++)
		{
			if (*end == quote)
				break;
			if ((*end == '\\') && (end > begin) && (end[1] == quote))
				end++;
			*d++ = *end;
		}
	}
	else
	{
		for (end = begin; *end && !isspace(*end) && ((d - dest) < destlen - 1); end++)
			*d++ = *end;
	}

	/* Terminate 'dest' */
	*d = '\0';

	/* Terminate token and set *stringp past NUL */
	*end++ = '\0';
	*stringp = end;

	return (begin);
}
/*--- strsep_quotes() ---------------------------------------------------------------------------*/

/* vi:set ts=3: */

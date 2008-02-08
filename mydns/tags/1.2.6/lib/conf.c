/**************************************************************************************************
	$Id: conf.c,v 1.26 2005/04/21 16:25:45 bboy Exp $
	conf.c: Routines to read the configuration file.

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
	CONF_TRIM_COMMENTS
	Trim comments from a line, obeying escape characters.
**************************************************************************************************/
static void
conf_trim_comments(char *s)
{
	register char *c;

	for (c = s; *c; c++)
	{
		if (*c == '\\')
			c++;
		else if (c[0] == '#' || (c[0] == '/' && (c[1] == '*' || c[1] == '/')))
		{
			*c = '\0';
			return;
		}
	}
}
/*--- conf_trim_comments() ----------------------------------------------------------------------*/


/**************************************************************************************************
	CONF_GET_OPTION
	Parses the provided line, splitting a "name = value" pair, and escapes text.
**************************************************************************************************/
static void
conf_get_option(char *str, char **namep, char **valuep)
{
	register char	*c;
	register int	got_equal = 0;								/* Have we found a valid equal sign yet? */
	register size_t nlen, vlen;
	char	*name, *value;

	if (!(name = malloc(1 * sizeof(char))))
		Err("malloc");
	if (!(value = malloc(1 * sizeof(char))))
		Err("malloc");
	nlen = vlen = (size_t)0;
	name[0] = '\0';
	value[0] = '\0';

	*namep = *valuep = (char *)NULL;

	for (c = str; *c; c++)
	{
		if (*c == '=' && !got_equal)
		{
			got_equal = 1;
			continue;
		}

		if (*c == '\\')
			c++;

		if (!got_equal)
		{
			if (!name)
			{
				if (!(name = malloc(nlen + 2)))
					Err("malloc");
			}
			else
			{
				if (!(name = realloc(name, (nlen + 2))))
					Err("realloc");
			}
			name[nlen++] = *c;
			name[nlen] = '\0';
		}
		else
		{
			if (!value)
			{
				if (!(value = malloc(vlen + 2)))
					Err("malloc");
			}
			else
			{
				if (!(value = realloc(value, (vlen + 2))))
					Err("realloc");
			}
			value[vlen++] = *c;
			value[vlen] = '\0';
		}
	}
	strtrim(name);
	strtolower(name);
	strtrim(value);

	if (strlen(name))
		*namep = name;
	else
	{
		Free(name);
	}

	if (strlen(value))
		*valuep = value;
	else
	{
		Free(value);
	}
}
/*--- conf_get_option() -------------------------------------------------------------------------*/


/**************************************************************************************************
	CONF_CLOBBER
	Overwrite a value if defaulted.
**************************************************************************************************/
void
conf_clobber(CONF **confp, char *name, char *value)
{
	CONF *conf = *confp, *c;									/* Start of list/found item */

	if (!name || !value)
		return;

	for (c = conf; c; c = c->next)
		if (c->name && !memcmp(c->name, name, strlen(c->name)) && c->defaulted)
		{
			Free(c->value);
			c->value = strdup(value);
		}
}
/*--- conf_clobber() ----------------------------------------------------------------------------*/


/**************************************************************************************************
	CONF_SET
	Sets a configuration value.
**************************************************************************************************/
void
conf_set(CONF **confp, char *name, char *value, int defaulted)
{
	CONF *conf = *confp;											/* Start of list */
	CONF *new;														/* New item to add */
	register CONF *c;

	if (!name)
		return;

	/* Try to find the specified option in the current list */
	for (c = conf; c; c = c->next)
		if (c->name && !memcmp(c->name, name, strlen(c->name)))
		{
			/* Found in current list; append field separator and value */
			if (!strcasecmp(c->name, "listen")
				 || !strcasecmp(c->name, "no-listen")
				 || !strcasecmp(c->name, "bind"))
			{
				if (!c->value)
				{
					if (!(c->value = malloc(strlen(value) + 2)))
						Err("malloc");
				}
				else
				{
					if (!(c->value = realloc(c->value, strlen(c->value) + strlen(value) + 2)))
						Err("realloc");
				}
				strcpy(c->value + strlen(c->value) + 1, value);
				c->value[strlen(c->value)] = CONF_FS_CHAR;
			}
			return;
		}

	/* Add new item to array */
	if (!(new = (CONF *)calloc(1, sizeof(CONF))))
		Err("calloc");
	if (!(new->name = strdup(name)))
		Err("strdup");
	if (!(new->value = value ? (char *)strdup(value) : (char *)NULL))
		Err("strdup");
	new->defaulted = defaulted;
	new->next = conf;
	conf = new;

	*confp = conf;
}
/*--- conf_set() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	CONF_GET
	Returns the value associated with the specified name.
**************************************************************************************************/
char *
conf_get(CONF **confp, char *name, int *defaulted)
{
	CONF *conf = *confp;											/* Start of list */
	register CONF *c;

	if (defaulted)
		*defaulted = 1;

	for (c = conf; c; c = c->next)
		if (!memcmp(c->name, name, strlen(c->name))
			 || (c->altname && !memcmp(c->altname, name, strlen(c->name))))
		{
			if (defaulted)
				*defaulted = c->defaulted;
			return (c->value);
		}
	return (NULL);
}
/*--- conf_get() --------------------------------------------------------------------------------*/


/**************************************************************************************************
	CONF_LOAD
	Load the MyDNS configuration file.
**************************************************************************************************/
void
conf_load(CONF **confp, const char *filename)
{
	FILE	*fp;														/* Input file pointer */
	char	linebuf[BUFSIZ];										/* Input line buffer */
	char	*name, *value;											/* Name and value from `linebuf' */
	CONF	*conf = *confp;										/* Config list */
	int	lineno = 0;												/* Current line number in config */
	struct stat st;												/* File stat for conf file */

	/* If the config file is not present at all, that's not an error */
	if (stat(filename, &st))
	{
		Verbose("%s: %s", filename, strerror(errno));
		return;
	}

	/* Read config file */
	if ((fp = fopen(filename, "r")))
	{
		while (fgets(linebuf, sizeof(linebuf), fp))
		{
			lineno++;
			conf_trim_comments(linebuf);
			conf_get_option(linebuf, &name, &value);
			if (name)
			{
#if 0
				if (!value)
					Warnx(_("%s: line %d: option \"%s\" given without a value - ignored"), filename, lineno, name);
				else
#endif
				if (value)
					conf_set(&conf, name, value, 0);

				Free(name);
			}
			if (value)
			{
				Free(value);
			}
		}
		fclose(fp);
	}
	else
	{
		Verbose("%s: %s", filename, strerror(errno));
		return;
	}

	*confp = conf;
}
/*--- conf_load() -------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */

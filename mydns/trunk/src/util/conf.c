/**************************************************************************************************
	$Id: conf.c,v 1.8 2005/04/20 17:22:25 bboy Exp $

	conf.c: Create daemontools service directory for MyDNS.

	Copyright (C) 2002-2005  Don Moore <bboy@bboy.net>
	Program by David Phillips <david@acz.org>

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

#include "util.h"

#include <pwd.h>


/**************************************************************************************************
	USAGE
	Display program usage information.
**************************************************************************************************/
static void
usage(int status)
{
	fprintf(stderr, _("Usage: %s logacct /mydns\n"), progname);
	fprintf(stderr, _("Create daemontools service directory for MyDNS.\n"));
	exit(status);
}
/*--- usage() -----------------------------------------------------------------------------------*/


/**************************************************************************************************
	MAIN
**************************************************************************************************/
int
main(int argc, char **argv)
{
	const char *loguser;
	const char *dir;
	struct passwd *pw;
	const char *fn;
	int fd;
	FILE *fp;

	setlocale(LC_ALL, "");										/* Internationalization */
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	progname = "mydns-conf";
	err_file = stdout;
	error_init(argv[0], LOG_USER);                     /* Init output routines */

	if (argc != 3)
		usage(EXIT_FAILURE);
	loguser = argv[1];
	dir = argv[2];
	if (dir[0] != '/')
		usage(EXIT_FAILURE);

	pw = getpwnam(loguser);
	if (!pw)
		Errx("unknown account %s", loguser);

	/* Create main directory */
	umask(022);
	if (mkdir(dir, 0700) == -1)
		Err("unable to create %s", dir);
	if (chmod(dir, 03755) == -1)
		Err("unable to set mode of %s", dir);
	if (chdir(dir) == -1)
		Err("unable to switch to %s", dir);

	/* Create run script */
	fn = "run";
	fp = fopen(fn, "w");
	if (!fp)
		Err("unable to create %s/%s", dir, fn);
	fprintf(fp, "#!/bin/sh\n");
	fprintf(fp, "exec 2>&1\n");
	fprintf(fp, "exec %s/mydns -v\n", SBINDIR);
	fflush(fp);
	if (ferror(fp))
		Err("unable to create %s/%s", dir, fn);
	fclose(fp);
	if (chmod(fn, 0755) == -1)
		Err("unable to set mode of %s/%s", dir, fn);

	/* Create log directory */
	fn = "log";
	if (mkdir(fn, 0700) == -1)
		Err("unable to create %s/%s", dir, fn);
	if (chmod(fn, 02755) == -1)
		Err("unable to set mode of %s/%s", dir, fn);

	/* Create log/main directory */
	fn = "log/main";
	if (mkdir(fn, 0700) == -1)
		Err("unable to create %s/%s", dir, fn);
	if (chown(fn, pw->pw_uid, pw->pw_gid) == -1)
		Err("unable to set ownership of %s/%s", dir, fn);
	if (chmod(fn, 02755) == -1)
		Err("unable to set mode of %s/%s", dir, fn);

	/* Create log status file */
	fn = "log/status";
	fd = open(fn, O_WRONLY | O_NDELAY | O_TRUNC | O_CREAT, 0644);
	if (fd == -1)
		Err("unable to create %s/%s", dir, fn);
	close(fd);
	if (chown(fn, pw->pw_uid, pw->pw_gid) == -1)
		Err("unable to set ownership of %s/%s", dir, fn);
	if (chmod(fn, 0644) == -1)
		Err("unable to set mode of %s/%s", dir, fn);

	/* Create log run script */
	fn = "log/run";
	fp = fopen(fn, "w");
	if (!fp)
		Err("unable to create %s/%s", dir, fn);
	fprintf(fp, "#!/bin/sh\n");
	fprintf(fp, "exec setuidgid %s multilog \\\n", loguser);
	fprintf(fp, "  ./main \\\n");
	fprintf(fp, "  '-*' '+* up *' =status\n");
	fflush(fp);
	if (ferror(fp))
		Err("unable to create %s/%s", dir, fn);
	fclose(fp);
	if (chmod(fn, 0755) == -1)
		Err("unable to set mode of %s/%s", dir, fn);

	return (0);
}
/*--- main() ------------------------------------------------------------------------------------*/

/* vi:set ts=3: */
/* NEED_PO */


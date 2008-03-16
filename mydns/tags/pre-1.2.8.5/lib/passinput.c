/**************************************************************************************************
	$Id: passinput.c,v 1.10 2005/04/20 16:49:11 bboy Exp $

	passinput.c: Read a password from standard input.

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

//#include <signal.h>
//#include <stdio.h>
//#include <termios.h>
//#include <unistd.h>
#include "mydnsutil.h"

#ifndef PASS_MAX
#	define PASS_MAX	128
#endif

/**************************************************************************************************
	PASSINPUT
	Prompts user to input password.
	Truncates all characters beyond PASS_MAX
**************************************************************************************************/
char *
passinput(const char *prompt) {
  static char		buf[PASS_MAX + 1];
  char			*ptr;
  sigset_t		sig, sigsave;
  struct termios	term, termsave;
  FILE			*fp;
  int			c;

  buf[0] = '\0';

  if (!isatty(STDIN_FILENO)) return (buf);

  if (!(fp = fopen(ctermid(NULL), "r+"))) return (buf);
  setbuf(fp, NULL);

  sigemptyset(&sig);
  sigaddset(&sig, SIGINT);
  sigaddset(&sig, SIGTSTP);
  sigprocmask(SIG_BLOCK, &sig, &sigsave);

  tcgetattr(fileno(fp), &termsave);
  term = termsave;
  term.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
  tcsetattr(fileno(fp), TCSAFLUSH, &term);

  fputs(prompt, stdout);
  fputs(": ", stdout);

  ptr = buf;
  while ((c = getc(fp)) != EOF && c != '\n') {
    if (ptr < &buf[PASS_MAX])
      *ptr++ = c;
  }
  *ptr = 0;
  putc('\n', fp);

  tcsetattr(fileno(fp), TCSAFLUSH, &termsave);
  sigprocmask(SIG_SETMASK, &sigsave, NULL);
  fclose(fp);
  return (buf);
}
/*--- passinput() -------------------------------------------------------------------------------*/

/* vi:set ts=3: */

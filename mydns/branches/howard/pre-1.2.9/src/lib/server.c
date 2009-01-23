/**************************************************************************************************
	Copyright (C) 2009-  Howard Wilkinson <howard@cohtech.com>

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

#include "server.h"
#include "taskobj.h"

ARRAY	*Servers = NULL;

SERVER *server_find_by_task(TASK *t) {
  int n = 0;

  for (n = 0; n < array_numobjects(Servers); n++) {
    SERVER *server = (SERVER*)array_fetch(Servers, n);
    if (server->listener == t) return server;
  }

  return NULL;
}

void server_kill(SERVER *server, int sig) {
  kill(server->pid, sig);
}

void server_kill_all(int signalid) {
  int n = 0;
  for (n = 0; n < array_numobjects(Servers); n++)
    server_kill((SERVER*)array_fetch(Servers, n), signalid);
}

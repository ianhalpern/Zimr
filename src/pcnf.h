/*   Podora - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Podora.org
 *
 *   This file is part of Podora.
 *
 *   Podora is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Podora is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Podora.  If not, see <http://www.gnu.org/licenses/>
 *

TODO: EXAMPLE OF WHATS TO COME:

# podora.cnf example

proxy: "127.0.0.1:8080"

websites:
- url: [ welive.net, www.welive.net ]
  ip: [ default, "10.10.1.0" ]
  public directory: welive.net/

- url: podora.org
  www-redirect
  public directory: podora.org/

- url: ian-halpern.com
  proxy: [ default, local-default, "127.0.1.1:1245" ]
  public directory: ian-halpern.com/

- url: impulse.ian-halpern.com
  public directory: impulse.ian-halpern.com/

- url: pookieweatherburn.com
  public directory: pookieweatherburn.com/


 */

#ifndef _PD_PCNF_H
#define _PD_PCNF_H

#include <stdbool.h>
#include <yaml.h>
#include <assert.h>

#include "simclist.h"
#include "general.h"

// Website Config structs
typedef struct pcnf_website {
	char* url;
	char* pubdir;
	struct pcnf_website* next;
} pcnf_website_t;

typedef struct {
	pcnf_website_t* website_node;
} pcnf_app_t;
///////////////////////////

// Proxy State structs
typedef struct pcnf_state_app {
	char* exec;
	char* cwd;
	pid_t pid;
} pcnf_state_app_t;

typedef struct {
	list_t apps;
} pcnf_state_t;
///////////////////////////

pcnf_state_t* pcnf_state_load( );
pcnf_app_t* pcnf_app_load( );

bool pcnf_state_app_is_running( pcnf_state_t* state, const char* exec, const char* cwd );
void pcnf_state_set_app( pcnf_state_t* state, const char* exec, const char* cwd, pid_t pid );
void pcnf_state_save( pcnf_state_t* state );

void pcnf_app_free( pcnf_app_t* cnf );
void pcnf_state_app_free( pcnf_state_app_t* app );
void pcnf_state_free( pcnf_state_t* state );

#endif

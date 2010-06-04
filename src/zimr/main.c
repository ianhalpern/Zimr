/*   Zimr - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Zimr.org
 *
 *   This file is part of Zimr.
 *
 *   Zimr is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Zimr is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Zimr.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pwd.h>
#include <fcntl.h>
#include <pwd.h>

#include "zimr.h"
#include "cli.h"

#define ADD     0x1
#define STOP    0x2
#define RESTART 0x3
#define REMOVE  0x4
#define STATUS  0x5

#define CMDSTR(X) (X==ADD?"Adding":X==STOP?"Stopping":X==RESTART?"Restarting":X==REMOVE?"Removing":NULL)

cli_cmd_t root_cmd;

void print_usage() {
	printf( "Zimr " ZIMR_VERSION " (" BUILD_DATE ") - " ZIMR_WEBSITE "\n\n" );
	printf( "Commands:\n\n" );
	cli_print( &root_cmd );
}

/*void print_errors( char* path ) {
	char buff[ PATH_MAX ],* ptr;
	ptr = strrchr( path, '/' ) + 1;
	strncpy( buff, path, ptr - path );
	buff[ ptr - path ] = 0;
	strcat( buff, ZM_ERR_LOGFILE );

	FILE* file;
	file = fopen( buff, "r" );

	if ( file == NULL ) return;

	while ( fgets( buff, sizeof( buff ), file ) != NULL )
		printf( "%s", buff );

	fclose(file);
}*/

void sigquit() {
	exit(0);
}
// Functions ////////////////////////////
void application_shutdown() {
	zimr_shutdown();
	puts( "here" );
}

pid_t application_exec( uid_t uid, char* path, bool nofork ) {
	pid_t pid = !nofork ? fork() : 0;
	char dir[ PATH_MAX ],* ptr;

	if ( pid == 0 ) {
		// in the child
		ptr = strrchr( path, '/' );
		strncpy( dir, path, ptr - path );
		dir[ ptr - path ] = 0;
		chdir( dir );

		setuid( uid );
		setgid( uid );

		//freopen( ZM_OUT_LOGFILE, "w", stdout );
		//freopen( ZM_ERR_LOGFILE, "w", stderr );

		zimr_init();

		signal( SIGTERM, sigquit );
		signal( SIGINT,  sigquit );

		assert( zimr_cnf_load( path ) );

		atexit( application_shutdown );

		zimr_start();

	} else if ( pid == (pid_t) -1 ) {
		// still in parent, but there is no child
		puts( "parent error: no child" );
		return -1;
	} else {
		/*int exitstat;
		sleep( 1 );
		waitpid( pid, &exitstat, WNOHANG );
		if ( exitstat ) {
			//waitpid( pid, &exitstat, 0 );
			return -1;
		}*/
	}

	return pid;
}

bool application_kill( pid_t pid ) {
	if ( !pid ) return true;
	if ( !stopproc( pid ) && errno != ESRCH ) {
		return false;
	}
	return true;
}

bool application_function( uid_t uid, char* cnf_path, char type ) {
	if ( CMDSTR( type ) )
		printf( " * %s %s...", CMDSTR( type ), cnf_path ); fflush( stdout );

	userdir_init( uid );
	zcnf_state_t* state = zcnf_state_load( uid );

	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	zcnf_state_app_t* app = NULL;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( strcmp( app->path, cnf_path ) == 0 ) {
			if ( type == ADD ) {
				puts( "Failed: Already added" );
				retval = false;
				goto quit;
			}
			break;
		}
		app = NULL;
	}

	if ( type != ADD && !app ) {
		puts( "Failed: not added" );
		retval = false;
		goto quit;
	}

	else {
		pid_t pid;
		switch ( type ) {
			case ADD:
				if ( ( pid = application_exec( state->uid, cnf_path, false ) ) == -1 ) {
					retval = false;
					printf( "Failed\n" );
					goto quit;
				}
				printf( "Success.\n" );
				zcnf_state_set_app( state, cnf_path, pid );
				break;
			case RESTART:
				if ( !application_kill( app->pid ) ) {
					retval = false;
					printf( "Failed: %s\n", strerror( errno ) );
					goto quit;
				} else if ( ( pid = application_exec( state->uid, app->path, false ) ) == -1 ) {
					retval = false;
					printf( "Failed\n" );
					//print_errors( app->path );
					goto quit;
				}
				printf( "Success.\n" );
				//print_errors( app->path );
				zcnf_state_set_app( state, app->path, pid );
				break;
			case STOP:
				if ( !application_kill( app->pid ) ) {
					retval = false;
					//printf( "Failed: %s\n", strerror( errno ) );
					goto quit;
				}
				printf( "Success.\n" );
				zcnf_state_set_app( state, app->path, 0 );
				break;
			case REMOVE:
				if ( !application_kill( app->pid ) ) {
					retval = false;
					printf( "Failed: %s\n", strerror( errno ) );
					//print_errors( cnf_path );
					goto quit;
				}
				printf( "Success.\n" );
				list_delete_at( &state->apps, i );
				zcnf_state_app_free( app );
				break;
			case STATUS:
				printf( "%d: %s ", i, app->path );

				if ( !app->pid || kill( app->pid, 0 ) == -1 ) // process still running, not dead
					printf( "(*Stopped)" );
				else printf( "%d", app->pid );
				// TODO: proxy address, uptime, memory usage, running websites, etc

				printf( "\n" );
				break;
		}
	}

	zcnf_state_save( state );

quit:
	zcnf_state_free( state );
	return retval;
}

bool state_function( uid_t uid, char type ) {
	zcnf_state_t* state = zcnf_state_load( uid );
	if ( !state ) {
		puts( "failed to load state" );
		return false;
	}

	bool retval = true;

	zcnf_state_app_t* app;

	int i;
	for ( i = 0; i < list_size( &state->apps ); i++ ) {
		app = list_get_at( &state->apps, i );
		if ( !application_function( uid, app->path, type ) ) {
			//retval = false;
			//goto quit;
		}
	}

//quit:
	zcnf_state_free( state );
	return retval;
}

bool state_all_function( char type ) {
	/* yea...this whole bit is a hack */
	if ( getuid() ) {
		puts( "Failed: Must have superuser privileges" );
		return false;
	}

	struct passwd *p;

	while ( ( p = getpwent() ) ) {
		char filepath[128];

		if ( !expand_tilde_with( ZM_USR_STATE_FILE, filepath, sizeof( filepath ), p->pw_dir ) )
			continue;

		int fd;
		if ( ( fd = open( filepath, O_RDONLY ) ) == -1 ) continue;
		close( fd );

		printf( "User: %s (%s)\n", p->pw_name, filepath );

		state_function( p->pw_uid, type );
	}

	endpwent();

	exit( EXIT_SUCCESS );
}

///////////////////////////////////////////////////////

void application_add_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );


	bool nostate = false;
	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( strcmp( optv[ i ], "-no-state" ) == 0 ) {
			nostate = true;
		} else if ( !i ) {
			realpath( optv[ i ], cnf_path );
		} else {
			print_usage();
			return;
		}
	}

	if ( nostate )
		application_exec( getuid(), cnf_path, true );

	else if ( !application_function( getuid(), cnf_path, ADD ) )
		exit( EXIT_FAILURE );

}

void application_stop_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, STOP ) )
		exit( EXIT_FAILURE );
}

void application_restart_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, RESTART ) )
		exit( EXIT_FAILURE );
}

void application_remove_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, REMOVE ) )
		exit( EXIT_FAILURE );
}

void application_status_cmd( int optc, char* optv[] ) {
	char cnf_path[ PATH_MAX ] = "";
	getcwd( cnf_path, sizeof( cnf_path ) );
	strcat( cnf_path, "/" ZM_APP_CNF_FILE );

	int i;
	for ( i = 0; i < optc; i++ ) {
		if ( !i )
			realpath( optv[ i ], cnf_path );
		else {
			print_usage();
			return;
		}
	}

	if ( !application_function( getuid(), cnf_path, STATUS ) )
		exit( EXIT_FAILURE );
}

void state_stop_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), STOP ) )
		exit( EXIT_FAILURE );
}

void state_restart_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), RESTART ) )
		exit( EXIT_FAILURE );
}

void state_remove_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), REMOVE ) )
		exit( EXIT_FAILURE );
}

void state_status_cmd( int optc, char* optv[] ) {
	if ( !state_function( getuid(), STATUS ) )
		exit( EXIT_FAILURE );
}

void state_restart_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( RESTART ) )
		exit( EXIT_FAILURE );
}

void state_stop_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( STOP ) )
		exit( EXIT_FAILURE );
}

void state_remove_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( REMOVE ) )
		exit( EXIT_FAILURE );
}

void state_status_all_cmd( int optc, char* optv[] ) {
	if ( !state_all_function( STATUS ) )
		exit( EXIT_FAILURE );
}

void help_cmd( int optc, char* optv[] ) {
	print_usage();
}

// Proxy Functions ////////////////////////////

/*void proxy_status() {
	int command = ZM_CMD_STATUS;
	zsocket_init();
	zsocket_t* proxy = zsocket_connect( inet_addr( ZM_PROXY_ADDR ), ZM_PROXY_PORT );

	if ( !proxy ) {
		printf( "could not connect to proxy.\n" );
		return;
	}

	ztransport_t* transport;

	transport = ztransport_open( proxy->sockfd, ZT_WO, command );
	ztransport_close( transport );

	transport  = ztransport_open( proxy->sockfd, ZT_RO, 0 );
	ztransport_read( transport );
	ztransport_close( transport );

	puts( transport->message );

	zsocket_close( proxy );
}

void proxy_status_cmd( int optc, char* optv[] ) {
	proxy_status();
}*/

///////////////////////////////////////////////


int main( int argc, char* argv[] ) {

	cli_cmd_t stop_all_commands[] = {
		{ "states", "Stop all users' webapps.\n", NULL, &state_stop_all_cmd },
		{ NULL }
	};

	cli_cmd_t stop_commands[] = {
		{ "all", "Stop all of the current user's webapps.", &stop_all_commands, &state_stop_cmd },
		{ NULL }
	};

	cli_cmd_t restart_all_commands[] = {
		{ "states", "Restart all users' webapps.\n", NULL, &state_restart_all_cmd },
		{ NULL }
	};

	cli_cmd_t restart_commands[] = {
		{ "all", "Restart all of the current user's webapps.", &restart_all_commands, &state_restart_cmd },
		{ NULL }
	};

	cli_cmd_t remove_all_commands[] = {
		{ "states", "Remove all users' webapps.\n", NULL, &state_remove_all_cmd },
		{ NULL }
	};

	cli_cmd_t remove_commands[] = {
		{ "all", "Remove all of the current user's webapps.", &remove_all_commands, &state_remove_cmd },
		{ NULL }
	};

	cli_cmd_t status_all_commands[] = {
		{ "states", "View the status of all users' webapps.\n", NULL, &state_status_all_cmd },
		{ NULL }
	};

	cli_cmd_t status_commands[] = {
		{ "all", "View the status of all of the current user's webapps.", &status_all_commands, &state_status_cmd },
		{ NULL }
	};

	cli_cmd_t zimr_commands[] = {
		{ "add",     "Add a webapp. [ <config path>, -no-state ]\n", NULL, &application_add_cmd },
		{ "restart", "Restart a webapp. [ <config path> ]", &restart_commands, &application_restart_cmd },
		{ "stop",    "Stop a webapp. [ <config path> ]", &stop_commands, &application_stop_cmd },
		{ "status",  "View the status of a webapp. [ <config path> ]", &status_commands, &application_status_cmd },
		{ "remove",  "Remove a webapp. [ <config path> ]", &remove_commands, &application_remove_cmd },
		{ "help",    "List all commands.", NULL, &help_cmd },
		{ NULL }
	};

	root_cmd.name = "zimr";
	root_cmd.description = "";
	root_cmd.sub_commands = zimr_commands;
	root_cmd.func = NULL;

	char* last_slash = strrchr( argv[ 0 ], '/' );
	if ( last_slash ) argv[ 0 ] = last_slash + 1;

	if ( !cli_run( &root_cmd, argc, argv ) )
		print_usage();

	return EXIT_SUCCESS;
}

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
 */

#include "podora.h"

typedef struct {
	char page_type[ 10 ];
	void (*page_handler)( connection_t*, const char*, void* );
	void* udata;
} page_handler_t;

static int res_fd;
static int page_handler_count = 0;
static page_handler_t page_handlers[ 100 ];

const char* podora_version( ) {
	return PODORA_VERSION;
}

const char* podora_build_date( ) {
	return BUILD_DATE;
}

int podora_init( ) {
	int pid, res_filenum;

	char filename[ 128 ];
	sprintf( filename, PD_TMPDIR "/%d", getpid( ) );
	mkdir( filename, S_IRWXU );
	chmod( filename, 0744 );

	if ( !podora_read_server_info( &pid, &res_filenum ) )
		return 0;

	if ( ( res_fd = pcom_connect( pid, res_filenum ) ) < 0 ) {
		return 0;
	}

	pfd_register_type( PFD_TYPE_PCOM, PFD_TYPE_HDLR &podora_connection_handler );
	pfd_register_type( PFD_TYPE_FILE, PFD_TYPE_HDLR &podora_file_handler );

	return 1;
}

void podora_shutdown( ) {
	while ( website_get_root( ) ) {
		podora_website_destroy( website_get_root( ) );
	}

	char filename[ 128 ];
	sprintf( filename, PD_TMPDIR "/%d", getpid( ) );
	remove( filename );
}

int podora_read_server_info( int* pid, int* res_fn ) {
	int fd;

	if ( ( fd = open( PD_TMPDIR "/" PD_INFO_FILE, O_RDONLY ) ) < 0 ) {
		perror( "[fatal] read_podora_info: open() failed" );
		return 0;
	}

	read( fd, pid, sizeof( *pid ) );
	read( fd, res_fn, sizeof( *res_fn ) );

	close( fd );

	return 1;
}

void podora_start( ) {
	pfd_start( );
}

void podora_send_cmd( int cmd, int key, void* message, int size ) {
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, cmd, key );
	pcom_write( transport, message, size );
	pcom_close( transport );
}

website_t* podora_website_create( char* url ) {
	int req_fd;

	if ( ( req_fd = pcom_create( ) ) < 0 ) {
		return NULL;
	}

	website_t* website;
	website_data_t* website_data;

	if ( !( website = website_add( randkey( ), url ) ) )
		return NULL;

	website_data = (website_data_t*) malloc( sizeof( website_data_t ) );
	website->data = (void*) website_data;

	website_data->req_fd = req_fd;
	website_data->pubdir = strdup( "./" );
	website_data->status = WS_STATUS_DISABLED;
	website_data->connection_handler = NULL;

	pfd_set( website_data->req_fd, PFD_TYPE_PCOM, website );

	return website;
}

void podora_website_destroy( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	podora_website_disable( website );

	if ( website_data->pubdir )
		free( website_data->pubdir );

	pfd_clr( website_data->req_fd );
	pcom_destroy( website_data->req_fd );

	free( website_data );
	website_remove( website );
}

int podora_website_enable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	if ( website_data->status == WS_STATUS_ENABLED )
		return 0;

	int pid = getpid( );
	char message[ sizeof( int ) * 2 + strlen( website->url ) + 1 ];

	memcpy( message, &pid, sizeof( int ) );
	memcpy( message + sizeof( int ), &website_data->req_fd, sizeof( int ) );
	memcpy( message + sizeof( int ) * 2, website->url, strlen( website->url ) + 1 );

	podora_send_cmd( WS_START_CMD, website->key, message, sizeof( message ) );

	website_data->status = WS_STATUS_ENABLED;

	return 1;
}

int podora_website_disable( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;

	if ( website_data->status == WS_STATUS_DISABLED )
		return 0;

	podora_send_cmd( WS_STOP_CMD, website->key, NULL, 0 );

	website_data->status = WS_STATUS_DISABLED;

	return 1;
}

void podora_website_set_connection_handler( website_t* website, void (*connection_handler)( connection_t connection ) ) {
	website_data_t* website_data = (website_data_t*) website->data;
	website_data->connection_handler = connection_handler;
}

void podora_website_unset_connection_handler( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;
	website_data->connection_handler = NULL;
}

void podora_website_set_pubdir( website_t* website, const char* pubdir ) {
	website_data_t* website_data = (website_data_t*) website->data;
	if ( website_data->pubdir ) free( website_data->pubdir );
	website_data->pubdir = strdup( pubdir );
}

char* podora_website_get_pubdir( website_t* website ) {
	website_data_t* website_data = (website_data_t*) website->data;
	return website_data->pubdir;
}

void podora_website_default_connection_handler( connection_t connection ) {
	podora_connection_send_file( &connection, connection.request.url, 1 );
}

void podora_connection_handler( int req_fd, website_t* website ) {
	pcom_transport_t* transport = pcom_open( req_fd, PCOM_RO, 0, 0 );
	connection_t connection;
	website_data_t* website_data = (website_data_t*) website->data;

	pcom_read( transport );

	connection = connection_create( website, transport->header->id, transport->message, transport->header->size );

	response_set_status( &connection.response, 200 );
	// To ensure thier position at the top of the headers
	headers_set_header( &connection.response.headers, "Date", "" );
	headers_set_header( &connection.response.headers, "Server", "" );

	if ( website_data->connection_handler )
		website_data->connection_handler( connection );
	else
		podora_website_default_connection_handler( connection );

	pcom_close( transport );
}

void podora_file_handler( int fd, pcom_transport_t* transport ) {
	int n;
	char buffer[ 2048 ];

	if ( ( n = read( fd, buffer, sizeof( buffer ) ) ) ) {
		if ( !pcom_write( transport, buffer, n ) ) {
			fprintf( stderr, "[error] file_handler: pcom_write() failed\n" );
			return;
		}
	} else {
		pcom_close( transport );
		pfd_clr( fd );
		close( fd );
	}
}

void podora_connection_send_status( pcom_transport_t* transport, connection_t* connection ) {
	char status_line[ 100 ];
	sprintf( status_line, "HTTP/%s %s" HTTP_HDR_ENDL, connection->http_version, response_status( connection->response.http_status ) );

	pcom_write( transport, status_line, strlen( status_line ) );
}

void podora_connection_send_headers( pcom_transport_t* transport, connection_t* connection ) {
	time_t now;
	char now_str[ 80 ];

	time( &now );
	strftime( now_str, 80, "%a %b %d %I:%M:%S %Z %Y", localtime( &now ) );

	headers_set_header( &connection->response.headers, "Date", now_str );
	headers_set_header( &connection->response.headers, "Server", "Podora/" PODORA_VERSION );

	char* cookies = cookies_to_string( &connection->cookies, (char*) malloc( 1024 ), 1024 );

	if ( strlen( cookies ) )
		headers_set_header( &connection->response.headers, "Set-Cookie", cookies );

	char* headers = headers_to_string( &connection->response.headers, (char*) malloc( 1024 ) );

	pcom_write( transport, (void*) headers, strlen( headers ) );
	free( cookies );
	free( headers );
}

void podora_connection_send( connection_t* connection, void* message, int size ) {
	char sizebuf[ 10 ];
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, connection->sockfd, connection->website->key );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );

	sprintf( sizebuf, "%d", size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	pcom_write( transport, (void*) message, size );
	pcom_close( transport );
}

void podora_connection_send_file( connection_t* connection, char* filepath, unsigned char use_pubdir ) {
	struct stat file_stat;
	char full_filepath[ 256 ];
	char* extension;
	int i;

	full_filepath[ 0 ] = '\0';

	if ( use_pubdir )
		strcpy( full_filepath, ((website_data_t*) connection->website->data)->pubdir );

	strcat( full_filepath, filepath );

	if ( stat( full_filepath, &file_stat ) ) {
		podora_connection_send_error( connection );
		return;
	}

	if ( S_ISDIR( file_stat.st_mode ) ) {

		if ( full_filepath + strlen( filepath ) != (char*) "/" )
			strcat( full_filepath, "/" );

		strcat( full_filepath, "default.html" );
	}

	extension = strrchr( full_filepath, '.' );

	if ( extension != NULL && *( extension + 1 ) != '\0' ) {
		extension++;

		for ( i = 0; i < page_handler_count; i++ ) {
			if ( strcmp( page_handlers[ i ].page_type, extension ) == 0 ) {
				page_handlers[ i ].page_handler( connection, full_filepath, page_handlers[ i ].udata );
				break;
			}
		}

	} else {
		i = page_handler_count;
	}

	if ( i == page_handler_count )
		podora_connection_default_page_handler( connection, full_filepath );

}

void podora_connection_send_error( connection_t* connection ) {
	char sizebuf[ 10 ];
	pcom_transport_t* transport = pcom_open( res_fd, PCOM_WO, connection->sockfd, connection->website->key );

	response_set_status( &connection->response, 404 );
	headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( ".html" ) );
	sprintf( sizebuf, "%d", 48 );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	pcom_write( transport, (void*) "<html><body><h1>404 Not Found</h1></body></html>", 48 );
	pcom_close( transport );

}

void podora_register_page_handler( const char* page_type, void (*page_handler)( connection_t*, const char*, void* ), void* udata ) {
	strcpy( page_handlers[ page_handler_count ].page_type, page_type );
	page_handlers[ page_handler_count ].page_handler = page_handler;
	page_handlers[ page_handler_count ].udata = udata;
	page_handler_count++;
}

void podora_connection_default_page_handler( connection_t* connection, char* filepath ) {
	int fd;
	struct stat file_stat;
	char sizebuf[ 10 ];
	pcom_transport_t* transport;

	if ( stat( filepath, &file_stat ) ) {
		podora_connection_send_error( connection );
		return;
	}

	if ( ( fd = open( filepath, O_RDONLY ) ) < 0 ) {
		podora_connection_send_error( connection );
		return;
	}

	transport = pcom_open( res_fd, PCOM_WO, connection->sockfd, connection->website->key );

	if ( !headers_get_header( &connection->response.headers, "Content-Type" ) )
		headers_set_header( &connection->response.headers, "Content-Type", mime_get_type( filepath ) );

	sprintf( sizebuf, "%d", (int) file_stat.st_size );
	headers_set_header( &connection->response.headers, "Content-Length", sizebuf );

	podora_connection_send_status( transport, connection );
	podora_connection_send_headers( transport, connection );

	pfd_set( fd, PFD_TYPE_FILE, transport );
}
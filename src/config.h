/*   Pacoda - Next Generation Web Server
 *
 *+  Copyright (c) 2009 Ian Halpern
 *@  http://Pacoda.org
 *
 *   This file is part of Pacoda.
 *
 *   Pacoda is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Pacoda is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Pacoda.  If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef _PD_CONFIG_H
#define _PD_CONFIG_H

#define BUILD_DATE __DATE__ " " __TIME__

#define PACODA_WEBSITE "http://Pacoda.org"

#define FLAG_ISSET( flag, flags ) ( (flag) & (flags) )

#define D_LOCKFILE_PATH "/tmp/pacoda-proxy.pid" // used by daemon.c
#define PD_APP_CNF_FILE "pacoda.cnf"
#define PD_USR_STATE_FILE "~/.pacoda.state"
#define PD_REQ_LOGFILE "pacoda.log"

#define PD_PROXY_ADDR "127.0.0.1"
#define PD_PROXY_PORT 8888

#define PD_NUM_PROXY_DEATH_RETRIES 1000 // set to 0(ZERO) for infinite retries
#define PD_PROXY_DEATH_RETRY_DELAY 2

/******* PD_CMD's *********/

// Commands MUST be NEGATIVE
#define PD_CMD_WS_START  -1
#define PD_CMD_WS_STOP   -2
#define PD_CMD_STATUS    -3

/**************************/

/****** ptransmission config *******/

#define PT_MSG_SIZE ( PT_BUF_SIZE - PT_HDR_SIZE )
#define PT_HDR_SIZE ( sizeof( ptransport_header_t ) )
#define PT_BUF_SIZE 4096

/**************************/

/****** psocket config ********/

#define PSOCK_N_PENDING 25

/*************************/

/****** http config *******/

#define HTTP_POST "POST"
#define HTTP_GET  "GET"

#define HTTP_GET_TYPE  0x01
#define HTTP_POST_TYPE 0x02

#define HTTP_TYPE(X) ( (X) == HTTP_GET_TYPE ? HTTP_GET : HTTP_POST )

#define HTTP_HDR_ENDL "\r\n"

#define HTTP_DEFAULT_PORT  80

/*************************/

/******* headers config *******/

#define HEADERS_MAX_NUM 32

#define HEADER_NAME_MAX_LEN  128
#define HEADER_VALUE_MAX_LEN 512

/*************************/

/******* params config *******/

#define PARAMS_MAX_NUM 32

#define PARAM_NAME_MAX_LEN  128

/*************************/

#endif

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

#include <Python.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "zimr.h"
#include "dlog.h"

static PyThreadState* mainstate;

void modzimr_init() {

	// initialize thread support
	PyEval_InitThreads();
//	PyEval_ReleaseLock();

	Py_Initialize();
	mainstate = PyThreadState_Swap(NULL);
	PyEval_ReleaseLock();

	PyEval_AcquireLock();
	PyThreadState_Swap( mainstate );
//	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* local_path = PyString_FromString( "" );
	PyObject* sys_path   = PySys_GetObject( "path" );

	PyList_Insert( sys_path, 0, local_path );
	Py_DECREF( local_path );

	char argv[][1] = { { "" } };

	PySys_SetArgv( 0, (char**) argv );

	if ( PyErr_Occurred() )
		PyErr_PrintEx(0);

	/*if ( !PyRun_SimpleString(
		"import zimr\n"
		"from zimr.page_handlers import psp\n"
		"zimr.registerPageHandler( 'psp', psp.render )\n"
	) ) {
		PyErr_Print();
	}*/

//	PyGILState_Release( gstate );
	PyEval_ReleaseLock();
//	Py_UNBLOCK_THREADS
}

void modzimr_destroy() {
	PyEval_AcquireLock();
	PyThreadState_Swap( mainstate );

//	Py_BLOCK_THREADS
//	PyEval_AcquireLock();
	Py_Finalize();
}

void* modzimr_website_init( website_t* website, int argc, char* argv[] ) {
	char* filepath;
	FILE* fd;
	struct stat file_stat;

	PyObject* zimr_module = NULL,* website_type = NULL,* website_obj = NULL,* psp_module = NULL,
	  * psp_render_func = NULL,* register_page_handler = NULL,* insert_default_page = NULL,
	  * func1_ret = NULL,* func2_ret = NULL,* pyfilename = NULL,* module_path = NULL;

	PyEval_AcquireLock();
	PyThreadState_Swap( mainstate );
//	PyGILState_STATE gstate = PyGILState_Ensure();
//	PyEval_AcquireLock();
//	Py_BLOCK_THREADS


	if ( !argc ) {
		filepath = strdup( "main.py" );
		fd = fopen( filepath, "r" );
	}

	else {
		filepath = NULL;
		if ( !( fd = fopen( argv[0], "r" ) ) ) {
			PyObject* imp_module = NULL,* find_module_func = NULL,* ret = NULL,* path = NULL;

			if (
			  !( imp_module = PyImport_ImportModule( "imp" ) ) ||
			  !( find_module_func = PyObject_GetAttrString( imp_module, "find_module" ) )
			)
				goto quit;

			path = PySys_GetObject( "path" );
			Py_INCREF( path );

			char* ptr = argv[0],* ptr2 = NULL;
			bool last = false;

			while ( 1 ) {
				ptr2 = strchr( ptr, '.' );
				if ( !ptr2 ) {
					last = true;
					ptr2 = ptr + strlen( ptr );
				}

				*ptr2 = 0;

				if (
				  !( ret = PyObject_CallFunction( find_module_func, "sO", ptr, path ) ) ||
				  !( module_path = PyTuple_GetItem( ret, 1 ) ) ||
				  last
				)
					break;

				Py_DECREF( path );

				path = PyList_New(1);
				Py_INCREF( module_path );
				PyList_SetItem( path, 0, module_path );
				module_path = NULL;

				ptr = ptr2+1;
			}

			if ( module_path ) {
				filepath = strdup( PyString_AsString( module_path ) );
			}

			Py_XDECREF( imp_module );
			Py_XDECREF( find_module_func );
			Py_XDECREF( ret );
			Py_XDECREF( path );

		//	if ( !filepath || !( fd = fopen( filepath, "r" ) ) ) {
			if ( !filepath || stat( filepath, &file_stat ) ) {
				fprintf( stderr, "Error: modpython could not open file or find module %s.\n", ptr );
				goto quit;
			}

			if ( S_ISDIR( file_stat.st_mode ) ) {
				char* tmp = filepath;
				filepath = malloc( strlen( tmp ) + 12 );
				strcpy( filepath, tmp );
				strcat( filepath, "/__init__.so" );
				free( tmp );

				if ( stat( filepath, &file_stat ) ) {
					strcpy( filepath + ( strlen( filepath ) - 2 ), "py" );
					if ( stat( filepath, &file_stat ) ) {
						fprintf( stderr, "Error: modpython could not open file or find module %s.\n", ptr );
						goto quit;
					}
				}
			}

			if ( !filepath || !( fd = fopen( filepath, "r" ) ) ) {
				fprintf( stderr, "Error: modpython could not open file or find module %s.\n", ptr );
				goto quit;
			}

		} else filepath = strdup(argv[0]);
	}

	if (
	  !( zimr_module     = PyImport_ImportModule( "zimr" ) ) ||
	  !( website_type    = PyObject_GetAttrString( zimr_module, "website" ) ) ||
	  !( website_obj     = PyObject_CallFunction( website_type, "s", website->full_url ) ) ||
	  !( psp_module      = PyImport_ImportModule( "zimr.page_handlers.psp" ) ) ||
	  !( psp_render_func = PyObject_GetAttrString( psp_module, "render" ) ) ||
	  !( register_page_handler = PyObject_GetAttrString( website_obj, "registerPageHandler" ) ) ||
	  !( func1_ret       = PyObject_CallFunction( register_page_handler, "sO", "psp", psp_render_func ) ) ||
	  !( insert_default_page = PyObject_GetAttrString( website_obj, "insertDefaultPage" ) ) ||
	  !( func2_ret       = PyObject_CallFunction( insert_default_page, "s", "default.psp" ) )
	) {
		goto quit;
	}

	zimr_website_insert_ignored_regex( website, "psp_cache" );

	/*
	 website_type    = PyObject_GetAttrString( zimr_module, "website" );
	PyObject* website_obj     = PyObject_CallFunction( website_type, "s", website->url );

	PyObject* psp_module      = PyImport_ImportModule( "zimr.page_handlers.psp" );
	PyObject* psp_render_func = PyObject_GetAttrString( psp_module, "render" );
	PyObject* website_register_page_handler_func = PyObject_GetAttrString( website_obj, "registerPageHandler" );
	PyObject_CallFunction( website_register_page_handler_func, "sO", "psp", psp_render_func );
	PyObject* website_insert_default_page_func = PyObject_GetAttrString( website_obj, "insertDefaultPage" );
	PyObject_CallFunction( website_insert_default_page_func, "s", "default.psp" );
	*/
	if ( !fd ) goto quit;

	PyObject* main_module = PyImport_AddModule( "__main__" );
	PyObject* main_dict = PyModule_GetDict( main_module );

	if ( !( pyfilename = PyString_FromString( filepath ) ) || PyDict_SetItemString( main_dict, "__file__", pyfilename ) == -1 ) {
		//Py_XDECREF( pyfilename ); // Only DECREF if error, else main_dict takes the reference...maybe?
		goto quit;
	}

	if ( !PyRun_File( fd, filepath, Py_file_input, main_dict, main_dict ) ) {
		goto quit;
	}

	if ( PyObject_HasAttrString( main_module, "connection_handler" ) ) {
		PyObject* connection_handler = PyObject_GetAttrString( main_module, "connection_handler" );
		PyObject_SetAttrString( website_obj, "connection_handler", connection_handler );
		Py_DECREF( connection_handler );
	}

	if ( PyObject_HasAttrString( main_module, "error_handler" ) ) {
		PyObject* error_handler = PyObject_GetAttrString( main_module, "error_handler" );
		PyObject_SetAttrString( website_obj, "error_handler", error_handler );
		Py_DECREF( error_handler );
	}

quit:
	//Py_XDECREF( website_obj );
	Py_XDECREF( zimr_module );
	Py_XDECREF( psp_module );
	Py_XDECREF( website_type );
	Py_XDECREF( psp_render_func );
	Py_XDECREF( register_page_handler );
	Py_XDECREF( insert_default_page );
	Py_XDECREF( func1_ret );
	Py_XDECREF( func2_ret );
	Py_XDECREF( module_path );
	Py_XDECREF( pyfilename );

	if ( filepath ) free( filepath );

	if ( PyErr_Occurred() )
		PyErr_PrintEx(0);

	PyEval_ReleaseLock();
//	PyGILState_Release( gstate );
//	Py_UNBLOCK_THREADS

	return NULL;
}

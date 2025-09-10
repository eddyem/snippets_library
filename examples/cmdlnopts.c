/*                                                                                                  geany_encoding=koi8-r
 * cmdlnopts.c - the only function that parse cmdln args and returns glob parameters
 *
 * Copyright 2013 Edward V. Emelianoff <eddy@sao.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include "cmdlnopts.h"
#include "usefull_macros.h"

/*
 * here are global parameters initialisation
 */
static int help;
static glob_pars  G;

// default PID filename:
#define DEFAULT_PIDFILE "/tmp/testcmdlnopts.pid"

//            DEFAULTS
// default global parameters
static glob_pars const Gdefault = {
    .lo0 = INT_MIN,
    .lo1 = INT_MIN,
    .lo2 = INT_MIN,
    .device = NULL,
    .pidfile = DEFAULT_PIDFILE,
    .speed = 9600,
    .logfile = NULL // don't save logs
};

/*
 * Define command line options by filling structure:
 *  name        has_arg     flag    val     type        argptr              help
 *  BE carefull! The `help` field is mandatory! Omitting it equivalent of 'end_option'
*/
static sl_option_t cmdlnopts[] = {
    // short option in only-long options should be zeroed, or you can add flag to set it to given value
    {"lo0",     NEED_ARG,   NULL,     0,    arg_int,    APTR(&G.lo0),       _("only long arg 0 (int)")},
    // for short-only options long option can be NULL
    {NULL,      NEED_ARG,   NULL,   '0',    arg_string, APTR(&G.so1),       _("only short arg 1 (string)")},
    // if you change `arg_int` to `arg_none`, value will be incremented each `-h`
    {"help",    NO_ARGS,    NULL,   'h',    arg_int,    APTR(&help),        _("show this help")},
    {"device",  NEED_ARG,   NULL,   'd',    arg_string, APTR(&G.device),    _("serial device name")},
    // for short-only options long option can also be an empty string
    {"",        NEED_ARG,   NULL,   '1',    arg_string, APTR(&G.so2),       _("only short arg 2 (string)")},
    {"lo2",     NEED_ARG,   NULL,     0,    arg_int,    APTR(&G.lo2),       _("only long arg 2 (int)")},
    {"speed",   NEED_ARG,   NULL,   's',    arg_int,    APTR(&G.speed),     _("serial device speed (default: 9600)")},
    {"logfile", NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   _("file to save logs")},
    {"pidfile", NEED_ARG,   NULL,   'P',    arg_string, APTR(&G.pidfile),   _("pidfile (default: " DEFAULT_PIDFILE ")")},
    {"exclusive",NO_ARGS,   NULL,   'e',    arg_int,    APTR(&G.exclusive), _("open serial device exclusively")},
    // example of multiple options
    {"Int",     MULT_PAR,   NULL,   'I',    arg_int,    APTR(&G.intarr),    _("integer parameter")},
    {"Dbl",     MULT_PAR,   NULL,   'D',    arg_double, APTR(&G.dblarr),    _("double parameter")},
    {"Str",     MULT_PAR,   NULL,   'S',    arg_string, APTR(&G.strarr),    _("string parameter")},
    {"lo1",     NEED_ARG,   NULL,     0,    arg_int,    APTR(&G.lo1),       _("only long arg 1 (int)")},
    end_option
};

/**
 * Parse command line options and return dynamically allocated structure
 *      to global parameters
 * @param argc - copy of argc from main
 * @param argv - copy of argv from main
 * @return allocated structure with global parameters
 */
glob_pars *parse_args(int argc, char **argv){
    int i;
    void *ptr;
    ptr = memcpy(&G, &Gdefault, sizeof(G)); assert(ptr);
    size_t hlen = 1024;
    char helpstring[1024], *hptr = helpstring;
    snprintf(hptr, hlen, "Usage: %%s [args]\n\n\tWhere args are:\n");
    // format of help: "Usage: progname [args]\n"
    sl_helpstring(helpstring);
    // parse arguments
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(help) sl_showhelp(-1, cmdlnopts);
    if(argc > 0){
        G.rest_pars_num = argc;
        G.rest_pars = MALLOC(char *, argc);
        for (i = 0; i < argc; i++)
            G.rest_pars[i] = strdup(argv[i]);
    }
    return &G;
}


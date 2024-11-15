/*
 * This file is part of the Snippets project.
 * Copyright 2024 Edward V. Emelianov <edward.emelianoff@gmail.com>.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h> // NaN
#include <stdint.h>
#include <stdio.h>

#include "usefull_macros.h"

typedef struct{
    char *sp1;
    char *sp2;
    int ip1;
    int ip2;
    double dp1;
    double dp2;
    float fp1;
    float fp2;
    int help;
    int verbose;
    char *confname;
} parameters;

static parameters G = {
    .ip1 = INT_MIN,
    .ip2 = INT_MIN,
    .dp1 = NAN,
    .dp2 = NAN,
    .fp1 = NAN,
    .fp2 = NAN
};

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),  "show this help"},
    {"string1",     NEED_ARG,   NULL,   's',    arg_string, APTR(&G.sp1),   "string one"},
    {"string2",     NEED_ARG,   NULL,   'c',    arg_string, APTR(&G.sp2),   "string two"},
    {"int1",        NEED_ARG,   NULL,   'i',    arg_int,    APTR(&G.ip1),   "integer one"},
    {"int2",        NEED_ARG,   NULL,   'u',    arg_int,    APTR(&G.ip2),   "integer two"},
    {"double1",     NEED_ARG,   NULL,   'd',    arg_double, APTR(&G.dp1),   "double one"},
    {"double2",     NEED_ARG,   NULL,   'o',    arg_double, APTR(&G.dp2),   "double two"},
    {"float1",      NEED_ARG,   NULL,   'f',    arg_float,  APTR(&G.fp1),   "float one"},
    {"float2",      NEED_ARG,   NULL,   'l',    arg_float,  APTR(&G.fp2),   "float two"},
    {"config",      NEED_ARG,   NULL,   'C',    arg_string, APTR(&G.confname),"name of configuration file"},
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),"verbose level (each -v adds 1)"},
    end_option
};


int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    sl_loglevel_e lvl = G.verbose + LOGLEVEL_ERR;
    if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_AMOUNT - 1;
    printf("verbose level: %d\n", lvl);
    if(G.sp1){
        printf("Parsing of string1: ");
        char key[SL_KEY_LEN], val[SL_VAL_LEN];
        int k = sl_get_keyval(G.sp1, key, val);
        switch(k){
            case 0:
                red("key not found\n");
            break;
            case 1:
                green("got key='%s'\n", key);
            break;
            default:
                green("got key='%s', value='%s'\n", key, val);
        }
    }
    green("Starting parameters values:\n");
    char *buf = sl_print_opts(cmdlnopts, TRUE);
    printf("%s\n", buf);
    FREE(buf);
    if(G.confname){
        int o = sl_conf_readopts(G.confname, cmdlnopts);
        if(o > 0){
            printf("got %d options in '%s'\n", o, G.confname);
            green("And after reading of conffile:\n");
            buf = sl_print_opts(cmdlnopts, TRUE);
            printf("%s\n", buf);
            FREE(buf);
        }
    }
    return 0;
}

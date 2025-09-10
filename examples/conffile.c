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
    char **sp;
    int ip1;
    int ip2;
    double **dp;
    float fp1;
    float fp2;
    int help;
    int verbose;
    char *confname;
} parameters;

static parameters G, parini = {
    .ip1 = INT_MIN,
    .ip2 = INT_MIN,
    .fp1 = NAN,
    .fp2 = NAN
};

#define CONFOPTS \
    {"string",      MULT_PAR,   NULL,   's',    arg_string, APTR(&G.sp),    "string array"}, \
    {"int1",        NEED_ARG,   NULL,   'i',    arg_int,    APTR(&G.ip1),   "integer one"}, \
    {"int2",        NEED_ARG,   NULL,   'u',    arg_int,    APTR(&G.ip2),   "integer two"}, \
    {"double",      MULT_PAR,   NULL,   'd',    arg_double, APTR(&G.dp),    "double array"}, \
    {"float1",      NEED_ARG,   NULL,   'f',    arg_float,  APTR(&G.fp1),   "float one"}, \
    {"float2",      NEED_ARG,   NULL,   'l',    arg_float,  APTR(&G.fp2),   "float two"}, \
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),"verbose level (each -v adds 1)"},

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),  "show this help"},
    CONFOPTS
    {"config",      NEED_ARG,   NULL,   'C',    arg_string, APTR(&G.confname),"name of configuration file"},
    end_option
};
// config options - without some unneed
static sl_option_t confopts[] = {
    CONFOPTS
    end_option
};

int main(int argc, char **argv){
    sl_init();
    G = parini;
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    // if you will end main options with '--', you can write some additional options after and again run sl_parseargs with other sl_option_t array
    if(argc) for(int i = 0; i < argc; ++i){
        red("Extra arg: `%s`\n", argv[i]);
    }
    sl_loglevel_e lvl = G.verbose + LOGLEVEL_ERR;
    if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_AMOUNT - 1;
    printf("verbose level: %d\n", lvl);
    if(G.sp){
        char **s = G.sp;
        while(*s){
            printf("Parsing of string: ");
            char key[SL_KEY_LEN], val[SL_VAL_LEN];
            int k = sl_get_keyval(*s, key, val);
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
            ++s;
        }
    }
    green("Starting parameters values:\n");
    char *buf = sl_print_opts(cmdlnopts, TRUE);
    printf("%s\n", buf);
    FREE(buf); // don't forget to `free` this buffer
    if(G.confname){
        const char *confname = G.confname;
        G = parini;
        printf("now v=%d\n", G.verbose);
        int o = sl_conf_readopts(confname, confopts);
        if(o > 0){
            printf("got %d options in '%s'\n", o, confname);
            green("And after reading of conffile:\n");
            buf = sl_print_opts(confopts, TRUE);
            printf("%s\n", buf);
            FREE(buf);
        }
        // if we want to re-read conffile many times over program runs, don't forget to `free` old arrays like this:
        if(G.dp){
            DBG("Clear double array");
            double **p = G.dp;
            while(*p){ FREE(*p); ++p; }
            FREE(G.dp);
        }
        if(G.sp){
            DBG("Clear string array %s", *G.sp);
            char **s = G.sp;
            while(*s){ FREE(*s); ++s; }
            FREE(G.sp);
        }
    }
    return 0;
}

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

#include <stdio.h>
#include <string.h>
#include <usefull_macros.h>

#define BUFS 128

typedef struct{
    int help;
    int verbose;
    int size;
} parameters;

static parameters G = {
    .size = 1024,
};

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   "verbose level (each -v adds 1)"},
    {"bufsize",     NEED_ARG,   NULL,   's',    arg_longlong,APTR(&G.size),      "size of ring buffer"},
    end_option
};

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    sl_ringbuffer *b = sl_RB_new(G.size);
    if(!b) return 1;
    printf("Created ring buffer of %d bytes\n", G.size);
    printf("Enter lines of text to fill it or type (get) to get one line from buffer\n");
    ssize_t got = -1;
    char buf[BUFS];
    for(int nline = 1; ; ++nline){
        printf("%4d > ", nline);
        char *ln = NULL; size_t ls = 0;
        got = getline(&ln, &ls, stdin);
        if(got < 1){ FREE(ln); break; }
        if(0 == strcmp(ln, "get\n")){
            if(sl_RB_readline(b, buf, BUFS)){
                printf("line: %s\n", buf);
                nline -= 2;
            }else --nline;
            continue;
        }
        if(!sl_RB_writestr(b, ln)){
            WARNX("Buffer overfull");
            break;
        }
    }
    printf("\n This is all rest buffer data:\n");
    for(int nline = 1; ; ++nline){
        int r = sl_RB_readline(b, buf, BUFS);
        if(r < 1){
            if(r == -1) WARNX("Next string it too long");
            break;
        }
        printf("line %d: %s\n", nline, buf);
    }
    sl_RB_delete(&b);
    return 0;
}

/*
 * This file is part of the usefull_macros project.
 * Copyright 2018 Edward V. Emelianoff <eddy@sao.ru>.
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

#include <usefull_macros.h>
#include <signal.h>         // signal
#include <stdlib.h>         // exit, free
#include <stdio.h>          // printf
#include <string.h>         // strdup
#include <unistd.h>         // sleep
#include "cmdlnopts.h"

/**
 * This is an example of usage:
 *  - command line arguments,
 *  - log file,
 *  - check of another file version running,
 *  - signals management,
 *  - serial port reading/writing.
 * The `cmdlnopts.[hc]` are intrinsic files of this demo.
 */

/**
 * We REDEFINE the default WEAK function of signal processing
 */
void signals(int sig){
    signal(sig, SIG_IGN);
    restore_console();
    restore_tty();
    DBG("Get signal %d, quit.\n", sig);
    putlog("Exit with status %d", sig);
    exit(sig);
}

void iffound_default(pid_t pid){
    ERRX("Another copy of this process found, pid=%d. Exit.", pid);
}

int main(int argc, char *argv[]){
    initial_setup();
    char *self = strdup(argv[0]);
    glob_pars *GP = parse_args(argc, argv);
    if(GP->rest_pars_num){
        printf("%d extra options:\n", GP->rest_pars_num);
        for(int i = 0; i < GP->rest_pars_num; ++i)
            printf("%s\n", GP->rest_pars[i]);
    }
    check4running(self, GP->pidfile);
    free(self);
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    if(GP->logfile) openlogfile(GP->logfile);
    setup_con();
    putlog(("Start application..."));
    ; // main stuff goes here
    green("Now I will sleep for 10 seconds. Do whatever you want.\n");
    sleep(10);
    ; // clean everything
    if(GP->pidfile) // remove unnesessary PID file
        unlink(GP->pidfile);
    restore_console();
    restore_tty();
    return 0;
}

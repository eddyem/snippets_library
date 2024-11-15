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

#include "cmdlnopts.h"
#include <signal.h>         // signal
#include <stdio.h>          // printf
#include <stdlib.h>         // exit, free
#include <string.h>         // strdup
#include <unistd.h>         // sleep
#include <usefull_macros.h>

/**
 * This is an example of usage:
 *  - command line arguments,
 *  - log file,
 *  - check of another file version running,
 *  - signals management,
 *  - serial port reading/writing.
 * The `cmdlnopts.[hc]` are intrinsic files of this demo.
 */

static sl_tty_t *dev = NULL; // shoul be global to restore if die
static glob_pars *GP = NULL;  // for GP->pidfile need in `signals`

/**
 * We REDEFINE the default WEAK function of signal processing
 */
void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    LOGERR("Exit with status %d", sig);
    if(GP && GP->pidfile) // remove unnesessary PID file
        unlink(GP->pidfile);
    sl_restore_con();
    if(dev) sl_tty_close(&dev);
    exit(sig);
}

void sl_iffound_deflt(pid_t pid){
    ERRX("Another copy of this process found, pid=%d. Exit.", pid);
}

int main(int argc, char *argv[]){
    sl_init();
    GP = parse_args(argc, argv);
    if(GP->rest_pars_num){
        printf("%d extra options:\n", GP->rest_pars_num);
        for(int i = 0; i < GP->rest_pars_num; ++i)
            printf("%s\n", GP->rest_pars[i]);
    }
    sl_check4running((char*)__progname, GP->pidfile);
    red("%s started, snippets library version is %s\n", __progname, sl_libversion());
    sl_setup_con();
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGHUP, SIG_IGN);  // hup - ignore
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    if(GP->logfile) OPENLOG(GP->logfile, LOGLEVEL_ANY, 1);
    LOGMSG("Start application...");
    if(GP->rest_pars_num){
        for(int i = 0; i < GP->rest_pars_num; ++i)
            printf("Extra argument: %s\n", GP->rest_pars[i]);
    }
    if(GP->intarr){
        int **p = GP->intarr;
        for(int i = 0; *p; ++i) printf("Integer[%d]: %d\n", i, **p++);
    }
    if(GP->dblarr){
        double **p = GP->dblarr;
        for(int i = 0; *p; ++i) printf("Double[%d]: %g\n", i, **p++);
    }
    if(GP->strarr){
        char **p = GP->strarr;
        for(int i = 0; *p; ++i) printf("String[%d]: \"%s\"\n", i, *p++);
    }
    if(GP->lo0 != INT_MIN) printf("You set lo0 to %d\n", GP->lo0);
    if(GP->lo1 != INT_MIN) printf("You set lo1 to %d\n", GP->lo1);
    if(GP->lo2 != INT_MIN) printf("You set lo2 to %d\n", GP->lo2);
    if(GP->device){
        LOGDBG("Try to open serial %s", GP->device);
        dev = sl_tty_new(GP->device, GP->speed, 4096);
        if(dev) dev = sl_tty_open(dev, GP->exclusive);
        if(!dev){
            LOGERR("Can't open %s with speed %d. Exit.", GP->device, GP->speed);
            signals(0);
        }
    }

    // main stuff goes here
    long seed = sl_random_seed();
    green("Now I will sleep for 10 seconds after your last input.\n Do whatever you want. Random seed: %ld\n", seed);
    LOGWARN("warning message example");
    LOGWARNADD("with next string without timestamp");
    double t0 = sl_dtime();
    char b[2] = {0};
    while(sl_dtime() - t0 < 10.){ // read data from port and print in into terminal
        if(dev){
            if(sl_tty_read(dev)){
                printf("Got %zd bytes from port: %s\n", dev->buflen, dev->buf);
                LOGMSG("Got from serial: %s", dev->buf);
                t0 = sl_dtime();
            }
            int r = sl_read_con();
            if(r < 1) continue;
            t0 = sl_dtime();
            b[0] = (char) r;
            printf("send to tty: %d (%c)\n", r, b[0]);
            LOGMSG("send to tty: %d (%c)\n", r, b[0]);
            sl_tty_write(dev->comfd, b, 1);
        }
    }
    // clean everything
    signals(0);
    // never reached
    return 0;
}

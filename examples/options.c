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


#include <termios.h>		// tcsetattr
#include <unistd.h>			// tcsetattr, close, read, write
#include <sys/ioctl.h>		// ioctl
#include <stdio.h>			// printf, getchar, fopen, perror
#include <stdlib.h>			// exit
#include <sys/stat.h>		// read
#include <fcntl.h>			// read
#include <signal.h>			// signal
#include <time.h>			// time
#include <string.h>			// memcpy
#include <stdint.h>			// int types
#include <sys/time.h>		// gettimeofday

/**
 * This is an example of usage:
 *  - command line arguments,
 *  - log file,
 *  - check of another file version running,
 *  - signals management,
 *  - serial port reading/writing.
 * The `cmdlnopts.[hc]` are intrinsic files of this demo.
 */

static TTY_descr *dev = NULL; // shoul be global to restore if die
static glob_pars *GP = NULL;  // for GP->pidfile need in `signals`

/**
 * We REDEFINE the default WEAK function of signal processing
 */
void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    putlog("Exit with status %d", sig);
    if(GP->pidfile) // remove unnesessary PID file
        unlink(GP->pidfile);
    restore_console();
    close_tty(&dev);
    exit(sig);
}

void iffound_default(pid_t pid){
    ERRX("Another copy of this process found, pid=%d. Exit.", pid);
}

int main(int argc, char *argv[]){
    initial_setup();
    char *self = strdup(argv[0]);
    GP = parse_args(argc, argv);
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
    if(GP->rest_pars_num){
        for(int i = 0; i < GP->rest_pars_num; ++i)
            printf("Extra argument: %s\n", GP->rest_pars[i]);
    }
    if(GP->device){
        putlog("Try to open serial %s", GP->device);
        dev = new_tty(GP->device, GP->speed, 256);
        if(dev) dev = tty_open(dev, GP->exclusive);
        if(!dev){
            putlog("Can't open %s with speed %d. Exit.", GP->device, GP->speed);
            signals(0);
        }
    }

    // main stuff goes here
    long seed = throw_random_seed();
    green("Now I will sleep for 10 seconds. Do whatever you want. Random seed: %ld\n", seed);
    double t0 = dtime();
    char b[2] = {0};
    while(dtime() - t0 < 10.){ // read data from port and print in into terminal
        if(dev){
            if(read_tty(dev)){
                printf("Got %zd bytes from port: %s\n", dev->buflen, dev->buf);
                t0 = dtime();
            }
            int r = read_console();
            if(r < 1) continue;
            t0 = dtime();
            b[0] = (char) r;
            printf("send to tty: %d (%c)\n", r, b[0]);
            write_tty(dev->comfd, b, 1);
        }
    }
    // clean everything
    signals(0);
    return 0;
}

/*
 * daemon.c - functions for running in background like a daemon
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

#include <stdio.h>      // printf, fopen, ...
#include <unistd.h>     // getpid
#include <stdio.h>      // perror
#include <sys/types.h>  // opendir
#include <dirent.h>     // opendir
#include <sys/stat.h>   // stat
#include <fcntl.h>      // fcntl
#include <stdlib.h>     // exit
#include <string.h>     // memset
#include "usefull_macros.h"

/**
 * @brief sl_getPSname - read process name from /proc/PID/cmdline
 * @param pid - PID of interesting process
 * @return filename or NULL if not found
 *      don't use this function twice for different names without copying
 *      its returning by strdup, because `name` contains in static array
 */
char *sl_getPSname(pid_t pid){
    static char name[PATH_MAX];
    char *pp = name, byte, path[PATH_MAX];
    FILE *file;
    int cntr = 0;
    size_t sz;
    snprintf(path, PATH_MAX, PROC_BASE "/%d/cmdline", pid);
    file = fopen(path, "r");
    if(!file) return NULL; // there's no such file
    do{ // read basename
        sz = fread(&byte, 1, 1, file);
        if(sz != 1) break;
        if(byte != '/') *pp++ = byte;
        else{
            pp = name;
            cntr = 0;
        }
    }while(byte && cntr++ < PATH_MAX-1);
    name[cntr] = 0;
    fclose(file);
    return name;
}

/**
 * @brief sl_iffound_deflt - default action when running process found
 * @param pid - another process' pid
 *      Redefine this function for user action
 */
void WEAK sl_iffound_deflt(pid_t pid){
    fprintf(stderr, _("\nFound running process (pid=%d), exit.\n"), pid);
    exit(-1);
}

/**
 * check wether there is a same running process
 * exit if there is a running process or error
 * Checking have 3 steps:
 *      1) lock executable file
 *      2) check pidfile (if you run a copy?)
 *      3) check /proc for executables with the same name (no/wrong pidfile)
 * @param selfname - argv[0] or NULL for non-locking
 * @param pidfilename - name of pidfile or NULL if none
 */
void sl_check4running(char *selfname, char *pidfilename){
    DIR *dir;
    FILE *pidfile, *fself;
    struct dirent *de;
    struct stat s_buf;
    pid_t pid = 0, self;
    struct flock fl;
    char *name, *myname;
    if(selfname){ // block self
        fself = fopen(selfname, "r"); // open self binary to lock
        if(!fself){
            WARN("fopen()");
            goto selfpid;
        }
        memset(&fl, 0, sizeof(struct flock));
        fl.l_type = F_WRLCK;
        if(fcntl(fileno(fself), F_GETLK, &fl) == -1){ // check locking
            WARN("fcntl()");
            goto selfpid;
        }
        if(fl.l_type != F_UNLCK){ // file is locking - exit
            sl_iffound_deflt(fl.l_pid);
        }
        fl.l_type = F_RDLCK;
        if(fcntl(fileno(fself), F_SETLKW, &fl) == -1){
            WARN("fcntl()");
        }
    }
    selfpid:
    self = getpid(); // get self PID
    if(!(dir = opendir(PROC_BASE))){ // open /proc directory
        ERR(PROC_BASE);
    }
    if(!(name = sl_getPSname(self))){ // error reading self name
        ERR(_("Can't read self name"));
    }
    myname = strdup(name);
    if(pidfilename && stat(pidfilename, &s_buf) == 0){ // pidfile exists
        pidfile = fopen(pidfilename, "r");
        if(pidfile){
            if(fscanf(pidfile, "%d", &pid) > 0){ // read PID of (possibly) running process
                if((name = sl_getPSname(pid)) && strncmp(name, myname, 255) == 0)
                    sl_iffound_deflt(pid);
            }
            fclose(pidfile);
        }
    }
    // There is no pidfile or it consists a wrong record
    while((de = readdir(dir))){ // scan /proc
        if(!(pid = (pid_t)atoi(de->d_name)) || pid == self) // pass non-PID files and self
            continue;
        if((name = sl_getPSname(pid)) && strncmp(name, myname, 255) == 0)
            sl_iffound_deflt(pid);
    }
    closedir(dir);
    free(myname);
    if(pidfilename){
        pidfile = fopen(pidfilename, "w");
        if(!pidfile) ERR(_("Can't open PID file"));
        fprintf(pidfile, "%d\n", self); // write self PID to pidfile
        fclose(pidfile);
    }
}

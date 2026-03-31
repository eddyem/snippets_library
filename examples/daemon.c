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

// example of simplest daemon: search for running process, daemonize if all OK and herd its child

#include <linux/prctl.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <usefull_macros.h>

static pid_t childpid = 0;

typedef struct{
    int help;
    int verbose;
    int nodaemon;
    char *logfile;
    char *pidfile;
} parameters;

static parameters G = {0};

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   "verbose level (each -v adds 1)"},
    {"logfile",     NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   "log file name (FULL path!!!)"},
    {"pidfile",     NEED_ARG,   NULL,   'p',    arg_string, APTR(&G.pidfile),   "PID-file name (FULL path!!!)"},
    {"nodaemon",    NO_ARGS,    NULL,   0,      arg_int,    APTR(&G.nodaemon),  "don't daemonize"},
    end_option
};

void sl_iffound_deflt(pid_t pid){
    WARNX("Another copy of this process found, pid=%d. Exit.", pid);
    exit(1); // don't run `signals` to protect foreign PID-file from removal
}

void signals(int signo){
    if(childpid){ // this is a main process!
        LOGERR("Main process exits with status %d", signo);
        // main process have nothing to cleanup, just remove PID-file
        if(G.pidfile) unlink(G.pidfile);
    }else{ // this is child
        LOGERR("Killed with status %d, clearing", signo);
        // here we can close everything and make cleanup
    }
    usleep(1000);
    LOGERR("Exited");
    exit(signo);
}

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    sl_loglevel_e lvl = G.verbose + LOGLEVEL_ERR;
    if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_AMOUNT - 1;
    sl_check4running(NULL, G.pidfile);
    LOGMSG("Hello, I'm started");
    if(G.logfile) OPENLOG(G.logfile, lvl, 1);
    if(!G.nodaemon){
        green("Daemonize..\n");
        LOGMSG("Daemonize");
        // and ignore SIGHUP
        if(sl_daemonize()){
            WARN("sl_daemonize() failed");
            LOGERR("Can't daemonize");
        }
    }
    signal(SIGTERM, signals); // kill (-15) - quit
    signal(SIGINT, signals);  // ctrl+C - quit
    signal(SIGQUIT, signals); // ctrl+\ - quit
    signal(SIGTSTP, SIG_IGN); // ignore ctrl+Z
    while(1){ // guard for dead processes
        childpid = fork();
        if(childpid){
            LOGMSG("create child with PID %d\n", childpid);
            DBG("Created child with PID %d\n", childpid);
            wait(NULL);
            WARNX("Child %d died\n", childpid);
            LOGWARN("Child %d died\n", childpid);
            sleep(1);
        }else{
            prctl(PR_SET_PDEATHSIG, SIGTERM); // send SIGTERM to child when parent dies
            break; // go out to normal functional
        }
    }
    LOGMSG("Here is the child process; sleep for 1 second and die");
    sleep(1);
    return 0;
}

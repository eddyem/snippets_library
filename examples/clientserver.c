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

#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <usefull_macros.h>

static sl_sock_t *s = NULL;

typedef struct{
    int help;
    int verbose;
    int isserver;
    int isunix;
    int maxclients;
    char *logfile;
    char *node;
} parameters;

static parameters G = {
    .maxclients = 2,
};

static sl_option_t cmdlnopts[] = {
    {"help",        NO_ARGS,    NULL,   'h',    arg_int,    APTR(&G.help),      "show this help"},
    {"verbose",     NO_ARGS,    NULL,   'v',    arg_none,   APTR(&G.verbose),   "verbose level (each -v adds 1)"},
    {"logfile",     NEED_ARG,   NULL,   'l',    arg_string, APTR(&G.logfile),   "log file name"},
    {"node",        NEED_ARG,   NULL,   'n',    arg_string, APTR(&G.node),      "node \"IP\", \"name:IP\" or path (could be \"\\0path\" for anonymous UNIX-socket)"},
    {"server",      NO_ARGS,    NULL,   's',    arg_int,    APTR(&G.isserver),  "create server"},
    {"unixsock",    NO_ARGS,    NULL,   'u',    arg_int,    APTR(&G.isunix),    "UNIX socket instead of INET"},
    {"maxclients",  NEED_ARG,   NULL,   'm',    arg_int,    APTR(&G.maxclients),"max amount of clients connected to server (default: 2)"},
    end_option
};

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
    }
    LOGERR("Exit with status %d", sig);
    sl_restore_con();
    if(s) sl_sock_delete(&s);
    exit(sig);
}

static void runclient(sl_sock_t *s){
    char buf[300];
    if(!s) return;
    do{
        printf("send > ");
        char *r = fgets(buf, 300, stdin);
        if(r){
            DBG("try");
            if(-1 == sl_sock_sendstrmessage(s, buf)){
                WARNX("Error send");
                return;
            }
            DBG("OK");
        }else break;
        ssize_t got = 0;
        double t0 = sl_dtime();
        do{
            got = sl_sock_readline(s, buf, 299);
            if(got > 0) printf("server > %s\n", buf);
        }while(s && s->connected && (got > 0 || sl_dtime() - t0 < 0.3));
        if(got < 0) break;
    } while(s && s->connected);
    WARNX("Ctrl+D or disconnected");
}

// flags for standard handlers
static sl_sock_int_t iflag = {0};
static sl_sock_double_t dflag = {0};

static sl_sock_hresult_e dtimeh(sl_sock_t *client, _U_ sl_sock_hitem_t *item, _U_ const char *req){
    char buf[32];
    snprintf(buf, 31, "UNIXT=%.2f\n", sl_dtime());
    sl_sock_sendstrmessage(client, buf);
    return RESULT_SILENCE;
}

static sl_sock_hresult_e show(sl_sock_t *client, _U_ sl_sock_hitem_t *item, _U_ const char *req){
    if(!G.isunix){
        if(*client->IP){
            printf("Client \"%s\" (fd=%d) ask for flags:\n", client->IP, client->fd);
        }else printf("Can't get client's IP, flags:\n");
    }else printf("Socket fd=%d asks for flags:\n", client->fd);
    printf("\tiflag=%" PRId64 ", dflag=%g\n", iflag.val, dflag.val);
    return RESULT_OK;
}

static void toomuch(int fd){
    const char *m = "Try later: too much clients connected\n";
    send(fd, m, sizeof(m+1), MSG_NOSIGNAL);
    LOGWARN("Client fd=%d tried to connect after MAX reached", fd);
}

static sl_sock_hitem_t handlers[] = {
    {sl_sock_inthandler, "int", "set/get integer flag", (void*)&iflag},
    {sl_sock_dblhandler, "dbl", "set/get double flag", (void*)&dflag},
    {show, "show", "show current flags @ server console", NULL},
    {dtimeh, "dtime", "get server's UNIX time", NULL},
    {NULL, NULL, NULL, NULL}
};

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    if(!G.node) ERRX("Point node");
    sl_socktype_e type = (G.isunix) ? SOCKT_UNIX : SOCKT_NET;
    if(G.isserver){
        sl_sock_changemaxclients(G.maxclients);
        sl_sock_maxclhandler(toomuch);
        s = sl_sock_run_server(type, G.node, -1, handlers);
    } else {
        s = sl_sock_run_client(type, G.node, -1);
    }
    if(!s) ERRX("Can't create socket and/or run threads");
    sl_loglevel_e lvl = G.verbose + LOGLEVEL_ERR;
    if(lvl >= LOGLEVEL_AMOUNT) lvl = LOGLEVEL_AMOUNT - 1;
    if(G.logfile) OPENLOG(G.logfile, lvl, 1);
    LOGMSG("Started");
    signal(SIGTERM, signals);
    signal(SIGINT, signals);
    signal(SIGQUIT, signals);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGHUP, signals);
    if(G.isserver){
        //double t0 = sl_dtime();
        while(s && s->connected){
            if(!s->rthread){
                WARNX("Server handlers thread is dead");
                LOGERR("Server handlers thread is dead");
                break;
            }
            /*double tn = sl_dtime();
            if(tn - t0 > 10.){
                sl_sock_sendall((uint8_t*)"PING\n", 5);
                t0 = tn;
            }*/
        }
    }else runclient(s);
    LOGMSG("Ended");
    DBG("Close");
    sl_sock_delete(&s);
    return 0;
}

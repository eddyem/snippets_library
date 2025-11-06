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
#include <string.h>
#include <sys/socket.h>
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

static sl_ringbuffer_t *rb = NULL;
static char rbuf[BUFSIZ], tbuf[BUFSIZ];

void signals(int sig){
    if(sig){
        signal(sig, SIG_IGN);
        DBG("Get signal %d, quit.\n", sig);
        LOGERR("Exit with status %d", sig);
    }else LOGERR("Exit");
    if(rb){
        while(sl_RB_readline(rb, rbuf, BUFSIZ-1)){ // show all recent messages from server
            printf("server > %s\n", rbuf);
        }
        sl_RB_delete(&rb);
    }
    sl_restore_con();
    if(s) sl_sock_delete(&s);
    exit(sig);
}

static void runclient(sl_sock_t *s){
    rb = sl_RB_new(BUFSIZ * 4);
    if(!s) return;
    do{
        while(sl_RB_readline(rb, rbuf, BUFSIZ-1)){ // show all recent messages from server
            printf("server > %s\n", rbuf);
        }
        printf("send > "); fflush(stdout);
        int c, k = 0;
        while (k < BUFSIZ-1){
            ssize_t got = sl_sock_readline(s, rbuf, BUFSIZ);
            if(got > 0){
                DBG("GOT %zd", got);
                if(k == 0){ printf("\nserver > %s\nsend > ", rbuf); fflush(stdout); }// user didn't type anything -> show server messages
                else sl_RB_writestr(rb, rbuf);
            } else if(got < 0){ DBG("disc"); signals(0);}
            if(!s || !s->connected){ DBG("DISC"); signals(0); }
            c = sl_read_con();
            if(!c) continue;
            if(c == '\b' || c == 127){ // use DEL and BACKSPACE to erase previous symbol
                if(k){
                    --k;
                    printf("\b \b");
                }
            }else{
                if(c == EOF) break;
                tbuf[k++] = c;
                printf("%c", c);
            }
            fflush(stdout);
            if(c == '\n') break;
        }
        tbuf[k] = 0;
        DBG("Your str: _%s_", tbuf);
        if(c == EOF) break;
        if(k >= BUFSIZ-1) ERRX("Congrats! You caused buffer overflow!");
        if(k){
            DBG("try");
            if(-1 == sl_sock_sendstrmessage(s, tbuf)){
                WARNX("Error send");
                return;
            }
            DBG("OK");
        }else break;
    } while(s && s->connected);
    WARNX("Ctrl+D or disconnected");
}

// flags for standard handlers
static sl_sock_int_t iflag = {0};
static sl_sock_double_t dflag = {0};
static sl_sock_string_t sflag = {0};
static uint32_t bitflags = 0;

static sl_sock_hresult_e dtimeh(sl_sock_t _U_ *client, _U_ sl_sock_hitem_t *item, _U_ const char *req){
    char buf[32];
    snprintf(buf, 31, "UNIXT=%.2f\n", sl_dtime());
    sl_sock_sendall(s, (uint8_t*)buf, strlen(buf));
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

// Too much clients handler
static void toomuch(int fd){
    const char m[] = "Try later: too much clients connected\n";
    send(fd, m, sizeof(m)-1, MSG_NOSIGNAL);
    shutdown(fd, SHUT_WR);
    DBG("shutdown, wait");
    double t0 = sl_dtime();
    uint8_t buf[8];
    while(sl_dtime() - t0 < 11.){
        if(sl_canread(fd)){
            ssize_t got = read(fd, buf, 8);
            DBG("Got=%zd", got);
            if(got < 1) break;
        }
    }
    DBG("Disc after %gs", sl_dtime() - t0);
    LOGWARN("Client fd=%d tried to connect after MAX reached", fd);
}
// new connections handler (return FALSE to reject client)
static int connected(sl_sock_t *c){
    if(c->type == SOCKT_UNIX) LOGMSG("New client fd=%d connected", c->fd);
    else LOGMSG("New client fd=%d, IP=%s connected", c->fd, c->IP);
    return TRUE;
}
// disconnected handler
static void disconnected(sl_sock_t *c){
    if(c->type == SOCKT_UNIX) LOGMSG("Disconnected client fd=%d", c->fd);
    else LOGMSG("Disconnected client fd=%d, IP=%s", c->fd, c->IP);
}
// default (unknown key) handler
static sl_sock_hresult_e defhandler(struct sl_sock *s, const char *str){
    if(!s || !str) return RESULT_FAIL;
    sl_sock_sendstrmessage(s, "You entered wrong command:\n```\n");
    sl_sock_sendstrmessage(s, str);
    sl_sock_sendstrmessage(s, "\n```\nTry \"help\"\n");
    return RESULT_SILENCE;
}
// if we use this macro, there's no need to run `sl_sock_keyno_init` later
static sl_sock_keyno_t kph_number = SL_SOCK_KEYNO_DEFAULT;
// handler for key with optional parameter number
static sl_sock_hresult_e keyparhandler(struct sl_sock *s, sl_sock_hitem_t *item, const char *req){
    if(!item->data) return RESULT_FAIL;
    char buf[1024];
    int no = sl_sock_keyno_check((sl_sock_keyno_t*)item->data);
    long long newval = -1;
    if(req){
        if(!sl_str2ll(&newval, req) || newval < 0 || newval > 0xffffffff) return RESULT_BADVAL;
    }
    printf("no = %d\n", no);
    if(no < 0){ // flags as a whole
        if(req) bitflags = (uint32_t)newval;
        snprintf(buf, 1023, "flags = 0x%08X\n", bitflags);
        sl_sock_sendstrmessage(s, buf);
    }else if(no < 32){ // bit access
        int bitmask = 1 << no;
        if(req){
            if(newval) bitflags |= bitmask;
            else bitflags &= ~bitmask;
        }
        snprintf(buf, 1023, "flags[%d] = %d\n", no, bitflags & bitmask ? 1 : 0);
        sl_sock_sendstrmessage(s, buf);
    }else return RESULT_BADKEY;
    return RESULT_SILENCE;
}

static sl_sock_hitem_t handlers[] = {
    {sl_sock_inthandler, "int", "set/get integer flag", (void*)&iflag},
    {sl_sock_dblhandler, "dbl", "set/get double flag", (void*)&dflag},
    {sl_sock_strhandler, "str", "set/get string variable", (void*)&sflag},
    {show, "show", "show current flags @ server console", NULL},
    {dtimeh, "dtime", "get server's UNIX time for all clients connected", NULL},
    {keyparhandler, "flags", "set/get bit flags as whole (flags=val) or by bits (flags[bit]=val)", (void*)&kph_number},
    {NULL, NULL, NULL, NULL}
};

int main(int argc, char **argv){
    sl_init();
    sl_parseargs(&argc, &argv, cmdlnopts);
    if(G.help) sl_showhelp(-1, cmdlnopts);
    if(!G.node) ERRX("Point node");
    sl_socktype_e type = (G.isunix) ? SOCKT_UNIX : SOCKT_NET;
    if(G.isserver){
        //sl_sock_keyno_init(&kph_number); // don't forget to init first or use macro in initialisation
        s = sl_sock_run_server(type, G.node, -1, handlers);
    } else {
        sl_setup_con();
        s = sl_sock_run_client(type, G.node, -1);
    }
    if(!s) ERRX("Can't create socket and/or run threads");
    if(G.isserver){
        sl_sock_changemaxclients(s, G.maxclients);
        sl_sock_maxclhandler(s, toomuch);
        sl_sock_connhandler(s, connected);
        sl_sock_dischandler(s, disconnected);
        sl_sock_defmsghandler(s, defhandler);
    }
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

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

#include <arpa/inet.h>
#include <ctype.h>
#include <inttypes.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/un.h>  // unix socket
#include <unistd.h>

#include "usefull_macros.h"

/**
 * @brief sl_sock_changemaxclients - change amount of max simultaneously connected clients
 * SHOULD BE run BEFORE running of server
 * @param val - maximal clients number
 */
void sl_sock_changemaxclients(sl_sock_t *sock, int val){
    if(sock) sock->maxclients = val;
}
int sl_sock_getmaxclients(sl_sock_t *sock){
    if(sock) return sock->maxclients;
    else return -1;
}

// in each next function changed default handlers you can set h to NULL to remove your handler
// setter of "too much clients" handler
void sl_sock_maxclhandler(sl_sock_t *sock, void (*h)(int)){
    if(sock) sock->toomuch_handler = h;
}
// setter of "new client connected" handler
void sl_sock_connhandler(sl_sock_t *sock, int(*h)(struct sl_sock*)){
    if(sock) sock->newconnect_handler = h;
}
// setter of "client disconnected" handler
void sl_sock_dischandler(sl_sock_t *sock, void(*h)(struct sl_sock*)){
    if(sock) sock->disconnect_handler = h;
}
// setter of default client message handler
void sl_sock_defmsghandler(struct sl_sock *sock, sl_sock_hresult_e(*h)(struct sl_sock *s, const char *str)){
    if(sock) sock->defmsg_handler = h;
}

// text messages for `hresult`
static const char *resmessages[RESULT_AMOUNT] = {
    [RESULT_OK] = "OK\n",
    [RESULT_FAIL] = "FAIL\n",
    [RESULT_BADKEY] = "BADKEY\n",
    [RESULT_BADVAL] = "BADVAL\n",
    [RESULT_SILENCE] = "",
};

/**
 * @brief sl_sock_hresult2str - get string message by hresult value
 * @param r - handler result
 * @return string with message
 */
const char *sl_sock_hresult2str(sl_sock_hresult_e r){
    if(r >= RESULT_AMOUNT) return "BADRESULT";
    return resmessages[r];
}

// convert UNIX socket name for unaddr; result should be free'd
static char *convunsname(const char *path){
    char *apath = MALLOC(char, 106);
    if(*path == 0){
        DBG("convert name starting from 0");
        apath[0] = 0;
        strncpy(apath+1, path+1, 104);
    }else if(strncmp("\\0", path, 2) == 0){
        DBG("convert name starting from \\0");
        apath[0] = 0;
        strncpy(apath+1, path+2, 104);
    }else strncpy(apath, path, 105);
    return apath;
}

/**
 * @brief sl_sock_delete - close socket and delete descriptor
 * @param sock - pointer to socket descriptor
 */
void sl_sock_delete(sl_sock_t **sock){
    if(!sock || !*sock) return;
    sl_sock_t *ptr = *sock;
    ptr->connected = 0;
    if(ptr->rthread){
        DBG("Cancel thread");
        pthread_cancel(ptr->rthread);
    }
    DBG("close fd=%d", ptr->fd);
    if(ptr->fd > -1) close(ptr->fd);
    DBG("delete ring buffer");
    sl_RB_delete(&ptr->buffer);
    DBG("free addrinfo");
    if(ptr->addrinfo) freeaddrinfo(ptr->addrinfo);
    DBG("free other");
    FREE(ptr->node);
    FREE(ptr->service);
    FREE(ptr->data);
    DBG("free sock");
    FREE(*sock);
}

/**
 * @brief clientrbthread - thread to fill client's ringbuffer with incoming data
 *        If s->handlers is not NULL, process all incoming data HERE, you shouldn't use ringbuffer by hands!
 * @param d - socket descriptor
 * @return NULL
 */
static void *clientrbthread(void *d){
    sl_sock_t *s = (sl_sock_t*) d;
    size_t buflen = s->buffer->length;
    char *buf = MALLOC(char, buflen);
    DBG("Start client read buffer thread");
    while(s && s->connected){
        pthread_mutex_lock(&s->mutex);
        if(1 != sl_canread(s->fd)){
            pthread_mutex_unlock(&s->mutex);
            usleep(1000);
            continue;
        }
        ssize_t n = read(s->fd, buf, buflen);
        //DBG("read %zd from fd=%d, unlock", n, s->fd);
        //DBG("buf=%s", buf);
        pthread_mutex_unlock(&s->mutex);
        if(n < 1){
            WARNX(_("Server disconnected"));
            goto errex;
        }
        ssize_t got = 0;
        do{
            ssize_t written = sl_RB_write(s->buffer, (uint8_t*)buf + got, n-got);
            //DBG("Put %zd to buffer, got=%zd, n=%zd", written, got, n);
            if(got > n) return NULL;
            if(written > 0) got += written;
        }while(got != n);
        //DBG("All messsages done");
    }
errex:
    s->rthread = 0;
    s->connected = FALSE;
    return NULL;
}

/**
 * @brief sl_sock_sendall - send data to all clients connected (works only for server)
 * @param data - message
 * @param len - its length
 * @return N of sends or -1 if no server process running
 */
int sl_sock_sendall(sl_sock_t *sock, uint8_t *data, size_t len){
    FNAME();
    if(!sock || !sock->clients) return -1;
    int nsent = 0;
    for(int i = sock->maxclients; i > 0; --i){
        if(!sock->clients[i]) continue;
        if(sock->clients[i]->fd < 0 || !sock->clients[i]->connected) continue;
        if((ssize_t)len == sl_sock_sendbinmessage(sock->clients[i], data, len)) ++nsent;
    }
    return nsent;
}

static sl_sock_hresult_e parse_post_data(sl_sock_t *c, char *str);

// return TRUE if this is header without data (also modify c->sockmethod)
static int iswebheader(sl_sock_t *client, char *str){
    DBG("check header: _%s_", str);
    const char *methods[] = {
        [SOCKM_GET] = "GET",
        [SOCKM_PUT] = "PUT",
        [SOCKM_POST] = "POST",
        [SOCKM_PATCH] = "PATCH",
        [SOCKM_DELETE] = "DELETE"
    };
    const int metlengths[] = {
        [SOCKM_GET] = 3,
        [SOCKM_PUT] = 3,
        [SOCKM_POST] = 4,
        [SOCKM_PATCH] = 5,
        [SOCKM_DELETE] = 6
    };
    for(sl_sockmethod_e m = SOCKM_GET; m < SOCKM_AMOUNT; ++m){
        if(0 == strncmp(str, methods[m], metlengths[m])){
            client->sockmethod = m;
            DBG("SOCK method: %s", methods[m]);
            if(m == SOCKM_GET){ // modify `str` by GET
                char *slash = strchr(str, '/');
                if(slash){
                    char *eol = strstr(slash, "HTTP");
                    if(eol){ // move to `str` head
                        size_t l = eol - slash - 2;
                        memmove(str, slash+1, l);
                        str[l] = 0;
                        DBG("User asks GET '%s' -> run parser", str);
                        //parse_post_data(client, str);
                        return TRUE;
                    }
                }
            }
            return TRUE;
        }
    }
    return FALSE;
}

static char *stringscan(char *str, char *needle){
    char *a, *end = str + strlen(str);
    a = strstr(str, needle);
    if(!a) return NULL;
    a += strlen(needle);
    while (a < end && isspace(*a)) a++;
    if(a >= end) return NULL;
    DBG("Found a needle \"%s\"", needle);
    return a;
}

// just check for header words - if so, return TRUE
static int chkwebreq(sl_sock_t *client, char *str){
    if(client->contlen == 0){
        char *cl = stringscan(str, "Content-Length:");
        if(cl){
            DBG("Contlen: _%s_", cl);
            sl_str2i(&client->contlen, cl);
            return TRUE;
        }
    }
    DBG("retval: %d", !client->gotemptyline);
    return !client->gotemptyline;
}

// In-place URL parser
void url_decode(char *str) {
    if (!str) return;
    char *src = str;
    char *dst = str;
    char hex[3] = {0};
    DBG("STR ori: _%s_", str);
    while(*src){
        if(*src == '+'){
            *dst++ = ' ';
            src++;
        }else if(*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])){
            hex[0] = src[1];
            hex[1] = src[2];
            *dst++ = (char)strtol(hex, NULL, 16);
            src += 3;
        }else{
            *dst++ = *src++;
        }
    }
    *dst = 0;
    DBG("STR DECODED to _%s_", str);
}

static sl_sock_hresult_e msgparser(sl_sock_t *client, char *str);

// parser of web-encoded data by POST/GET:
static sl_sock_hresult_e parse_post_data(sl_sock_t *c, char *str){
    if (!c || !str) return RESULT_BADKEY;
    if(0 == strcmp("favicon.ico", str)){
        DBG("icon -> omit");
        return RESULT_SILENCE;
    }
    char *start = str;
    char *current = str;
    DBG("\n\n\nSTART parser");
    while(*current){
        if(*current == '&'){
            *current = '\0';
            DBG("\n\n`start` = _%s_", start);
            url_decode(start);
            sl_sock_hresult_e r = msgparser(c, start);
            if(r != RESULT_SILENCE) sl_sock_sendstrmessage(c, sl_sock_hresult2str(r));
            start = current + 1;
        }
        current++;
    }
    if(*start){
        url_decode(start);
        sl_sock_hresult_e r = msgparser(c, start);
        if(r != RESULT_SILENCE) sl_sock_sendstrmessage(c, sl_sock_hresult2str(r));
    }
    DBG("\n\n\nEND parser");
    return RESULT_SILENCE;
}

// parser of client's message
// "only-server's" fields of `client` are copies of server's
static sl_sock_hresult_e msgparser(sl_sock_t *client, char *str){
    char key[SL_KEY_LEN], val[SL_VAL_LEN], *valptr;
    if(!str || !*str) return RESULT_BADKEY;
    // check web headers and fields
    DBG("\n\nLINENO: %lu", client->lineno);
    if(client->lineno == 0){ // first data line from client -> check if it's a header
        if(iswebheader(client, str)){
            if(client->sockmethod == SOCKM_GET)
                return parse_post_data(client, str);
            return RESULT_SILENCE;
        }
    }else if(client->sockmethod != SOCKM_RAW){
        if(chkwebreq(client, str)) return RESULT_SILENCE;
    }
    if(!client->handlers){ // have only default handler
        if(!client->defmsg_handler) return RESULT_BADKEY;
        return client->defmsg_handler(client, str);
    }
    int N = sl_get_keyval(str, key, val);
    DBG("getval=%d, key=%s, val=%s", N, key, val);
    if(N == 0){
        if(client->defmsg_handler) return client->defmsg_handler(client, str);
        return RESULT_BADKEY;
    }
    if(N == 1) valptr = NULL;
    else valptr = val;
    if(0 == strcmp(key, "help")){
        sl_sock_sendstrmessage(client, "\nHelp:\n");
        for(sl_sock_hitem_t *h = client->handlers; h->handler; ++h){
            if(h->help){
                sl_sock_sendstrmessage(client, h->key);
                sl_sock_sendstrmessage(client, ": ");
                sl_sock_sendstrmessage(client, h->help);
                sl_sock_sendbyte(client, '\n');
            }
        }
        sl_sock_sendbyte(client, '\n');
        return RESULT_SILENCE;
    }
    // check for strict params like `key=val`
    for(sl_sock_hitem_t *h = client->handlers; h->handler; ++h){
        if(strcmp(h->key, key)) continue;
        if(h->data){
            sl_sock_keyno_t *kn = (sl_sock_keyno_t*)h->data;
            if(-1 == isinf(kn->magick)) kn->n = -1; // no value number
        }
        return h->handler(client, h, valptr);
    }
    // now check for optional key's number like key0=val, key[1]=val, key(2)=val or key{3}=val
    int keylen = strlen(key);
    char *numstart = NULL;
    const char *bra = "([{", *ket = ")]}";
    char *found = strchr(ket, key[keylen - 1]);
    DBG("found=%s", found);
    if(found){
        char *keyend = strchr(key, bra[found - ket]); // find starting bracket
        DBG("keyend=%s", keyend);
        if(keyend){
            numstart = keyend + 1;
            keylen = keyend - key;
        }
    }else{ // maybe this is key123=val ?
        numstart = &key[keylen-1];
        while(numstart > key && isdigit(*numstart)) --numstart;
        if(numstart == &key[keylen-1]) numstart = NULL;
        else{
            keylen = (++numstart) - key;
            DBG("numstart=%s", numstart);
        }
    }
    if(numstart){
        char *eptr;
        long long LL = strtoll(numstart, &eptr, 0);
        DBG("LL=%lld, len=%d", LL, keylen);
        if(eptr != numstart && LL >= 0 && LL <= INT_MAX){
            int parno = (int)LL;
            for(sl_sock_hitem_t *h = client->handlers; h->handler; ++h){
                if(strncmp(h->key, key, keylen)) continue; // now search only in first `keylen` bytes
                DBG("found %s", h->key);
                if(h->data){
                    sl_sock_keyno_t *kn = (sl_sock_keyno_t*)h->data;
                    if(-1 == isinf(kn->magick)){
                        kn->n = parno;
                        DBG("run handler, parno=%d", parno);
                        return h->handler(client, h, valptr);
                    }
                }
                DBG("NO data");
                break;
            }
        }
    }
    if(client->defmsg_handler) return client->defmsg_handler(client, str);
    return RESULT_BADKEY;
}

// send http responce with data in c->outbuffer
static void send_http_response(sl_sock_t* c){
    if(c->sockmethod == SOCKM_RAW) return;
    DBG("Send to client HTTP response");
    c->sockmethod = SOCKM_RAW; // dirty hack to send data by sl_sock_sendbinmessage
    char tbuf[BUFSIZ];
    ssize_t L = snprintf(tbuf, BUFSIZ-1,
         "HTTP/2.0 200 OK\r\n"
         "Access-Control-Allow-Origin: *\r\n"
         "Access-Control-Allow-Methods: GET, POST\r\n"
         "Access-Control-Allow-Credentials: true\r\n"
         "Content-type: text/plain\r\nContent-Length: %zd\r\n\r\n", c->outplen);
    if(L < 0){
        WARN("sprintf()");
        LOGWARN("sprintf()");
        return;
    }
    if(L != sl_sock_sendbinmessage(c, (uint8_t*)tbuf, L)) return;
    if(c->outplen != (size_t)sl_sock_sendbinmessage(c, (uint8_t*)c->outbuffer, c->outplen)) return;
}

/**
 * @brief serverrbthread - thread for standard server procedure (when user give non-NULL `handlers`)
 * @param d - socket descriptor
 * @return NULL
 */
static void *serverthread(void _U_ *d){
    sl_sock_t *s = (sl_sock_t*) d;
    if(!s || (!s->handlers && !s->defmsg_handler)){
        WARNX(_("Can't start server handlers thread"));
        goto errex;
    }
    int sockfd = s->fd;
    if(listen(sockfd, s->maxclients) == -1){
        WARN("listen");
        goto errex;
    }
    DBG("Start server handlers thread");
    int nfd = 1; // only one socket @start
    struct pollfd *poll_set = MALLOC(struct pollfd, s->maxclients+1);
    sl_sock_t **clients = MALLOC(sl_sock_t*, s->maxclients+1);
    s->clients = clients;
    // init default clients records
    for(int i = s->maxclients; i > 0; --i){
        DBG("fill %dth client info", i);
        clients[i] = MALLOC(sl_sock_t, 1);
        sl_sock_t *c = clients[i];
        c->type = s->type;
        if(s->node) c->node = strdup(s->node);
        if(s->service) c->service = strdup(s->service);
        // fill addrinfo
        c->addrinfo = MALLOC(struct addrinfo, 1);
        c->addrinfo->ai_addr = MALLOC(struct sockaddr, 1);
        // copy server data: we have no `self`, so use so
        c->handlers = s->handlers;
        c->defmsg_handler = s->defmsg_handler;
    }
    // ZERO - listening server socket
    poll_set[0].fd = sockfd;
    poll_set[0].events = POLLIN;
    // disconnect client
    void disconnect_(sl_sock_t *c, int N){
        DBG("Disconnect client \"%s\" (fd=%d)", c->IP, c->fd);
        if(s->disconnect_handler) s->disconnect_handler(c);
        if(c->sockmethod != SOCKM_RAW) send_http_response(c); // we are closing HTTP request - send headers and answer
        pthread_mutex_lock(&c->mutex);
        DBG("close fd %d", c->fd);
        c->connected = 0;
        close(c->fd);
        c->outplen = 0;
        c->lineno = 0;
        c->gotemptyline = 0;
        sl_RB_clearbuf(c->buffer);
        // now move all data of last client to disconnected
        if(nfd > 2 && N != nfd - 1){ // don't move the only or the last
            DBG("lock fd=%d", clients[nfd-1]->fd);
            pthread_mutex_lock(&clients[nfd-1]->mutex);
            clients[N] = clients[nfd-1];
            clients[nfd-1] = c;
            DBG("unlock fd=%d", clients[N]->fd);
            pthread_mutex_unlock(&clients[N]->mutex); // now N is nfd-1
            poll_set[N] = poll_set[nfd - 1];
        }
        DBG("unlock fd=%d", c->fd);
        pthread_mutex_unlock(&c->mutex);
        --nfd;
    }
    // allocate buffer with size not less than RB size
    size_t bufsize = s->buffer->length; // as RB should be 1 byte less, this is OK
    uint8_t *buf = MALLOC(uint8_t, bufsize);
    while(s && s->connected){
        poll(poll_set, nfd, 1);
        if(poll_set[0].revents & POLLIN){ // check main for accept()
            struct sockaddr a;
            socklen_t len = sizeof(struct sockaddr);
            int client = accept(sockfd, &a, &len);
            DBG("New connection, nfd=%d, len=%d", nfd, len);
            if(nfd == s->maxclients + 1){
                WARNX(_("Limit of connections reached"));
                if(s->toomuch_handler) s->toomuch_handler(client);
                close(client);
            }else{
                memset(&poll_set[nfd], 0, sizeof(struct pollfd));
                poll_set[nfd].fd = client;
                poll_set[nfd].events = POLLIN;
                DBG("got client[%d], fd=%d", nfd, client);
                sl_sock_t *c = clients[nfd];
                c->fd = client;
                DBG("memcpy");
                memcpy(c->addrinfo->ai_addr, &a, len);
                DBG("set conn flag");
                c->connected = 1;
                struct sockaddr_in* inaddr = (struct sockaddr_in*)&a;
                if(!inet_ntop(AF_INET, &inaddr->sin_addr, c->IP, INET_ADDRSTRLEN)){
                    WARN("inet_ntop()");
                    *c->IP = 0;
                }
                DBG("got IP:%s", c->IP);
                ++nfd;
                if(s->newconnect_handler && s->newconnect_handler(c) == FALSE){
                    DBG("Client %s rejected", c->IP);
                    disconnect_(c, nfd);
                }else{
                    if(!c->buffer){ // allocate memory for client's ringbuffer
                        DBG("allocate ringbuffer");
                        c->buffer = sl_RB_new(s->buffer->length); // the same size as for master
                    }
                }
            }
        }
        // scan connections
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            if((poll_set[fdidx].revents & POLLIN) == 0) continue;
            int fd = poll_set[fdidx].fd;
            sl_sock_t *c = clients[fdidx];
            pthread_mutex_lock(&c->mutex);
            size_t nread = sl_RB_freesize(c->buffer);
            if(nread > bufsize) nread = bufsize;
            else if(nread < 1){ // no space in ringbuffer
                pthread_mutex_unlock(&c->mutex);
                // check for RB overflow
                if(sl_RB_hasbyte(c->buffer, '\n') < 0){ // -1 - buffer empty (can't be), -2 - buffer overflow
                    WARNX(_("Server thread: ring buffer overflow for fd=%d"), fd);
                    LOGERR(_("Server thread: ring buffer overflow for fd=%d"), fd);
                    disconnect_(c, fdidx);
                    --fdidx;
                }
                continue;
            }
            ssize_t got = read(fd, buf, nread);
            DBG("can read %zd bytes, got %zd bytes, buf=_%s_", nread, got, buf);
            pthread_mutex_unlock(&c->mutex);
            if(got <= 0){ // client disconnected
                disconnect_(c, fdidx);
                --fdidx;
            }else{
                if(sl_RB_write(c->buffer, buf, got) < (size_t) got){
                    WARNX(_("Server thread: can't write data to ringbuffer: overflow from fd=%d"), fd);
                    disconnect_(c, fdidx);
                    --fdidx;
                }
            }
        }
        // and now check all incoming buffers
        for(int fdidx = 1; fdidx < nfd; ++fdidx){
            sl_sock_t *c = clients[fdidx];
            if(!c->connected) continue;
            ssize_t got = sl_RB_readline(c->buffer, (char*)buf, bufsize);
            if(got < 0){ // buffer overflow
                WARNX(_("Server thread: buffer overflow from fd=%d"), c->fd);
                disconnect_(c, fdidx);
                --fdidx;
                continue;
            }else if(got == 0){ // check last data in POST/GET methods
                if(c->sockmethod == SOCKM_RAW) continue;
                size_t l = sl_RB_datalen(c->buffer);
                if(c->sockmethod == SOCKM_POST){
                    if(l != (size_t)c->contlen) continue; // wait for last data
                    if(l < bufsize - 1){
                        sl_RB_read(c->buffer, buf, l);
                        buf[l] = 0;
                        parse_post_data(c, (char*)buf);
                    }
                }
                disconnect_(c, fdidx);
                --fdidx;
                continue;
            }
            if(got > 1 && *buf && *buf != '\r'){ // not empty line
                if(buf[got-2] == '\r'){
                    buf[got-2] = 0; // omit '\r' for "\r\n"
                    DBG("delete \\r: _%s_", buf);
                }
                sl_sock_hresult_e r = msgparser(c, (char*)buf);
                if(r != RESULT_SILENCE) sl_sock_sendstrmessage(c, sl_sock_hresult2str(r));
            }else{
                DBG("EMPTY line");
                if(c->sockmethod != SOCKM_RAW) c->gotemptyline = TRUE;
            }
            ++c->lineno;
        }
    }
    // clear memory
    FREE(buf);
    FREE(poll_set);
    for(int i = s->maxclients; i > 0; --i){
        DBG("Clear %dth client data", i);
        sl_sock_t *c = clients[i];
        if(c->fd > -1) close(c->fd);
        if(c->buffer) sl_RB_delete(&c->buffer);
        FREE(c->addrinfo->ai_addr);
        FREE(c->addrinfo);
        FREE(c->node);
        FREE(c->service);
        FREE(c);
    }
    FREE(clients);
    s->clients = NULL;
errex:
    s->rthread = 0;
    return NULL;
}

/**
 * @brief sl_sock_open - open socket (client or server)
 * @param type - socket type
 * @param path - path (for UNIX socket); for NET: ":port" for server, "server:port" for client
 * @param handlers - standard handlers when read data (or NULL)
 * @param bufsiz - input ring buffer size
 * @param isserver - 1 for server, 0 for client
 * @return socket descriptor or NULL if failed
 * to create anonymous UNIX-socket you can start "path" from 0 or from string "\0"
 */
static sl_sock_t *sl_sock_open(sl_socktype_e type, const char *path, sl_sock_hitem_t *handlers, int bufsiz, int isserver){
    if(!path || type >= SOCKT_AMOUNT) return NULL;
    if(bufsiz < 256) bufsiz = 256;
    int sock = -1;
    struct addrinfo ai = {0}, *res = &ai;
    struct sockaddr_un unaddr = {0};
    char *str = NULL;
    ai.ai_socktype = SOCK_STREAM;
    switch(type){
        case SOCKT_UNIX:
            str = convunsname(path);
            if(!str) return NULL;
            unaddr.sun_family = AF_UNIX;
            ai.ai_addr = (struct sockaddr*) &unaddr;
            ai.ai_addrlen = sizeof(unaddr);
            memcpy(unaddr.sun_path, str, 106);
            ai.ai_family = AF_UNIX;
            //ai.ai_socktype = SOCK_SEQPACKET;
        break;
        case SOCKT_NET:
        case SOCKT_NETLOCAL:
            //ai.ai_socktype = SOCK_DGRAM; // = SOCK_STREAM;
            ai.ai_family = AF_INET;
            if(isserver) ai.ai_flags = AI_PASSIVE;
        break;
        default: // never reached
            WARNX(_("Unsupported socket type %d"), type);
            return NULL;
        break;
    }
    sl_sock_t *s = MALLOC(sl_sock_t, 1);
    s->type = type;
    s->fd = -1;
    s->maxclients = SL_DEF_MAXCLIENTS;
    s->handlers = handlers;
    s->buffer = sl_RB_new(bufsiz);
    if(!s->buffer){
        sl_sock_delete(&s);
        return NULL;
    }else{ // fill node/service
        if(type == SOCKT_UNIX) s->node = strdup(str); // str now is converted path
        else{
            char *delim = strchr(path, ':');
            if(!delim) s->service = strdup(path); // only port
            else{
                if(delim == path) s->service = strdup(path+1);
                else{
                    size_t l = delim - path;
                    s->node = MALLOC(char, l + 1);
                    strncpy(s->node, path, l);
                    s->service = strdup(delim + 1);
                }
            }
            DBG("socket->service=%s", s->service);
        }
        DBG("socket->node=%s", s->node);
    }
    // now try to open socket
    if(type != SOCKT_UNIX){
        DBG("try to get addrinfo for node '%s' and service '%s'", s->node, s->service);
        if(isserver){
            if(!s->node){
                DBG("Socket type now is SOCKT_NET");
                s->type = SOCKT_NET;
            }
            if(s->type == SOCKT_NETLOCAL){
                DBG("SOCKT_NETLOCAL: change `node` to localhost");
                FREE(s->node);
                s->node = strdup("127.0.0.1");  // localhost
            }
        }
        DBG("---> node '%s', service '%s'", s->node, s->service);
        int e = getaddrinfo(s->node, s->service, &ai, &res);
        if(e){
            WARNX("getaddrinfo(): %s", gai_strerror(e));
            sl_sock_delete(&s);
            return NULL;
        }
    }
    for(struct addrinfo *p = res; p; p = p->ai_next){
        if((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) continue;
        DBG("Try proto %d, type %d", p->ai_protocol, p->ai_socktype);
        if(isserver){
            int reuseaddr = 1;
            if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) == -1){
                WARN("setsockopt()");
                close(sock); sock = -1;
                continue;
            }
            if(bind(sock, p->ai_addr, p->ai_addrlen) == -1){
                WARN("bind()");
                close(sock); sock = -1;
                continue;
            }
            int enable = 1;
            if(ioctl(sock, FIONBIO, (void *)&enable) < 0){ // make socket nonblocking
                WARN("Can't make socket non-blocked");
            }
        }else{
            if(connect(sock, p->ai_addr, p->ai_addrlen) == -1){
                WARN("connect()");
                close(sock); sock = -1;
            }
        }
        break;
    }
    if(sock < 0) sl_sock_delete(&s);
    else{
        s->fd = sock;
        pthread_mutex_init(&s->mutex, NULL);
        DBG("s->fd=%d, node=%s, service=%s", s->fd, s->node, s->service);
        int r = -1;
        if(isserver){
            if(s->handlers || s->defmsg_handler)
                r = pthread_create(&s->rthread, NULL, serverthread, (void*)s);
            else r = 0;
        }else{
            r = pthread_create(&s->rthread, NULL, clientrbthread, (void*)s);
        }
        if(r){
            WARN("pthread_create()");
            sl_sock_delete(&s);
        }else{
            s->connected = TRUE;
            DBG("fd=%d CONNECTED", s->fd);
        }
    }
    return s;
}

/**
 * @brief sl_sock_run_client - try to connect to server and, socket read thread and process optional handlers
 * @param type - server type
 * @param path - path or address:port
 * @param handlers - array with handlers
 * @param bufsiz - input ring buffer size
 * @return socket descriptor or NULL if failed
 */
sl_sock_t *sl_sock_run_client(sl_socktype_e type, const char *path, int bufsiz){
    sl_sock_t *s = sl_sock_open(type, path, NULL, bufsiz, 0);
    return s;
}

/**
 * @brief sl_sock_run_server - run built-in server parser
 * @param type - server type
 * @param path - path or port
 * @param handlers - array with handlers
 * @param bufsiz - input ring buffer size
 * @return socket descriptor or NULL if failed
 */
sl_sock_t *sl_sock_run_server(sl_socktype_e type, const char *path, int bufsiz, sl_sock_hitem_t *handlers){
    sl_sock_t *s = sl_sock_open(type, path, handlers, bufsiz, 1);
    return s;
}

/**
 * @brief sl_sock_sendbinmessage - send binary data
 * @param socket - socket
 * @param msg - data
 * @param l - data length
 * @return amount of bytes sent or -1 in case of error
 */
ssize_t sl_sock_sendbinmessage(sl_sock_t *socket, const uint8_t *msg, size_t l){
    if(!msg || l < 1) return -1;
    if(socket->sockmethod != SOCKM_RAW){ // just fill buffer while socket isn't marked as "RAW"
        DBG("Put to buffer: _%s_", (char*)msg);
        size_t L = BUFSIZ - socket->outplen;
        if(l > L) l = L;
        memcpy(socket->outbuffer + socket->outplen, msg, l);
        socket->outplen += l;
        DBG("Now buflen=%zd, buf: ```%s```", socket->outplen, socket->outbuffer);
        return l;
    }
    DBG("send to fd=%d message with len=%zd (%s)", socket->fd, l, msg);
    while(socket && socket->connected && !sl_canwrite(socket->fd));
    if(!socket || !socket->connected) return -1;
    DBG("lock");
    pthread_mutex_lock(&socket->mutex);
    DBG("SEND");
    ssize_t sent = 0;
    do{
        ssize_t r = send(socket->fd, msg+sent, l, MSG_NOSIGNAL);
        if(r < 0){
            sent = -1;
            break;
        }else sent += r;
        DBG("sent %zd bytes", r);
    } while((size_t)sent != l);
    DBG("unlock");
    pthread_mutex_unlock(&socket->mutex);
    return sent;
}

ssize_t sl_sock_sendstrmessage(sl_sock_t *socket, const char *msg){
    if(!msg) return -1;
    size_t l = strlen(msg);
    return sl_sock_sendbinmessage(socket, (const uint8_t*)msg, l);
}

/**
 * @brief sl_sock_sendbyte - send one byte over socket
 * @param socket - socket
 * @param byte - byte to send
 * @return -1 in case of error, 1 if all OK
 */
ssize_t sl_sock_sendbyte(sl_sock_t *socket, uint8_t byte){
    while(socket && socket->connected && !sl_canwrite(socket->fd));
    if(!socket || !socket->connected) return -1;
    if(socket->sockmethod != SOCKM_RAW){ // just fill buffer while socket isn't marked as "RAW"
        DBG("Put to buffer: _%c_", (char)byte);
        if(socket->outplen == BUFSIZ) return 0;
        socket->outbuffer[socket->outplen++] = (char)byte;
        DBG("Now buflen=%zd, buf: ```%s```", socket->outplen, socket->outbuffer);
        return 1;
    }
    DBG("lock");
    pthread_mutex_lock(&socket->mutex);
    ssize_t r = send(socket->fd, &byte, 1, MSG_NOSIGNAL);
    DBG("unlock");
    pthread_mutex_unlock(&socket->mutex);
    return r;
}

/**
 * @brief sl_sock_readline - read string line from incoming ringbuffer
 * @param sock - socket
 * @param str (o) - buffer to copy
 * @param len - length of str
 * @return amount of bytes read
 */
ssize_t sl_sock_readline(sl_sock_t *sock, char *str, size_t len){
    if(!sock || !sock->buffer || !str || !len) return -1;
    return sl_RB_readline(sock->buffer, str, len);
}

// default handlers - setters/getters of int64, double and string
sl_sock_hresult_e sl_sock_inthandler(sl_sock_t *client, sl_sock_hitem_t *hitem, const char *str){
    char buf[128];
    sl_sock_int_t *i = (sl_sock_int_t *)hitem->data;
    if(!i) return RESULT_FAIL;
    if(!str){ // getter
        snprintf(buf, 127, "%s=%" PRId64 "\n", hitem->key, i->val);
        sl_sock_sendstrmessage(client, buf);
        return RESULT_SILENCE;
    }
    long long x;
    if(!sl_str2ll(&x, str)) return RESULT_BADVAL;
    if(x < INT64_MIN || x > INT64_MAX) return RESULT_BADVAL;
    i->val = (int64_t)x;
    i->timestamp = sl_dtime();
    return RESULT_OK;
}
sl_sock_hresult_e sl_sock_dblhandler(sl_sock_t *client, sl_sock_hitem_t *hitem, const char *str){
    char buf[128];
    sl_sock_double_t *d = (sl_sock_double_t *)hitem->data;
    if(!d) return RESULT_FAIL;
    if(!str){ // getter
        snprintf(buf, 127, "%s=%g\n", hitem->key, d->val);
        sl_sock_sendstrmessage(client, buf);
        return RESULT_SILENCE;
    }
    double dv;
    if(!sl_str2d(&dv, str)) return RESULT_BADVAL;
    d->val = dv;
    d->timestamp = sl_dtime();
    return RESULT_OK;
}
sl_sock_hresult_e sl_sock_strhandler(sl_sock_t *client, sl_sock_hitem_t *hitem, const char *str){
    char buf[SL_VAL_LEN + SL_KEY_LEN + 3];
    sl_sock_string_t *s = (sl_sock_string_t*) hitem->data;
    if(!s) return RESULT_FAIL;
    if(!str){ // getter
        snprintf(buf, SL_VAL_LEN + SL_KEY_LEN + 2, "%s=%s\n", hitem->key, s->val);
        sl_sock_sendstrmessage(client, buf);
        return RESULT_SILENCE;
    }
    int l = strlen(str);
    if(l > SL_VAL_LEN - 1) return RESULT_BADVAL;
    s->len = l;
    s->timestamp = sl_dtime();
    memcpy(s->val, str, l);
    s->val[l] = 0;
    return RESULT_OK;
}

/**
 * @brief sl_sock_keyno_init - init k->magick and k-> to default values
 * @param k - key's number value
 */
void sl_sock_keyno_init(sl_sock_keyno_t* k){
    if(!k) return;
    k->magick = -INFINITY;
    k->n = -1;
}
/**
 * @brief sl_sock_keyno_check - check if this is a really `sl_sock_keyno_t`
 * @param k - pointer to check
 * @return k.n or -1 if failed
 */
int sl_sock_keyno_check(sl_sock_keyno_t* k){
    DBG("magick=%g (%d)", k->magick, isinf(k->magick));
    if(!k || -1 != isinf(k->magick)) return -1;
    DBG("k->n=%d", k->n);
    return k->n;
}

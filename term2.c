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

#include <unistd.h>         // tcsetattr, close, read, write
#include <fcntl.h>          // read
#include <stdio.h>          // printf, getchar, fopen, perror
#include <stdlib.h>         // exit, realloc
#include <string.h>         // memcpy
#include <sys/ioctl.h>      // ioctl
#include <sys/stat.h>       // read
#include <sys/time.h>       // gettimeofday
#include <time.h>           // time
#include <unistd.h>         // usleep, tcsetattr, close, read, write
#include "usefull_macros.h"

#define LOGBUFSZ (1024)

// return FALSE if failed
static int parse_format(const char *iformat, tcflag_t *flags){
    tcflag_t f = 0;
    if(!iformat){ // default
        if(flags) *flags = CS8;
        return TRUE;
    }
    if(strlen(iformat) != 3) goto someerr;
    switch(iformat[0]){
        case '5':
            f |= CS5;
        break;
        case '6':
            f |= CS6;
        break;
        case '7':
            f |= CS7;
        break;
        case '8':
            f |= CS8;
        break;
        default:
            goto someerr;
    }
    switch(iformat[1]){
        case '0': // always 0
            f |= PARENB | CMSPAR;
        break;
        case '1': // always 1
            f |= PARENB | CMSPAR | PARODD;
        break;
        case 'E': // even
            f |= PARENB;
        break;
        case 'N': // none
        break;
        case 'O': // odd
            f |= PARENB | PARODD;
        break;
        default:
            goto someerr;
    }
    switch(iformat[2]){
        case '1':
        break;
        case '2':
            f |= CSTOPB;
        break;
        default:
            goto someerr;
    }
    if(flags) *flags = f;
    return TRUE;
someerr:
    WARNX(_("Wrong USART format \"%s\"; use NPS, where N: 5..8; P: N/E/O/1/0, S: 1/2"), iformat);
    return FALSE;
}

/**
 * @brief tty_init - open & setup terminal
 * @param descr (io) - port descriptor
 * !!! default settings are "8N1".
 * !!! If you want other, set it like `descr->format = "7O2"` between `sl_tty_new` and `sl_tty_open`
 * @return 0 if all OK, last error code or 1
 */
static int tty_init(sl_tty_t *descr){
    tcflag_t flags;
    if(!parse_format(descr->format, &flags)) return 1;
    if((descr->comfd = open(descr->portname, O_RDWR|O_NOCTTY)) < 0){
        WARN(_("Can't use port %s"), descr->portname);
        return globErr ? globErr : 1;
    }
    if(ioctl(descr->comfd, TCGETS2, &descr->oldtty)){ // Get settings
        WARN(_("Can't get old TTY settings"));
        return globErr ? globErr : 1;
    }
    descr->tty = descr->oldtty;
    descr->tty.c_lflag     = 0; // ~(ICANON | ECHO | ECHOE | ISIG)
    descr->tty.c_iflag     = 0; // don't do any changes in input stream
    descr->tty.c_oflag     = 0; // don't do any changes in output stream
    descr->tty.c_cflag     = BOTHER | flags | CREAD | CLOCAL; // other speed, user format, RW, ignore line ctrl
    descr->tty.c_ispeed = descr->speed;
    descr->tty.c_ospeed = descr->speed;
    descr->tty.c_cc[VMIN]  = 0;  // non-canonical mode
    descr->tty.c_cc[VTIME] = 5;
    if(ioctl(descr->comfd, TCSETS2, &descr->tty)){
        WARN(_("Can't apply new TTY settings"));
        return globErr ? globErr : 1;
    }
    ioctl(descr->comfd, TCGETS2, &descr->tty);
    if(descr->tty.c_ispeed != (speed_t)descr->speed || descr->tty.c_ospeed != (speed_t)descr->speed){
        WARN(_("Can't set speed %d, got ispeed=%d, ospeed=%d"), descr->speed, descr->tty.c_ispeed, descr->tty.c_ospeed);
        descr->speed = descr->tty.c_ispeed;
    }
    // make exclusive open
    if(descr->exclusive){
        if(ioctl(descr->comfd, TIOCEXCL)){
            WARN(_("Can't do exclusive open"));
        }}
    return 0;
}

/**
 * @brief restore_tty - restore opened TTY to previous state and close it
 */
void sl_tty_close(sl_tty_t **descr){
    if(descr == NULL || *descr == NULL) return;
    sl_tty_t *d = *descr;
    if(d->comfd){
        ioctl(d->comfd, TCSETS2, &d->oldtty); // return TTY to previous state
        close(d->comfd);
    }
    FREE(d->portname);
    FREE(d->buf);
    FREE(*descr);
}

/**
 * @brief sl_tty_new   - create new TTY structure with partially filled fields
 * @param comdev    - TTY device filename
 * @param speed     - speed (number)
 * @param bufsz     - size of buffer for input data (or 0 if opened only to write)
 * @return pointer to TTY structure if all OK
 */
sl_tty_t *sl_tty_new(char *comdev, int speed, size_t bufsz){
    sl_tty_t *descr = MALLOC(sl_tty_t, 1);
    descr->portname = strdup(comdev);
    descr->speed = speed;
    if(!descr->portname){
        WARNX(_("Port name is missing"));
    }else{
        if(bufsz){
            descr->buf = MALLOC(char, bufsz+1);
            descr->bufsz = bufsz;
            return descr;
        }else WARNX(_("Need non-zero buffer for TTY device"));
    }
    FREE(descr->portname);
    FREE(descr);
    return NULL;
}

/**
 * @brief sl_tty_open  - init & open tty device
 * @param d         - already filled structure (with new_tty or by hands)
 * @param exclusive - == 1 to make exclusive open
 * @return pointer to TTY structure if all OK
 */
sl_tty_t *sl_tty_open(sl_tty_t *d, int exclusive){
    if(!d || !d->portname) return NULL;
    if(exclusive) d->exclusive = TRUE;
    else d->exclusive = FALSE;
    if(tty_init(d)) return NULL;
    return d;
}

static struct timeval tvdefault = {.tv_sec = 0, .tv_usec = 5000};
/**
 * @brief sl_tty_tmout - set timeout for select() on reading
 * @param usec - microseconds of timeout
 * @return -1 if usec < 0, 0 if all OK
 */
int sl_tty_tmout(double usec){
    if(usec < 0.) return -1;
    tvdefault.tv_sec = 0;
    if(usec > 999999){
        tvdefault.tv_sec = (__time_t)(usec / 1e6);
        usec -= tvdefault.tv_sec * 1e6;
    }
    tvdefault.tv_usec = (__suseconds_t) usec;
    return 0;
}

/**
 * @brief sl_tty_read - read data from TTY with 10ms timeout
 * @param buff (o) - buffer for data read
 * @param length   - buffer len
 * @return amount of bytes read or -1 if disconnected
 */
int sl_tty_read(sl_tty_t *d){
    if(!d || d->comfd < 0) return 0;
    size_t L = 0;
    ssize_t l;
    size_t length = d->bufsz;
    char *ptr = d->buf;
    fd_set rfds;
    struct timeval tv;
    int retval;
    do{
        l = 0;
        FD_ZERO(&rfds);
        FD_SET(d->comfd, &rfds);
        //memcpy(&tv, &tvdefault, sizeof(struct timeval));
        tv = tvdefault;
        retval = select(d->comfd + 1, &rfds, NULL, NULL, &tv);
        if(!retval) break;
        if(retval < 0){
            if(errno == EINTR) continue;
            return -1;
        }
        if(FD_ISSET(d->comfd, &rfds)){
            l = read(d->comfd, ptr, length);
            if(l < 1) return -1; // disconnected
            ptr += l; L += l;
            length -= l;
        }
    }while(l && length);
    d->buflen = L;
    d->buf[L] = 0;
    return (size_t)L;
}

/**
 * @brief sl_tty_write - write data to serial port
 * @param buff (i)  - data to write
 * @param length    - its length
 * @return 0 if all OK
 */
int sl_tty_write(int comfd, const char *buff, size_t length){
    ssize_t L = write(comfd, buff, length);
    if((size_t)L != length){
        WARN("Write error");
        return 1;
    }
    return 0;
}

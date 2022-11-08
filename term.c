/*
 * client.c - simple terminal client
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
#include <unistd.h>         // tcsetattr, close, read, write
#include <sys/ioctl.h>      // ioctl
#include <stdio.h>          // printf, getchar, fopen, perror
#include <stdlib.h>         // exit, realloc
#include <sys/stat.h>       // read
#include <fcntl.h>          // read
#include <signal.h>         // signal
#include <time.h>           // time
#include <string.h>         // memcpy
#include <stdint.h>         // int types
#include <sys/time.h>       // gettimeofday
#include <unistd.h>         // usleep
#include "usefull_macros.h"

#define LOGBUFSZ (1024)

typedef struct {
    int speed;       // communication speed in bauds/s
    tcflag_t bspeed; // baudrate from termios.h
} spdtbl;

static int tty_init(TTY_descr *descr);

static spdtbl speeds[] = {
    {50, B50},
    {75, B75},
    {110, B110},
    {134, B134},
    {150, B150},
    {200, B200},
    {300, B300},
    {600, B600},
    {1200, B1200},
    {1800, B1800},
    {2400, B2400},
    {4800, B4800},
    {9600, B9600},
    {19200, B19200},
    {38400, B38400},
    {57600, B57600},
    {115200, B115200},
    {230400, B230400},
    {460800, B460800},
    {500000, B500000},
    {576000, B576000},
    {921600, B921600},
    {1000000, B1000000},
    {1152000, B1152000},
    {1500000, B1500000},
    {2000000, B2000000},
    {2500000, B2500000},
    {3000000, B3000000},
    {3500000, B3500000},
    {4000000, B4000000},
    {0,0}
};

/**
 * @brief conv_spd - test if `speed` is in .speed of `speeds` array
 * @param speed - integer speed (bps)
 * @return 0 if error, Bxxx if all OK
 */
tcflag_t conv_spd(int speed){
    spdtbl *spd = speeds;
    int curspeed = 0;
    do{
        curspeed = spd->speed;
        if(curspeed == speed)
            return spd->bspeed;
        ++spd;
    }while(curspeed);
    WARNX(_("Wrong speed value: %d!"), speed);
    return 0;
}

/**
 * @brief tty_init - open & setup terminal
 * @param descr (io) - port descriptor
 * @return 0 if all OK or error code
 */
static int tty_init(TTY_descr *descr){
    // |O_NONBLOCK ?
    if ((descr->comfd = open(descr->portname, O_RDWR|O_NOCTTY)) < 0){
        /// Не могу использовать порт %s
        WARN(_("Can't use port %s"), descr->portname);
        return globErr ? globErr : 1;
    }
    if(tcgetattr(descr->comfd, &descr->oldtty) < 0){ // Get settings
        /// Не могу получить действующие настройки порта
        WARN(_("Can't get old TTY settings"));
        return globErr ? globErr : 1;
    }
    descr->tty = descr->oldtty;
    descr->tty.c_lflag     = 0; // ~(ICANON | ECHO | ECHOE | ISIG)
    descr->tty.c_iflag     = 0;
    descr->tty.c_oflag     = 0;
    descr->tty.c_cflag     = descr->baudrate|CS8|CREAD|CLOCAL; // 9.6k, 8N1, RW, ignore line ctrl
    descr->tty.c_cc[VMIN]  = 0;  // non-canonical mode
    descr->tty.c_cc[VTIME] = 5;
    if(tcsetattr(descr->comfd, TCSANOW, &descr->tty) < 0){
        /// Не могу сменить настройки порта
        WARN(_("Can't apply new TTY settings"));
        return globErr ? globErr : 1;
    }
    // make exclusive open
    if(descr->exclusive){
    if(ioctl(descr->comfd, TIOCEXCL)){
        /// Не могу сделать порт эксклюзивным
        WARN(_("Can't do exclusive open"));
    }}
    return 0;
}

/**
 * @brief restore_tty - restore opened TTY to previous state and close it
 */
void close_tty(TTY_descr **descr){
    if(descr == NULL || *descr == NULL) return;
    TTY_descr *d = *descr;
    if(d->comfd){
        ioctl(d->comfd, TCSANOW, &d->oldtty); // return TTY to previous state
        close(d->comfd);
    }
    FREE(d->portname);
    FREE(d->buf);
    FREE(*descr);
}

/**
 * @brief new_tty   - create new TTY structure with partially filled fields
 * @param comdev    - TTY device filename
 * @param speed     - speed (number)
 * @param bufsz     - size of buffer for input data (or 0 if opened only to write)
 * @return pointer to TTY structure if all OK
 */
TTY_descr *new_tty(char *comdev, int speed, size_t bufsz){
    tcflag_t spd = conv_spd(speed);
    if(!spd) return NULL;
    TTY_descr *descr = MALLOC(TTY_descr, 1);
    descr->portname = strdup(comdev);
    descr->baudrate = spd;
    descr->speed = speed;
    if(!descr->portname){
        /// Отсутствует имя порта
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
 * @brief tty_open  - init & open tty device
 * @param d         - already filled structure (with new_tty or by hands)
 * @param exclusive - == 1 to make exclusive open
 * @return pointer to TTY structure if all OK
 */
TTY_descr *tty_open(TTY_descr *d, int exclusive){
    if(!d || !d->portname || !d->baudrate) return NULL;
    if(exclusive) d->exclusive = TRUE;
    else d->exclusive = FALSE;
    if(tty_init(d)) return NULL;
    return d;
}

static struct timeval tvdefault = {.tv_sec = 0, .tv_usec = 5000};
/**
 * @brief tty_timeout - set timeout for select() on reading
 * @param usec - microseconds of timeout
 * @return -1 if usec < 0, 0 if all OK
 */
int tty_timeout(double usec){
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
 * @brief read_tty - read data from TTY with 10ms timeout
 * @param buff (o) - buffer for data read
 * @param length   - buffer len
 * @return amount of bytes read or -1 if disconnected
 */
int read_tty(TTY_descr *d){
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
 * @brief write_tty - write data to serial port
 * @param buff (i)  - data to write
 * @param length    - its length
 * @return 0 if all OK
 */
int write_tty(int comfd, const char *buff, size_t length){
    ssize_t L = write(comfd, buff, length);
    if((size_t)L != length){
        /// "Ошибка записи!"
        WARN("Write error");
        return 1;
    }
    return 0;
}

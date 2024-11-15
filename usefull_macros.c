/*                                                                                                  geany_encoding=koi8-r
 * usefull_macros.h - a set of usefull functions: memory, color etc
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

#include <ctype.h> // isspace
#include <err.h>
#include <fcntl.h>
#include <linux/limits.h> // PATH_MAX
#include <locale.h>
#include <math.h>         // floor
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h> // flock
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "usefull_macros.h"

/**
 * @brief signals - signal handler
 * @param sig - signal
 * default signal handler simply exits with `sig` status
 */
void __attribute__ ((weak)) signals(int sig){
    exit(sig);
}

/**
 * @brief sl_libversion - version
 * @return return string with library version
 */
const char *sl_libversion(){
    return PACKAGE_VERSION;
}

/**
 * @brief sl_dtime - function for different purposes that need to know time intervals
 * @return double value: UNIX time in seconds
 */
double sl_dtime(){
    double t;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    t = tv.tv_sec + ((double)tv.tv_usec)/1e6;
    return t;
}

/******************************************************************************\
 *                          Coloured terminal
\******************************************************************************/
int globErr = 0; // errno for WARN/ERR

/**
 * @brief r_pr_, g_pr_ - format red / green messages
 * @param fmt ... - printf-like format
 * @return number of printed symbols
 */
static int r_pr_(const char *fmt, ...){
    va_list ar; int i;
    printf(COLOR_RED);
    va_start(ar, fmt);
    i = vprintf(fmt, ar);
    va_end(ar);
    printf(COLOR_OLD);
    return i;
}
static int g_pr_(const char *fmt, ...){
    va_list ar; int i;
    printf(COLOR_GREEN);
    va_start(ar, fmt);
    i = vprintf(fmt, ar);
    va_end(ar);
    printf(COLOR_OLD);
    return i;
}

/**
 * @brief r_WARN - print red error/warning messages (if output is a tty)
 * @param fmt ... - printf-like format
 * @return number of printed symbols
 */
static int r_WARN(const char *fmt, ...){
    va_list ar; int i = 1;
    fprintf(stderr, COLOR_RED);
    va_start(ar, fmt);
    if(globErr){
        errno = globErr;
        vwarn(fmt, ar);
        errno = 0;
    }else
        i = vfprintf(stderr, fmt, ar);
    va_end(ar);
    i++;
    fprintf(stderr, COLOR_OLD "\n");
    return i;
}

// pointers to coloured output printf
int (*red)(const char *fmt, ...) = r_pr_;
int (*green)(const char *fmt, ...) = g_pr_;
int (*_WARN)(const char *fmt, ...) = r_WARN;

static const char stars[] = "****************************************";
/**
 * @brief s_WARN, r_pr_notty - notty variants of coloured printf
 * @param fmt ... - printf-like format
 * @return number of printed symbols
 */
static int s_WARN(const char *fmt, ...){
    va_list ar; int i;
    i = fprintf(stderr, "\n%s\n", stars);
    va_start(ar, fmt);
    if(globErr){
        errno = globErr;
        vwarn(fmt, ar);
        errno = 0;
    }else
        i = +vfprintf(stderr, fmt, ar);
    va_end(ar);
    i += fprintf(stderr, "\n%s\n", stars);
    i += fprintf(stderr, "\n");
    return i;
}
static int r_pr_notty(const char *fmt, ...){
    va_list ar; int i;
    i = printf("\n%s\n", stars);
    va_start(ar, fmt);
    i += vprintf(fmt, ar);
    va_end(ar);
    i += printf("\n%s\n", stars);
    return i;
}

/**
 * @brief sl_init - setup locale & console
 * Run this function in the beginning of main() to setup locale & coloured output
 */
void sl_init(){
    // setup coloured output
    if(isatty(STDOUT_FILENO)){ // make color output in tty
        red = r_pr_; green = g_pr_;
    }else{ // no colors in case of pipe
        red = r_pr_notty; green = printf;
    }
    if(isatty(STDERR_FILENO)) _WARN = r_WARN;
    else _WARN = s_WARN;
    // Setup locale
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
#if defined GETTEXT_PACKAGE && defined LOCALEDIR
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    textdomain(GETTEXT_PACKAGE);
#endif
}

/******************************************************************************\
 *                                  Different things
\******************************************************************************/

/**
* @brief sl_random_seed - Generate a quasy-random number to initialize PRNG
* @return value for srand48
*/
long sl_random_seed(){
    long r_ini;
    int fail = 0;
    int fd = open("/dev/random", O_RDONLY);
    do{
        if(-1 == fd){
            WARN(_("Can't open /dev/random"));
            fail = 1; break;
        }
        if(sizeof(long) != read(fd, &r_ini, sizeof(long))){
            WARN(_("Can't read /dev/random"));
            fail = 1;
        }
        close(fd);
    }while(0);
    if(fail){
        double tt = sl_dtime() * 1e6;
        double mx = (double)LONG_MAX;
        r_ini = (long)(tt - mx * floor(tt/mx));
    }
    return (r_ini);
}

/**
 * @brief sl_mem_avail
 * @return system available physical memory
 */
uint64_t sl_mem_avail(){
    return sysconf(_SC_AVPHYS_PAGES) * (uint64_t) sysconf(_SC_PAGE_SIZE);
}



/******************************************************************************\
 *                                  Memory
\******************************************************************************/
/**
 * @brief sl_alloc - safe memory allocation for macro ALLOC
 * @param N - number of elements to allocate
 * @param S - size of single element (typically sizeof)
 * @return pointer to allocated memory area
 */
void *sl_alloc(size_t N, size_t S){
    void *p = calloc(N, S);
    if(!p) ERR("malloc");
    //assert(p);
    return p;
}

/**
 * @brief sl_mmap - mmap file to a memory area
 * @param filename (i) - name of file to mmap
 * @return stuct with mmap'ed file or die
 */
sl_mmapbuf_t *sl_mmap(char *filename){
    int fd;
    char *ptr;
    size_t Mlen;
    struct stat statbuf;
    if(!filename){
        WARNX(_("No filename given!"));
        return NULL;
    }
    if((fd = open(filename, O_RDONLY)) < 0){
        WARN(_("Can't open %s for reading"), filename);
        return NULL;
    }
    if(fstat (fd, &statbuf) < 0){
        WARN(_("Can't stat %s"), filename);
        close(fd);
        return NULL;
    }
    Mlen = statbuf.st_size;
    if((ptr = mmap (0, Mlen, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED){
        WARN(_("Mmap error for input"));
        close(fd);
        return NULL;
    }
    if(close(fd)) WARN(_("Can't close mmap'ed file"));
    sl_mmapbuf_t *ret = MALLOC(sl_mmapbuf_t, 1);
    ret->data = ptr;
    ret->len = Mlen;
    return  ret;
}

/**
 * @brief sl_munmap - unmap memory file
 * @param b (i) - mmap'ed buffer
 */
void sl_munmap(sl_mmapbuf_t *b){
    if(munmap(b->data, b->len)){
        ERR(_("Can't munmap"));
    }
    FREE(b);
}

/**
 * @brief sl_omitspaces - omit leading spaces
 * @param v - source string
 * @return pointer to first non-blank character (could be '\0' if end of string reached)
 */
char *sl_omitspaces(const char *v){
    if(!v) return NULL;
    char *p = (char*)v;
    while(*p && isspace(*p)) ++p;
    return p;
}

// the same as sl_omitspaces, but return (last non-space char + 1) in string or its first char
char *sl_omitspacesr(const char *v){
    if(!v) return NULL;
    char *eol = (char*)v + strlen(v) - 1;
    while(eol > v && isspace(*eol)) --eol;
    if(eol == v && isspace(*eol)) return eol;
    return eol + 1;
}


/******************************************************************************\
 *                          Terminal in no-echo mode
 * BE CAREFULL! These functions aren't thread-safe!
\******************************************************************************/
// console flags
#ifndef SL_USE_OLD_TTY
static struct termios2 oldt, newt;
#else
static struct termios oldt, newt;
#endif
static int console_changed = 0;
/**
 * @brief sl_restore_con - restore console to default mode
 */
void sl_restore_con(){
    if(console_changed)
#ifndef SL_USE_OLD_TTY
        ioctl(STDIN_FILENO, TCSETS2, &oldt);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // return terminal to previous state
#endif
    console_changed = 0;
}

/**
 * @brief sl_setup_con - setup console to non-canonical noecho mode
 */
void sl_setup_con(){
    if(console_changed) return;
#ifndef SL_USE_OLD_TTY
        ioctl(STDIN_FILENO, TCGETS2, &oldt);
#else
    tcgetattr(STDIN_FILENO, &oldt);
#endif
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
#ifndef SL_USE_OLD_TTY
        if(ioctl(STDIN_FILENO, TCSETS2, &newt)){
#else
    if(tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0){
#endif
        WARN(_("Can't setup console"));
#ifndef SL_USE_OLD_TTY
        ioctl(STDIN_FILENO, TCSETS2, &oldt);
#else
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
        signals(1); //quit?
    }
    console_changed = 1;
}

/**
 * @brief sl_read_con - read character from console without echo
 * @return char read or zero
 */
int sl_read_con(){
    int rb;
    struct timeval tv;
    int retval;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    tv.tv_sec = 0; tv.tv_usec = 10000;
    retval = select(1, &rfds, NULL, NULL, &tv);
    if(!retval) rb = 0;
    else {
        if(FD_ISSET(STDIN_FILENO, &rfds)) rb = getchar();
        else rb = 0;
    }
    return rb;
}

/**
 * @brief my - getchar() without echo
 * wait until at least one character pressed
 * @return character read
 */
int sl_getchar(){
    int ret;
    do ret = sl_read_con();
    while(ret == 0);
    return ret;
}

/**
 * @brief sl_str2d - safely convert data from string to double
 * @param num (o) - double number read from string
 * @param str (i) - input string
 * @return 1 if success, 0 if fails
 */
int sl_str2d(double *num, const char *str){
    double res;
    char *endptr;
    if(!str) return FALSE;
    res = strtod(str, &endptr);
    if(endptr == str || *str == '\0' || *endptr != '\0'){
        WARNX(_("Wrong double number format '%s'"), str);
        return FALSE;
    }
    if(num) *num = res; // you may run it like myatod(NULL, str) to test wether str is double number
    return TRUE;
}
// and so on
int sl_str2ll(long long *num, const char *str){
    long long res;
    char *endptr;
    if(!str) return FALSE;
    res = strtoll(str, &endptr, 0);
    if(endptr == str || *str == '\0' || *endptr != '\0'){
        WARNX(_("Wrong integer number format '%s'"));
        return FALSE;
    }
    if(num) *num = res;
    return TRUE;
}

/******************************************************************************\
 *                              Logging to file
\******************************************************************************/
sl_log_t *sl_globlog = NULL; // "global" log file (the first opened logfile)
/**
 * @brief sl_createlog - create log file, test file open ability
 * @param logpath - path to log file
 * @param level   - lowest message level (e.g. LOGLEVEL_ERR won't allow to write warn/msg/dbg)
 * @return allocated structure (should be free'd later by Cl_deletelog) or NULL
 */
sl_log_t *sl_createlog(const char *logpath, sl_loglevel_e level, int prefix){
    if(level < LOGLEVEL_NONE || level > LOGLEVEL_ANY) return NULL;
    if(!logpath) return NULL;
    FILE *logfd = fopen(logpath, "a");
    if(!logfd){
        WARN("Can't open log file");
        return NULL;
    }
    fclose(logfd);
    sl_log_t *log = MALLOC(sl_log_t, 1);
    log->logpath = strdup(logpath);
    if(!log->logpath){
        WARN("strdup()");
        FREE(log);
        return NULL;
    }
    log->loglevel = level;
    log->addprefix = prefix;
    return log;
}

void sl_deletelog(sl_log_t **log){
    if(!log || !*log) return;
    FREE((*log)->logpath);
    FREE(*log);
}

/**
 * @brief sl_putlog - put message to log file with/without timestamp
 * @param timest - ==1 to put timestamp
 * @param log - pointer to log structure
 * @param lvl - message loglevel (if lvl > loglevel, message won't be printed)
 * @param fmt - format and the rest part of message
 * @return amount of symbols saved in file
 */
int sl_putlogt(int timest, sl_log_t *log, sl_loglevel_e lvl, const char *fmt, ...){
    if(!log || !log->logpath) return 0;
    if(lvl > log->loglevel) return 0;
    int i = 0;
    FILE *logfd = fopen(log->logpath, "a+");
    if(!logfd) return 0;
    int lfd = fileno(logfd);
    // try to lock file
    double t0 = sl_dtime();
    int locked = 0;
    while(sl_dtime() - t0 < 0.1){ // timeout for 0.1s
        if(-1 == flock(lfd, LOCK_EX | LOCK_NB)) continue;
        locked = 1;
        break;
    }
    if(!locked) return 0; // can't lock
    if(log->addprefix){
        const char *p;
        switch(lvl){
            case LOGLEVEL_ERR:
                p = "[ERR]";
            break;
            case LOGLEVEL_WARN:
                p = "[WARN]";
            break;
            case LOGLEVEL_MSG:
                p = "[MSG]";
            break;
            case LOGLEVEL_DBG:
                p = "[DBG]";
            break;
            default:
                p = NULL;
        }
        if(p) i += fprintf(logfd, "%s\t", p);
    }
    if(timest){
        char strtm[128];
        time_t t = time(NULL);
        struct tm *curtm = localtime(&t);
        strftime(strtm, 128, "%Y/%m/%d-%H:%M:%S", curtm);
        i = fprintf(logfd, "%s", strtm);
    }
    i += fprintf(logfd, "\t");
    va_list ar;
    va_start(ar, fmt);
    i += vfprintf(logfd, fmt, ar);
    va_end(ar);
    fseek(logfd, -1, SEEK_CUR);
    char c;
    ssize_t r = fread(&c, 1, 1, logfd);
    if(1 == r){ // add '\n' if there was no newline
        if(c != '\n') i += fprintf(logfd, "\n");
    }
    flock(lfd, LOCK_UN);
    fclose(logfd);
    return i;
}

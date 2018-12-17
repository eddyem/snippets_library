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
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <err.h>
#include <locale.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <linux/limits.h> // PATH_MAX
#include <math.h>         // floor

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
 * @brief dtime - function for different purposes that need to know time intervals
 * @return double value: UNIX time in seconds
 */
double dtime(){
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
int r_pr_(const char *fmt, ...){
    va_list ar; int i;
    printf(COLOR_RED);
    va_start(ar, fmt);
    i = vprintf(fmt, ar);
    va_end(ar);
    printf(COLOR_OLD);
    return i;
}
int g_pr_(const char *fmt, ...){
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
int r_WARN(const char *fmt, ...){
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
int s_WARN(const char *fmt, ...){
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
int r_pr_notty(const char *fmt, ...){
    va_list ar; int i;
    i = printf("\n%s\n", stars);
    va_start(ar, fmt);
    i += vprintf(fmt, ar);
    va_end(ar);
    i += printf("\n%s\n", stars);
    return i;
}

/**
 * @brief initial_setup - setup locale & console
 * Run this function in the beginning of main() to setup locale & coloured output
 */
void initial_setup(){
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
* @brief throw_random_seed - Generate a quasy-random number to initialize PRNG
* @return value for srand48
*/
long throw_random_seed(){
    long r_ini;
    int fail = 0;
    int fd = open("/dev/random", O_RDONLY);
    do{
        if(-1 == fd){
            /// Не могу открыть /dev/random
            WARN(_("Can't open /dev/random"));
            fail = 1; break;
        }
        if(sizeof(long) != read(fd, &r_ini, sizeof(long))){
            /// Не могу прочесть /dev/random
            WARN(_("Can't read /dev/random"));
            fail = 1;
        }
        close(fd);
    }while(0);
    if(fail){
        double tt = dtime() * 1e6;
        double mx = (double)LONG_MAX;
        r_ini = (long)(tt - mx * floor(tt/mx));
    }
    return (r_ini);
}

/**
 * @brief get_available_mem
 * @return system available physical memory
 */
uint64_t get_available_mem(){
    return sysconf(_SC_AVPHYS_PAGES) * (uint64_t) sysconf(_SC_PAGE_SIZE);
}



/******************************************************************************\
 *                                  Memory
\******************************************************************************/
/**
 * @brief my_alloc - safe memory allocation for macro ALLOC
 * @param N - number of elements to allocate
 * @param S - size of single element (typically sizeof)
 * @return pointer to allocated memory area
 */
void *my_alloc(size_t N, size_t S){
    void *p = calloc(N, S);
    if(!p) ERR("malloc");
    //assert(p);
    return p;
}

/**
 * @brief My_mmap - mmap file to a memory area
 * @param filename (i) - name of file to mmap
 * @return stuct with mmap'ed file or die
 */
mmapbuf *My_mmap(char *filename){
    int fd;
    char *ptr;
    size_t Mlen;
    struct stat statbuf;
    if(!filename){
        /// Не задано имя файла!
        WARNX(_("No filename given!"));
        return NULL;
    }
    if((fd = open(filename, O_RDONLY)) < 0){
        /// Не могу открыть %s для чтения
        WARN(_("Can't open %s for reading"), filename);
        return NULL;
    }
    if(fstat (fd, &statbuf) < 0){
        /// Не могу выполнить stat %s
        WARN(_("Can't stat %s"), filename);
        close(fd);
        return NULL;
    }
    Mlen = statbuf.st_size;
    if((ptr = mmap (0, Mlen, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED){
        /// Ошибка mmap
        WARN(_("Mmap error for input"));
        close(fd);
        return NULL;
    }
    /// Не могу закрыть mmap'нутый файл
    if(close(fd)) WARN(_("Can't close mmap'ed file"));
    mmapbuf *ret = MALLOC(mmapbuf, 1);
    ret->data = ptr;
    ret->len = Mlen;
    return  ret;
}

/**
 * @brief My_munmap - unmap memory file
 * @param b (i) - mmap'ed buffer
 */
void My_munmap(mmapbuf *b){
    if(munmap(b->data, b->len)){
        /// Не могу munmap
        ERR(_("Can't munmap"));
    }
    FREE(b);
}


/******************************************************************************\
 *                          Terminal in no-echo mode
 * BE CAREFULL! These functions aren't thread-safe!
\******************************************************************************/
static struct termios oldt, newt; // console flags
static int console_changed = 0;
/**
 * @brief restore_console - restore console to default mode
 */
void restore_console(){
    if(console_changed)
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // return terminal to previous state
    console_changed = 0;
}

/**
 * @brief setup_con - setup console to non-canonical noecho mode
 */
void setup_con(){
    if(console_changed) return;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    if(tcsetattr(STDIN_FILENO, TCSANOW, &newt) < 0){
        /// Не могу настроить консоль
        WARN(_("Can't setup console"));
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        signals(1); //quit?
    }
    console_changed = 1;
}

/**
 * @brief read_console - read character from console without echo
 * @return char read or zero
 */
int read_console(){
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
int mygetchar(){
    int ret;
    do ret = read_console();
    while(ret == 0);
    return ret;
}

/**
 * @brief str2double - safely convert data from string to double
 * @param num (o) - double number read from string
 * @param str (i) - input string
 * @return 1 if success, 0 if fails
 */
int str2double(double *num, const char *str){
    double res;
    char *endptr;
    if(!str) return 0;
    res = strtod(str, &endptr);
    if(endptr == str || *str == '\0' || *endptr != '\0'){
        /// "Неправильный формат числа double!"
        WARNX("Wrong double number format!");
        return FALSE;
    }
    if(num) *num = res; // you may run it like myatod(NULL, str) to test wether str is double number
    return TRUE;
}

/******************************************************************************\
 *                              Logging to file
 * BE CAREFULL!!! There's only one log file per process!
\******************************************************************************/
FILE *Flog = NULL; // log file descriptor
char *logname = NULL;
time_t log_open_time = 0;
/**
 * @brief openlogfile - try to open log file
 * @param name (i)    - log file name
 * if failed show warning message
 */
void openlogfile(char *name){
    if(!name){
        /// Не задано имя логфайла
        WARNX(_("Need filename for log file"));
        return;
    }
    /// Пробую открыть логфайл %s в режиме дополнения\n
    green(_("Try to open log file %s in append mode\n"), name);
    fflush(stdout);
    if(!(Flog = fopen(name, "a"))){
        /// Не могу открыть логфайл
        WARN(_("Can't open log file"));
        return;
    }
    log_open_time = time(NULL);
    logname = name;
}

/**
 * Save message to log file, rotate logs every 24 hours
 */
int putlog(const char *fmt, ...){
    if(!Flog) return 0;
    time_t t_now = time(NULL);
    if(t_now - log_open_time > 86400){ // rotate log
        fprintf(Flog, "\n\t\t%sRotate log\n", ctime(&t_now));
        fclose(Flog);
        char newname[PATH_MAX];
        snprintf(newname, PATH_MAX, "%s.old", logname);
        if(rename(logname, newname)) WARN("rename()");
        openlogfile(logname);
        if(!Flog) return 0;
    }
    //int i = fprintf(Flog, "%s\t\t", ctime(&t_now));
    char buf[256];
    strftime(buf, 255, "%Y/%m/%d %H:%M:%S", localtime(&t_now));
    int i = fprintf(Flog, "%s\t\t", buf);
    va_list ar;
    va_start(ar, fmt);
    i = vfprintf(Flog, fmt, ar);
    va_end(ar);
    fprintf(Flog, "\n");
    fflush(Flog);
    return i;
}


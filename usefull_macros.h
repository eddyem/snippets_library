/*                                                                                                  geany_encoding=koi8-r
 * usefull_macros.h - a set of usefull macros: memory, color etc
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

#pragma once
#ifndef __USEFULL_MACROS_H__
#define __USEFULL_MACROS_H__

#include <stdbool.h>        // bool
#include <unistd.h>         // pid_t
#include <errno.h>          // errno
#include <termios.h>        // termios
#include <stdlib.h>         // alloc, free
// just for different purposes
#include <limits.h>
#include <stdint.h>

#if defined GETTEXT
/*
 * GETTEXT
 */
#include <libintl.h>
#define _(String)               gettext(String)
#define gettext_noop(String)    String
#define N_(String)              gettext_noop(String)
#else
#define _(String)               (String)
#define N_(String)              (String)
#endif

/******************************************************************************\
                         The original usefull_macros.h
\******************************************************************************/

// unused arguments of functions
#define _U_         __attribute__((__unused__))
// break absent in `case`
#define FALLTHRU    __attribute__ ((fallthrough))
// and synonym for FALLTHRU
#define NOBREAKHERE __attribute__ ((fallthrough))
// weak functions
#define WEAK        __attribute__ ((weak))

/*
 * Coloured messages output
 */
#define COLOR_RED           "\033[1;31;40m"
#define COLOR_GREEN         "\033[1;32;40m"
#define COLOR_OLD           "\033[0;0;0m"

#ifndef FALSE
#define FALSE (0)
#endif

#ifndef TRUE
#define TRUE (1)
#endif

/*
 * ERROR/WARNING messages
 */
// global error
extern int globErr;
void WEAK signals(int sig);
#define ERR(...) do{globErr=errno; _WARN(__VA_ARGS__); signals(9);}while(0)
#define ERRX(...) do{globErr=0; _WARN(__VA_ARGS__); signals(9);}while(0)
#define WARN(...) do{globErr=errno; _WARN(__VA_ARGS__);}while(0)
#define WARNX(...) do{globErr=0; _WARN(__VA_ARGS__);}while(0)

/*
 * print function name, debug messages
 * debug mode, -DEBUG
 */
#ifdef EBUG
    #define FNAME() do{ fprintf(stderr, COLOR_OLD); \
        fprintf(stderr, "\n%s (%s, line %d)\n", __func__, __FILE__, __LINE__);} while(0)
    #define DBG(...) do{ fprintf(stderr, COLOR_OLD); \
                    fprintf(stderr, "%s (%s, line %d): ", __func__, __FILE__, __LINE__); \
                    fprintf(stderr, __VA_ARGS__);           \
                    fprintf(stderr, "\n");} while(0)
#else
    #define FNAME()  do{}while(0)
    #define DBG(...) do{}while(0)
#endif //EBUG

/*
 * Memory allocation
 */
#define ALLOC(type, var, size)  type * var = ((type *)my_alloc(size, sizeof(type)))
#define MALLOC(type, size) ((type *)my_alloc(size, sizeof(type)))
#define FREE(ptr)  do{if(ptr){free(ptr); ptr = NULL;}}while(0)

#ifndef DBL_EPSILON
#define DBL_EPSILON        (2.2204460492503131e-16)
#endif

// double value of UNIX time
double dtime();

// functions for color output in tty & no-color in pipes
extern int (*red)(const char *fmt, ...);
extern int (*_WARN)(const char *fmt, ...);
extern int (*green)(const char *fmt, ...);
// safe allocation
void * my_alloc(size_t N, size_t S);
// setup locales & other
void initial_setup();

// mmap file
typedef struct{
    char *data;
    size_t len;
} mmapbuf;
mmapbuf *My_mmap(char *filename);
void My_munmap(mmapbuf *b);

// console in non-echo mode
void restore_console();
void setup_con();
int read_console();
int mygetchar();

long throw_random_seed();

uint64_t get_available_mem();

/******************************************************************************\
                         The original term.h
\******************************************************************************/
typedef struct {
    char *portname;         // device filename (should be freed before structure freeing)
    int speed;              // baudrate in human-readable format
    tcflag_t baudrate;      // baudrate (B...)
    struct termios oldtty;  // TTY flags for previous port settings
    struct termios tty;     // TTY flags for current settings
    int comfd;              // TTY file descriptor
    char *buf;              // buffer for data read
    size_t bufsz;           // size of buf
    size_t buflen;          // length of data read into buf
    bool exclusive;         // should device be exclusive opened
} TTY_descr;

void close_tty(TTY_descr **descr);
TTY_descr *new_tty(char *comdev, int speed, size_t bufsz);
TTY_descr *tty_open(TTY_descr *d, int exclusive);
size_t read_tty(TTY_descr *descr);
int write_tty(int comfd, const char *buff, size_t length);
tcflag_t conv_spd(int speed);

// convert string to double with checking
int str2double(double *num, const char *str);

// logging
void openlogfile(char *name);
int putlog(const char *fmt, ...);

/******************************************************************************\
                         The original parseargs.h
\******************************************************************************/
// macro for argptr
#define APTR(x)   ((void*)x)

// if argptr is a function:
typedef  bool(*argfn)(void *arg);

/**
 * type of getopt's argument
 * WARNING!
 * My function change value of flags by pointer, so if you want to use another type
 * make a latter conversion, example:
 *      char charg;
 *      int iarg;
 *      myoption opts[] = {
 *      {"value", 1, NULL, 'v', arg_int, &iarg, "char val"}, ..., end_option};
 *      ..(parse args)..
 *      charg = (char) iarg;
 */
typedef enum {
    arg_none = 0,   // no arg
    arg_int,        // integer
    arg_longlong,   // long long
    arg_double,     // double
    arg_float,      // float
    arg_string,     // char *
    arg_function    // parse_args will run function `bool (*fn)(char *optarg, int N)`
} argtype;

/**
 * Structure for getopt_long & help
 * BE CAREFUL: .argptr is pointer to data or pointer to function,
 *      conversion depends on .type
 *
 * ATTENTION: string `help` prints through macro PRNT(), bu default it is gettext,
 * but you can redefine it before `#include "parseargs.h"`
 *
 * if arg is string, then value wil be strdup'ed like that:
 *      char *str;
 *      myoption opts[] = {{"string", 1, NULL, 's', arg_string, &str, "string val"}, ..., end_option};
 *      *(opts[1].str) = strdup(optarg);
 * in other cases argptr should be address of some variable (or pointer to allocated memory)
 *
 * NON-NULL argptr should be written inside macro APTR(argptr) or directly: (void*)argptr
 *
 * !!!LAST VALUE OF ARRAY SHOULD BE `end_option` or ZEROS !!!
 *
 */
typedef enum{
    NO_ARGS  = 0,  // first three are the same as in getopt_long
    NEED_ARG = 1,
    OPT_ARG  = 2,
    MULT_PAR
} hasarg;

typedef struct{
    // these are from struct option:
    const char *name;       // long option's name
    hasarg      has_arg;    // 0 - no args, 1 - nesessary arg, 2 - optionally arg, 4 - need arg & key can repeat (args are stored in null-terminated array)
    int        *flag;       // NULL to return val, pointer to int - to set its value of val (function returns 0)
    int         val;        // short opt name (if flag == NULL) or flag's value
    // and these are mine:
    argtype     type;       // type of argument
    void       *argptr;     // pointer to variable to assign optarg value or function `bool (*fn)(char *optarg, int N)`
    const char *help;       // help string which would be shown in function `showhelp` or NULL
} myoption;

/*
 * Suboptions structure, almost the same like myoption
 * used in parse_subopts()
 */
typedef struct{
    const char *name;
    hasarg      has_arg;
    argtype     type;
    void       *argptr;
} mysuboption;

// last string of array (all zeros)
#define end_option {0,0,0,0,0,0,0}
#define end_suboption {0,0,0,0}

extern const char *__progname;

void showhelp(int oindex, myoption *options);
void parseargs(int *argc, char ***argv, myoption *options);
void change_helpstring(char *s);
bool get_suboption(char *str, mysuboption *opt);


/******************************************************************************\
                         The original daemon.h
\******************************************************************************/
#ifndef PROC_BASE
#define PROC_BASE "/proc"
#endif

// default function to run if another process found
void WEAK iffound_default(pid_t pid);
// check that our process is exclusive
void check4running(char *selfname, char *pidfilename);
// read name of process by its PID
char *readPSname(pid_t pid);
#endif // __USEFULL_MACROS_H__

/******************************************************************************\
                         The original fifo_lifo.h
\******************************************************************************/
typedef struct buff_node{
    void *data;
    struct buff_node *next, *last;
} List;

List *list_push_tail(List **lst, void *v);
List *list_push(List **lst, void *v);
void *list_pop(List **lst);

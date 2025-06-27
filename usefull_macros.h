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

#include <stdio.h>
#ifdef SL_USE_OLD_TTY
#include <termios.h>        // termios
#else
#include <asm-generic/termbits.h>
#endif

#include <errno.h>          // errno
#include <netdb.h>          // struct addrinfo
#include <pthread.h>
#include <stdlib.h>         // alloc, free
#include <sys/types.h>      // pid_t
#include <unistd.h>         // pid_t
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
#define ALLOC(type, var, size)  type * var = ((type *)sl_alloc(size, sizeof(type)))
#define MALLOC(type, size) ((type *)sl_alloc(size, sizeof(type)))
#define FREE(ptr)  do{if(ptr){free(ptr); ptr = NULL;}}while(0)

#ifndef DBL_EPSILON
#define DBL_EPSILON        (2.2204460492503131e-16)
#endif

// library version
const char *sl_libversion();

// double value of UNIX time
double sl_dtime();

// functions for color output in tty & no-color in pipes
extern int (*red)(const char *fmt, ...);
extern int (*_WARN)(const char *fmt, ...);
extern int (*green)(const char *fmt, ...);
// safe allocation
void *sl_alloc(size_t N, size_t S);
// setup locales & other
void sl_init();

// mmap file
typedef struct{
    char *data;
    size_t len;
} sl_mmapbuf_t;
sl_mmapbuf_t *sl_mmap(char *filename);
void sl_munmap(sl_mmapbuf_t *b);

// console in non-echo mode
void sl_restore_con();
void sl_setup_con();
int sl_read_con();
int sl_getchar();

long sl_random_seed();

uint64_t sl_mem_avail();

// omit leading spaces
char *sl_omitspaces(const char *str);
// omit trailing spaces
char *sl_omitspacesr(const char *v);

// convert string to double/integer with checking
int sl_str2d(double *num, const char *str);
int sl_str2ll(long long *num, const char *str);
int sl_str2i(int *num, const char *str);

int sl_canread(int fd);
int sl_canwrite(int fd);

/******************************************************************************\
                         The original term.h
\******************************************************************************/
#ifdef SL_USE_OLD_TTY
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
    int exclusive;          // should device be exclusive opened
} sl_tty_t;

tcflag_t sl_tty_convspd(int speed);
#else
typedef struct {
    char *portname;         // device filename (should be freed before structure freeing)
    int speed;              // baudrate in human-readable format
    char *format;           // format like 8N1
    struct termios2 oldtty; // TTY flags for previous port settings
    struct termios2 tty;    // TTY flags for current settings
    int comfd;              // TTY file descriptor
    char *buf;              // buffer for data read
    size_t bufsz;           // size of buf
    size_t buflen;          // length of data read into buf
    int exclusive;          // should device be exclusive opened
} sl_tty_t;
#endif

void sl_tty_close(sl_tty_t **descr);
sl_tty_t *sl_tty_new(char *comdev, int speed, size_t bufsz);
sl_tty_t *sl_tty_open(sl_tty_t *d, int exclusive);
int sl_tty_read(sl_tty_t *descr);
int sl_tty_tmout(double usec);
int sl_tty_write(int comfd, const char *buff, size_t length);

/******************************************************************************\
                                 Logging
\******************************************************************************/
typedef enum{
    LOGLEVEL_NONE,  // no logs
    LOGLEVEL_ERR,   // only errors
    LOGLEVEL_WARN,  // only warnings and errors
    LOGLEVEL_MSG,   // all without debug
    LOGLEVEL_DBG,   // all messages
    LOGLEVEL_ANY,   // all shit
    LOGLEVEL_AMOUNT // total amount
} sl_loglevel_e;

typedef struct{
    char *logpath;          // full path to logfile
    sl_loglevel_e loglevel; // loglevel
    int addprefix;          // if !=0 add record type to each line(e.g. [ERR])
} sl_log_t;

extern sl_log_t *sl_globlog; // "global" log file

sl_log_t *sl_createlog(const char *logpath, sl_loglevel_e level, int prefix);
void sl_deletelog(sl_log_t **log);
int sl_putlogt(int timest, sl_log_t *log, sl_loglevel_e lvl, const char *fmt, ...);
// open "global" log
#define OPENLOG(nm, lvl, prefix)   (sl_globlog = sl_createlog(nm, lvl, prefix))
// shortcuts for different log levels; ..ADD - add message without timestamp
#define LOGERR(...)     do{sl_putlogt(1, sl_globlog, LOGLEVEL_ERR, __VA_ARGS__);}while(0)
#define LOGERRADD(...)  do{sl_putlogt(0, sl_globlog, LOGLEVEL_ERR, __VA_ARGS__);}while(0)
#define LOGWARN(...)    do{sl_putlogt(1, sl_globlog, LOGLEVEL_WARN, __VA_ARGS__);}while(0)
#define LOGWARNADD(...) do{sl_putlogt(0, sl_globlog, LOGLEVEL_WARN, __VA_ARGS__);}while(0)
#define LOGMSG(...)     do{sl_putlogt(1, sl_globlog, LOGLEVEL_MSG, __VA_ARGS__);}while(0)
#define LOGMSGADD(...)  do{sl_putlogt(0, sl_globlog, LOGLEVEL_MSG, __VA_ARGS__);}while(0)
#define LOGDBG(...)     do{sl_putlogt(1, sl_globlog, LOGLEVEL_DBG, __VA_ARGS__);}while(0)
#define LOGDBGADD(...)  do{sl_putlogt(0, sl_globlog, LOGLEVEL_DBG, __VA_ARGS__);}while(0)

/******************************************************************************\
                         The original parseargs.h
\******************************************************************************/
// macro for argptr
#define APTR(x)   ((void*)x)

// if argptr is a function:
typedef  int(*sl_argfn_t)(void *arg);

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
    arg_function    // parse_args will run function `int (*fn)(char *optarg, int N)`
} sl_argtype_e;

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
} sl_hasarg_e;

typedef struct{
    // these are from struct option:
    const char *name;       // long option's name
    sl_hasarg_e has_arg;    // 0 - no args, 1 - nesessary arg, 2 - optionally arg, 3 - need arg & key can repeat (args are stored in null-terminated array)
    int        *flag;       // NULL to return val, pointer to int - to set its value of val (function returns 0)
    int         val;        // short opt name (if flag == NULL) or flag's value
    // and these are mine:
    sl_argtype_e type;      // type of argument
    void       *argptr;     // pointer to variable to assign optarg value or function `int (*fn)(char *optarg, int N)`
    const char *help;       // help string which would be shown in function `showhelp` or NULL
} sl_option_t;

/*
 * Suboptions structure, almost the same like sl_option_t
 * used in parse_subopts()
 */
typedef struct{
    const char  *name;
    sl_hasarg_e  has_arg;
    sl_argtype_e type;
    void        *argptr;
} sl_suboption_t;

// last string of array (all zeros)
#define end_option {0,0,0,0,0,0,0}
#define end_suboption {0,0,0,0}

extern const char *__progname;

void sl_showhelp(int oindex, sl_option_t *options);
void sl_parseargs(int *argc, char ***argv, sl_option_t *options);
/**
 * @brief sl_helpstring - change standard help header
 * @param str (i) - new format (MAY consist ONE "%s" for progname)
 */
void sl_helpstring(char *s);
int sl_get_suboption(char *str, sl_suboption_t *opt);


/******************************************************************************\
                         The original daemon.h
\******************************************************************************/
#ifndef PROC_BASE
#define PROC_BASE "/proc"
#endif

// default function to run if another process found
void WEAK sl_iffound_deflt(pid_t pid);
// check that our process is exclusive
void sl_check4running(char *selfname, char *pidfilename);
// read name of process by its PID
char *sl_getPSname(pid_t pid);

/******************************************************************************\
                         The original fifo_lifo.h
\******************************************************************************/
typedef struct sl_buff_node{
    void *data;
    struct sl_buff_node *next, *last;
} sl_list_t;

sl_list_t *sl_list_push_tail(sl_list_t **lst, void *v);
sl_list_t *sl_list_push(sl_list_t **lst, void *v);
void *sl_list_pop(sl_list_t **lst);

/******************************************************************************\
                         The original config.h
\******************************************************************************/

// max length of key (including '\0')
#define SL_KEY_LEN      (32)
// max length of value (including '\0')
#define SL_VAL_LEN      (128)

// starting symbol of any comment
#define SL_COMMENT_CHAR '#'

// option or simple configuration value (don't work for functions)
typedef struct{
    union{
        int ival; long long llval; double dval; float fval;
    };
    sl_argtype_e type;
} sl_optval;

int sl_get_keyval(const char *pair, char key[SL_KEY_LEN], char value[SL_VAL_LEN]);
char *sl_print_opts(sl_option_t *opt, int showall);
int sl_conf_readopts(const char *filename, sl_option_t *options);

/******************************************************************************\
                         The original ringbuffer.h
\******************************************************************************/

// ring buffer for string or binary data
typedef struct{
    uint8_t *data;          // data buffer
    size_t length;          // its length
    size_t head;               // head index
    size_t tail;               // tail index
    pthread_mutex_t busy;   // mutex of buffer activity
} sl_ringbuffer_t;

sl_ringbuffer_t *sl_RB_new(size_t size);
void sl_RB_delete(sl_ringbuffer_t **b);
size_t sl_RB_read(sl_ringbuffer_t *b, uint8_t *s, size_t len);
ssize_t sl_RB_readto(sl_ringbuffer_t *b, uint8_t byte, uint8_t *s, size_t len);
ssize_t sl_RB_hasbyte(sl_ringbuffer_t *b, uint8_t byte);
int sl_RB_putbyte(sl_ringbuffer_t *b, uint8_t byte);
size_t sl_RB_write(sl_ringbuffer_t *b, const uint8_t *str, size_t len);
size_t sl_RB_datalen(sl_ringbuffer_t *b);
size_t sl_RB_freesize(sl_ringbuffer_t *b);
void sl_RB_clearbuf(sl_ringbuffer_t *b);
ssize_t sl_RB_readline(sl_ringbuffer_t *b, char *s, size_t len);
size_t sl_RB_writestr(sl_ringbuffer_t *b, char *s);

/******************************************************************************\
                         The original socket.h
\******************************************************************************/

// handler result: what to send to client
typedef enum{
    RESULT_OK,      // all OK
    RESULT_FAIL,    // failed running command
    RESULT_BADVAL,  // bad value
    RESULT_BADKEY,  // bad (non-existant) key
    RESULT_SILENCE, // send nothing to client (in case of handlers which sends data by themself)
    RESULT_AMOUNT  // total amount of enum fields
} sl_sock_hresult_e;

// data types with timestamp
typedef struct{
    double timestamp; // time of last change
    double val;
} sl_sock_double_t;

typedef struct{
    double timestamp; // time of last change
    int64_t val;
} sl_sock_int_t;

typedef struct{
    double timestamp; // time of last change
    char val[SL_VAL_LEN];
    int len;        // strlen of `val`
} sl_sock_string_t;

struct sl_sock_hitem;
struct sl_sock;

// socket `key` handlers
typedef sl_sock_hresult_e (*sl_sock_msghandler)(struct sl_sock *s, struct sl_sock_hitem *item, const char *val);
// handler item
typedef struct sl_sock_hitem{
    sl_sock_msghandler handler; // function-handler
    const char *key;            // key name
    const char *help;           // key help
    void *data;                 // user data (e.g. struct key-varptr-limits)
} sl_sock_hitem_t;

// socket type
typedef enum{
    SOCKT_UNIX,     // UNIX socket
    SOCKT_NETLOCAL, // INET socket but only for localhost
    SOCKT_NET,      // true INET socket
    SOCKT_AMOUNT    // amount of types
} sl_socktype_e;

struct sl_sock;

// default max clients amount
#define SL_DEF_MAXCLIENTS   (32)
// custom socket handlers: connect/disconnect/etc
// max clients handler
void sl_sock_maxclhandler(struct sl_sock *s, void (*h)(int));
// client connected handler
void sl_sock_connhandler(struct sl_sock *s, int(*h)(struct sl_sock*));
// client disconnected handler
void sl_sock_dischandler(struct sl_sock *s, void(*h)(struct sl_sock*));
// unknown message handler (instead of "BADKEY" default message)
void sl_sock_defmsghandler(struct sl_sock *s, sl_sock_hresult_e(*h)(struct sl_sock *s, const char *str));

typedef enum{
    SOCKM_RAW = 0,  // default sockets
    SOCKM_GET,      // http methods - client should be closed after data processing
    SOCKM_PUT,
    SOCKM_POST,
    SOCKM_PATCH,
    SOCKM_DELETE,
    SOCKM_AMOUNT
} sl_sockmethod_e;

// socket itself
typedef struct sl_sock{
    int fd;                     // file descriptor
    int connected;              // == TRUE if connected
    sl_socktype_e type;         // type
    sl_ringbuffer_t *buffer;    // input data buffer
    char *node;                 // original UNIX-socket path or node name for INET (NULL - localhost client or any server)
    char *service;              // NULL for UNIX-socket and port for INET
    struct addrinfo *addrinfo;  // filled addrinfo structure
    void *data;                 // user data
    pthread_mutex_t mutex;      // read/write mutex
    pthread_t rthread;          // reading ring buffer thread for client and main server thread for server
    char IP[INET_ADDRSTRLEN];   // client's IP address
    sl_sock_hitem_t *handlers;  // if non-NULL, run handler's thread when opened
    sl_sockmethod_e sockmethod; // method
    uint64_t lineno;            // number of line read
    int contlen;                // content length for POST method
    int gotemptyline;           // == TRUE when found empty line in web request (to know that header is over)
    char outbuffer[BUFSIZ];     // buffer for output data (if client is WEB)
    size_t outplen;             // amount of bytes in `outbuffer`
    // server-only items
    int maxclients;             // max clients amount
    void (*toomuch_handler)(int); // too much clients handler; it is running for client connected with number>maxclients (before closing its fd)
    int (*newconnect_handler)(struct sl_sock*); // new client connected handler; it will be run each new connection
    void (*disconnect_handler)(struct sl_sock*); // client disconnected handler
    sl_sock_hresult_e(*defmsg_handler)(struct sl_sock *s, const char *str); // default message handler (the only without `handlers` array or instead of "BADKEY" answer
    struct sl_sock **clients;   // pointer to clients array for `sendall`
} sl_sock_t;

const char *sl_sock_hresult2str(sl_sock_hresult_e r);
void sl_sock_delete(sl_sock_t **sock);
sl_sock_t *sl_sock_run_client(sl_socktype_e type, const char *path, int bufsiz);
sl_sock_t *sl_sock_run_server(sl_socktype_e type, const char *path, int bufsiz, sl_sock_hitem_t *handlers);
void sl_sock_changemaxclients(sl_sock_t *sock, int val);
int sl_sock_getmaxclients(sl_sock_t *sock);

ssize_t sl_sock_sendbinmessage(sl_sock_t *socket, const uint8_t *msg, size_t l);
ssize_t sl_sock_sendbyte(sl_sock_t *socket, uint8_t byte);
ssize_t sl_sock_sendstrmessage(sl_sock_t *socket, const char *msg);
ssize_t sl_sock_readline(sl_sock_t *sock, char *str, size_t len);
int sl_sock_sendall(sl_sock_t *sock, uint8_t *data, size_t len);

sl_sock_hresult_e sl_sock_inthandler(sl_sock_t *client, sl_sock_hitem_t *hitem, const char *str);
sl_sock_hresult_e sl_sock_dblhandler(sl_sock_t *client, sl_sock_hitem_t *hitem, const char *str);
sl_sock_hresult_e sl_sock_strhandler(sl_sock_t *client, sl_sock_hitem_t *hitem, const char *str);


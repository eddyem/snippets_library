# `libusefull_macros`  A collection of useful C snippets for Linux

**Version:** 0.3.5  
**Author:** Edward V. Emelianov (<edward.emelianoff@gmail.com>)  
**License:** GPLv3+  
**Repository:** [github.com/eddyem/snippets_library](https://github.com/eddyem/snippets_library)

---

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
    - [Building from source](#building-from-source)
    - [CMake options](#cmake-options)
    - [Linking](#linking)
3. [Quick Start](#quick-start)
4. [Module Reference](#module-reference)
    - [Initialization & Locale](#initialization--locale)
    - [Colored Terminal Output](#colored-terminal-output)
    - [Error & Warning Macros](#error--warning-macros)
    - [Memory Management](#memory-management)
    - [String Utilities](#string-utilities)
    - [Number Conversion](#number-conversion)
    - [Console / Terminal I/O](#console--terminal-io)
    - [Logging](#logging)
    - [Command-line Argument Parsing](#command-line-argument-parsing)
    - [Configuration Files](#configuration-files)
    - [Daemon Support](#daemon-support)
    - [FIFO / LIFO Linked List](#fifo--lifo-linked-list)
    - [Ring Buffer](#ring-buffer)
    - [TCP / UNIX Socket Server & Client](#tcp--unix-socket-server--client)
    - [Serial Port (TTY)](#serial-port-tty)
    - [Sub-options Parsing](#sub-options-parsing)
    - [Miscellaneous Utilities](#miscellaneous-utilities)
5. [Data Structures](#data-structures)
6. [Examples](#examples)
7. [Internationalization (i18n)](#internationalization-i18n)
8. [Thread Safety](#thread-safety)

---

## Overview

`libusefull_macros` is a C shared library that bundles many frequently needed utility routines for
Linux application development. It covers:

- Colored, locale-aware terminal output
- Safe memory allocation and memory-mapped file I/O
- GNU `getopt_long`-style command-line argument parsing with typesafe callbacks
- INIlike configuration file reading/writing
- Daemonization with PIDfile management
- Threadsafe ring buffer for producerconsumer patterns
- FIFO/LIFO linked list
- TCP and UNIX socket server/client framework with builtin HTTP parsing and keyvalue handler dispatch
- Serial port (TTY) management with nonstandard baud rates
- Filebased logging with multiple severity levels
- `gettext` integration for internationalization

All public identifiers are prefixed with `sl_` (for "snippets library") to avoid naming collisions.

---

## Installation

### Building from source

```bash
git clone https://github.com/eddyem/snippets_library.git
cd snippets_library
mkdir build && cd build
cmake ..
make -j$(nproc)
make install
```

The build produces a shared library `libusefull_macros.so` and a pkg-config file.

### CMake options

| Variable | Default | Description |
|----------|---------|-------------|
| `DEBUG=1` | off | Build with `-Wextra -Wall -Werror -W` and enable debug output |
| `EXAMPLES=1` | off | Build example programs in the `examples/` subdirectory |
| `NOGETTEXT` | not set | Disable gettext integration |
| `PROCESSOR_COUNT` | auto | Number of threads for parallel operations (default detects from `/proc/cpuinfo`) |

Example:

```bash
cmake .. -DDEBUG=1 -DEXAMPLES=1
```

### Linking

A pkg-config file is installed:

```bash
pkg-config --cflags --libs usefull_macros
```

Or manually:

```bash
gcc -o myapp myapp.c -I/usr/local/include -L/usr/local/lib -lusefull_macros -lm -lpthread
```

---

## Quick Start

```c
#include <usefull_macros.h>

int main(int argc, char **argv) {
    sl_init();                          // locale, gettext, colored output
    green("Hello, world!\n");           // green text on tty
    red("An error occurred\n");         // red text on tty
    return 0;
}
```

Compile:

```bash
gcc -o hello hello.c $(pkg-config --cflags --libs usefull_macros)
```

---

## Module Reference

### Initialization & Locale

```c
void sl_init(void);
```

Must be called once at the beginning of `main()`. It:

- Detects whether `stdout`/`stderr` are terminals and sets up colored output functions accordingly.
- Calls `setlocale(LC_ALL, "")` and `setlocale(LC_NUMERIC, "C")` (decimal point is always a dot).
- If compiled with `GETTEXT` defined, binds the message domain.

**Important:** `sl_init()` must be called before any other library function that generates output.

---

### Colored Terminal Output

When output is a terminal (not redirected to a file or pipe), text can be printed in color:

```c
extern int (*red)(const char *fmt, ...);
extern int (*green)(const char *fmt, ...);
```

These function pointers are set by `sl_init()`. Use them like `printf`:

```c
red("Error code: %d\n", err);
green("Operation successful\n");
```

When output is not a tty, `red` wraps the message between lines of asterisks, and `green` falls
back to plain `printf`.

---

### Error & Warning Macros

```c
#define ERR(...)   // print errno + message, then exit(9)
#define ERRX(...)  // print message (no errno), then exit(9)
#define WARN(...)  // print errno + message, continue
#define WARNX(...) // print message (no errno), continue
```

These use the `_WARN` function pointer (respects colored output). They automatically include the
current `errno` value when using `ERR`/`WARN`.

The default signal handler for `ERR`/`ERRX` is `signals(9)`, which simply calls `exit(9)`. You can
override the `signals` function since it is declared `__attribute__((weak))`:

```c
void signals(int sig) {
    // custom cleanup
    exit(sig);
}
```

**Debug macros** (active only when `-DEBUG` is defined):

```c
FNAME()  // print current function name, file, line
DBG(...) // printf-like debug message
```

---

### Memory Management

```c
void *sl_alloc(size_t N, size_t S);
```

Safe `calloc` wrapper. Exits with error message if allocation fails.

Convenience macros:

```c
ALLOC(type, var, size)  // declare + allocate: type *var = calloc(size, sizeof(type))
MALLOC(type, size)      // allocate without declaration
FREE(ptr)               // free and set to NULL
```

**Memorymapped files:**

```c
typedef struct { char *data; size_t len; } sl_mmapbuf_t;
sl_mmapbuf_t *sl_mmap(char *filename);
void sl_munmap(sl_mmapbuf_t *b);
```

Maps a file readonly into memory; `sl_munmap` unmaps and frees the structure.

**System memory query:**

```c
uint64_t sl_mem_avail(void);  // available physical memory in bytes
```

---

### String Utilities

```c
char *sl_omitspaces(const char *str);   // skip leading whitespace
char *sl_omitspacesr(const char *str);  // pointer to (last non-space char + 1)
int sl_remove_quotes(char *string);     // remove outer matching quotes (' or ")
int sl_get_keyval(const char *pair, char key[32], char value[128]); // parse "key = value"
```

`sl_remove_quotes` strips matched pairs of single or double quotes from both ends. Returns the
number of pairs removed (0 if none).

`sl_get_keyval` parses a line into key and value:
- Returns `0` if the line is empty or a comment (starts with `#`).
- Returns `1` if only a key is present.
- Returns `2` if both key and value are found.
- Ignores inline comments, strips surrounding whitespace and quotes.

---

### Number Conversion

```c
int sl_str2i(int *num, const char *str);
int sl_str2ll(long long *num, const char *str);
int sl_str2d(double *num, const char *str);
```

Safe `strtol`/`strtod` wrappers. Return `TRUE` (1) on success, `FALSE` (0) on failure. The output
pointer may be `NULL` to only check validity.

---

### Console / Terminal I/O

For noncanonical, noecho terminal input:

```c
void sl_setup_con(void);     // switch terminal to raw mode
void sl_restore_con(void);   // restore original terminal settings
int sl_read_con(void);       // nonblocking read (0 if no key)
int sl_getchar(void);        // blocking read of one character
```

Typical usage:

```c
sl_setup_con();
int ch = sl_getchar();
sl_restore_con();
```

**Important:** These functions are **not threadsafe**  they use a global `struct termios2`.

---

### Logging

```c
typedef enum {
    LOGLEVEL_NONE,   // no logging
    LOGLEVEL_ERR,    // only errors
    LOGLEVEL_WARN,   // warnings + errors
    LOGLEVEL_MSG,    // all except debug
    LOGLEVEL_DBG,    // all messages
    LOGLEVEL_ANY     // everything
} sl_loglevel_e;

sl_log_t *sl_createlog(const char *logpath, sl_loglevel_e level, int prefix);
void sl_deletelog(sl_log_t **log);
int sl_putlogt(int timest, sl_log_t *log, sl_loglevel_e lvl, const char *fmt, ...);
```

A "global" log is managed through the pointer `sl_globlog`:

```c
extern sl_log_t *sl_globlog;
```

Convenience macros (write to `sl_globlog`):

| Macro | Meaning |
|-------|---------|
| `OPENLOG(path, level, prefix)` | Open global log |
| `LOGERR(...)` | Error with timestamp |
| `LOGERRADD(...)` | Error without timestamp |
| `LOGWARN(...)` / `LOGWARNADD(...)` | Warning |
| `LOGMSG(...)` / `LOGMSGADD(...)` | Message |
| `LOGDBG(...)` / `LOGDBGADD(...)` | Debug |

Timestamps use format `YYYY/MM/DD-HH:MM:SS`. Each log call locks the file with `flock` for
concurrent access.

---

### Command-line Argument Parsing

Built on top of `getopt_long`. Supports:

- Short and long options
- Required, optional, and no arguments
- Multiple occurrences of the same option (multiparameters)
- Six data types: `int`, `long long`, `double`, `float`, `char*`, and function callback
- Automatic help generation

**Option descriptor:**

```c
typedef struct {
    const char *name;       // long option (NULL for short-only)
    sl_hasarg_e has_arg;    // NO_ARGS, NEED_ARG, OPT_ARG, or MULT_PAR
    int *flag;              // NULL  return val; else set *flag = val
    int val;                // short option character or flag value
    sl_argtype_e type;      // arg_int, arg_longlong, arg_double, arg_float,
                            // arg_string, arg_function
    void *argptr;           // pointer to variable or callback function
    const char *help;       // help text (mandatory; end_option marks end)
} sl_option_t;
```

**Helper macro:**

```c
#define APTR(x) ((void*)x)
```

**Functions:**

```c
void sl_parseargs(int *argc, char ***argv, sl_option_t *options);
void sl_parseargs_hf(int *argc, char ***argv, sl_option_t *options,
                     void (*helpfun)(int, sl_option_t*));
void sl_showhelp(int oindex, sl_option_t *options);
void sl_helpstring(char *s);  // customize help header
```

After calling `sl_parseargs`, `argc` and `argv` are updated to point to remaining nonoption
arguments.

**Example:**

```c
int verbose = 0;
char *output = NULL;
sl_option_t opts[] = {
    {"verbose", NO_ARGS,  NULL, 'v', arg_none,   APTR(&verbose), "increase verbosity"},
    {"output",  NEED_ARG, NULL, 'o', arg_string, APTR(&output),  "output file"},
    end_option
};

int main(int argc, char **argv) {
    sl_init();
    sl_parseargs(&argc, &argv, opts);
    // argc, argv now contain nonoption arguments
}
```

**Multiparameters** (`MULT_PAR`): Options that may appear multiple times. The library allocates a
`NULL`terminated array of pointers, each pointing to a newly allocated value. For `arg_string`,
each element is a pointer to a `strdup`'d string; for numeric types, each is a pointer to a
heapallocated number.

**Function callback** (`arg_function`): The callback must have signature `int (*fn)(void *arg)` and
receives a `strdup`'d argument string.

**Custom help function:** Pass a function pointer to `sl_parseargs_hf` to handle errors differently
than the default `sl_showhelp` (which calls `exit(-1)`).

---

### Configuration Files

Reads keyvalue pairs from a file and treats them as commandline options.

```c
int sl_conf_readopts(const char *filename, sl_option_t *options);
char *sl_print_opts(sl_option_t *opt, int showall);
void sl_conf_showhelp(int idx, sl_option_t *options);
```

`sl_conf_readopts` reads a file with lines like:

```
# comment
key1 = value1
key2
key3 = "quoted value"
```

Each noncomment line is converted to `--key=value` (or `--key` if no value) and passed to
`sl_parseargs`. Returns the number of recognized options.

`sl_print_opts` generates a string representation of current option values (useful for debugging or
saving state). The returned string must be freed with `free()`.

---

### Daemon Support

```c
int sl_daemonize(void);
void sl_check4running(char *selfname, char *pidfilename);
char *sl_getPSname(pid_t pid);
void sl_iffound_deflt(pid_t pid);  // WEAK  overridable
```

`sl_daemonize()`:
- `chdir("/")`
- `umask(0)`
- Closes stdin/stdout/stderr, reopens to `/dev/null`
- Ignores `SIGHUP`
- Returns 0 on success, -1 on failure

`sl_check4running()`:
- Checks a PID file and `/proc` for a running process with the same name.
- If found, calls `sl_iffound_deflt` (by default prints a message and exits).
- Otherwise writes its own PID to the PID file.

Override `sl_iffound_deflt` in your application (it is `__attribute__((weak))`):

```c
void sl_iffound_deflt(pid_t pid) {
    fprintf(stderr, "Already running (pid %d)\n", pid);
    exit(1);
}
```

---

### FIFO / LIFO Linked List

A simple singlylinked list with both head and tail pointers.

```c
typedef struct sl_buff_node {
    void *data;
    struct sl_buff_node *next, *last;
} sl_list_t;

sl_list_t *sl_list_push(sl_list_t **lst, void *v);       // LIFO (push to head)
sl_list_t *sl_list_push_tail(sl_list_t **lst, void *v);  // FIFO (push to tail)
void *sl_list_pop(sl_list_t **lst);                      // pop from head
```

`sl_list_pop` returns the data pointer and frees the node. The caller is responsible for freeing
the data if needed.

---

### Ring Buffer

A threadsafe, fixedsize ring buffer for byte streams, protected by `pthread_mutex_t`.

```c
typedef struct {
    uint8_t *data;
    size_t length, head, tail;
    pthread_mutex_t busy;
} sl_ringbuffer_t;

sl_ringbuffer_t *sl_RB_new(size_t size);
void sl_RB_delete(sl_ringbuffer_t **b);
size_t sl_RB_read(sl_ringbuffer_t *b, uint8_t *s, size_t len);
ssize_t sl_RB_readto(sl_ringbuffer_t *b, uint8_t byte, uint8_t *s, size_t len);
ssize_t sl_RB_readline(sl_ringbuffer_t *b, char *s, size_t len);
int sl_RB_putbyte(sl_ringbuffer_t *b, uint8_t byte);
size_t sl_RB_write(sl_ringbuffer_t *b, const uint8_t *str, size_t len);
size_t sl_RB_writestr(sl_ringbuffer_t *b, char *s);
size_t sl_RB_datalen(sl_ringbuffer_t *b);
size_t sl_RB_freesize(sl_ringbuffer_t *b);
void sl_RB_clearbuf(sl_ringbuffer_t *b);
ssize_t sl_RB_hasbyte(sl_ringbuffer_t *b, uint8_t byte);
```

Key behaviors:

- `sl_RB_readline` reads up to and including a newline (`\n`), replaces `\n` with `\0`.
- `sl_RB_readto` reads until (and including) a specified byte.
- `sl_RB_writestr` ensures the string ends with `\n` before writing.
- All read/write operations are atomic with respect to the mutex.

---

### TCP / UNIX Socket Server & Client

A highlevel socket framework supporting TCP and UNIX domain sockets, with builtin HTTP method
detection.

**Socket types:**

```c
typedef enum { SOCKT_UNIX, SOCKT_NETLOCAL, SOCKT_NET } sl_socktype_e;
```

**Creating and destroying:**

```c
sl_sock_t *sl_sock_run_server(sl_socktype_e type, const char *path,
                              int bufsiz, sl_sock_hitem_t *handlers);
sl_sock_t *sl_sock_run_client(sl_socktype_e type, const char *path, int bufsiz);
void sl_sock_delete(sl_sock_t **sock);
```

- `path` for UNIX sockets: file path; prefix with `\0` or `@` for abstract namespace.
- `path` for INET sockets: `"host:port"` (client) or `":port"` (server).
- `handlers`: `NULL`terminated array of keyvalue handlers (see below).
- `bufsiz`: internal ring buffer size (minimum 256).

**Sending data:**

```c
ssize_t sl_sock_sendbinmessage(sl_sock_t *socket, const uint8_t *msg, size_t l);
ssize_t sl_sock_sendstrmessage(sl_sock_t *socket, const char *msg);
ssize_t sl_sock_sendbyte(sl_sock_t *socket, uint8_t byte);
int sl_sock_sendall(sl_sock_t *sock, uint8_t *data, size_t len); // server only
```

**Reading (client):**

```c
ssize_t sl_sock_readline(sl_sock_t *sock, char *str, size_t len);
```

**Handler dispatch (server):**

```c
typedef sl_sock_hresult_e (*sl_sock_msghandler)(struct sl_sock *s,
                            struct sl_sock_hitem *item, const char *val);

typedef struct sl_sock_hitem {
    sl_sock_msghandler handler;
    const char *key;
    const char *help;
    void *data;              // user data (e.g., &variable)
} sl_sock_hitem_t;
```

Handler results:

```c
typedef enum {
    RESULT_OK, RESULT_FAIL, RESULT_BADVAL,
    RESULT_BADKEY, RESULT_SILENCE
} sl_sock_hresult_e;
```

Builtin handlers for common types:

```c
sl_sock_hresult_e sl_sock_inthandler(...);  // int64_t
sl_sock_hresult_e sl_sock_dblhandler(...);  // double
sl_sock_hresult_e sl_sock_strhandler(...);  // string
```

**Optional key numbering** (`key[0]`, `key(1)`, `key{2}`, `key3`):

```c
typedef struct { double magick; int n; } sl_sock_keyno_t;
#define SL_SOCK_KEYNO_DEFAULT { .magick = -INFINITY, .n = -1 }
void sl_sock_keyno_init(sl_sock_keyno_t *k);
int sl_sock_keyno_check(sl_sock_keyno_t *k);
```

**Server hooks:**

```c
void sl_sock_changemaxclients(sl_sock_t *sock, int val);
void sl_sock_maxclhandler(sl_sock_t *sock, void (*h)(int));
void sl_sock_connhandler(sl_sock_t *sock, int (*h)(struct sl_sock*));
void sl_sock_dischandler(sl_sock_t *sock, void (*h)(struct sl_sock*));
void sl_sock_defmsghandler(sl_sock_t *sock, sl_sock_hresult_e (*h)(struct sl_sock*, const char*));
```

The server thread automatically handles `POLLIN` events, parses messages using `sl_get_keyval`, and
dispatches them to matching handlers. HTTP `GET`/`POST` requests are partially parsed: `GET`
parameters are URLdecoded and dispatched; `POST` data is accumulated and then parsed.

---

### Serial Port (TTY)

```c
typedef struct {
    char *portname;
    int speed;
    char *format;     // e.g., "8N1"
    int comfd;
    char *buf;
    size_t bufsz, buflen;
    int exclusive;
} sl_tty_t;

int sl_tty_fdescr(const char *comdev, const char *format, int speed, int exclusive);
sl_tty_t *sl_tty_new(char *comdev, int speed, size_t bufsz);
int sl_tty_setformat(sl_tty_t *d, const char *format);
sl_tty_t *sl_tty_open(sl_tty_t *d, int exclusive);
int sl_tty_read(sl_tty_t *d);
int sl_tty_write(int comfd, const char *buff, size_t length);
void sl_tty_close(sl_tty_t **descr);
int sl_tty_tmout(double usec);
```

Format string: three characters  data bits (58), parity (N/E/O/0/1), stop bits (1/2). Example:
`"8N1"`.

Uses `struct termios2` via `ioctl(TCGETS2/TCSETS2)` to support arbitrary baud rates (not limited to
the standard `Bxxx` constants).

`sl_tty_read` uses `select()` with a configurable timeout (default 5 ms, change with
`sl_tty_tmout`). Returns the number of bytes read; data is placed in `d->buf` with length
`d->buflen`.

`sl_tty_fdescr` allows to use library functions for opening serial device with given path, format string,
non-standard speed, marking it as exclusive (not share with other processes) or not. It doesn't allocates
memory and just returns opened tty file descriptor or `-1` in case of error.


---

### Sub-options Parsing

Parses strings like `key1=val1:key2=val2,key3`:

```c
typedef struct {
    const char *name;
    sl_hasarg_e has_arg;
    sl_argtype_e type;
    void *argptr;
} sl_suboption_t;

int sl_get_suboption(char *str, sl_suboption_t *opt);
```

The input string is tokenized on `:` and `,`; each token is matched against option names
(caseinsensitive).

---

### Miscellaneous Utilities

```c
const char *sl_libversion(void);    // returns PACKAGE_VERSION string
double sl_dtime(void);              // UNIX time as double (seconds)
long sl_random_seed(void);          // seed from /dev/random or time
int sl_canread(int fd);             // nonblocking select() for read
int sl_canwrite(int fd);            // nonblocking select() for write
```

---

## Data Structures

| Structure | Purpose |
|-----------|---------|
| `sl_option_t` | Commandline option descriptor |
| `sl_suboption_t` | Suboption descriptor |
| `sl_tty_t` | Serial port state |
| `sl_log_t` | Log file descriptor |
| `sl_mmapbuf_t` | Memorymapped file |
| `sl_list_t` | Linked list node |
| `sl_ringbuffer_t` | Threadsafe ring buffer |
| `sl_sock_t` | Socket state (client or server) |
| `sl_sock_hitem_t` | Socket handler item |
| `sl_sock_int_t` | Timestamped `int64_t` |
| `sl_sock_double_t` | Timestamped `double` |
| `sl_sock_string_t` | Timestamped string |
| `sl_sock_keyno_t` | Optional key number |

---

## Examples

The repository includes several example programs in the `examples/` directory:

| Example | Demonstrates |
|---------|-------------|
| `helloworld` | Minimal usage: `sl_init`, colored output, `sl_setup_con`/`sl_getchar`/`sl_restore_con` |
| `options` + `cmdlnopts` | Full commandline parsing with all types, logging, serial port, signals |
| `conffile` | Configuration file reading, `sl_print_opts`, multiparameters |
| `fifo` | LIFO and FIFO list operations |
| `ringbuffer` | Ring buffer creation, line reading, overflow handling |
| `clientserver` | Socket server/client with custom handlers, bit flags, logging |
| `daemon` | Daemonization, PID file, child process monitoring |

Build examples with:

```bash
cmake .. -DEXAMPLES=1
make
```

---

## Internationalization (i18n)

If compiled with `GETTEXT` defined, the `_()` macro wraps `gettext()`. Translation files are
expected in the `locale/` directory. The library generates `.po` and `.mo` files during the build
(in Debug mode). To disable, define `NOGETTEXT`.

```c
#define _(String)  gettext(String)    // when GETTEXT is defined
#define _(String)  (String)           // otherwise
```

---

## Thread Safety

- **Ring buffer:** all operations are protected by a `pthread_mutex_t`.
- **Logging:** file writes are guarded with `flock(LOCK_EX)`.
- **Sockets:** server thread uses `poll()`; client read thread is separate; send operations lock the socket mutex.
- **Console I/O:** `sl_setup_con`/`sl_read_con`/`sl_getchar`/`sl_restore_con` are **not** threadsafe (global terminal state).

---


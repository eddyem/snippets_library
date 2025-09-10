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

#include <ctype.h>
#include <float.h> // FLT_max/min
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "usefull_macros.h"

/**
 * @brief sl_remove_quotes - remove all outern quotes - starting and trailng " and '
 * @param (io) string to modify (if quotes found rest of string will be moved to head, tail will be zeroed)
 * @return amount of quotation pair found
 */
int sl_remove_quotes(char *string){
    if(!string) return 0;
    int l = strlen(string);
    if(l < 2) return 0;
    int nq = 0, half = l/2;
    for(; nq < half; ++nq){
        char _1st = string[nq];
        if(_1st != string[l-1-nq]) break;
        if(_1st != '\'' && _1st != '"') break;
    }
    if(nq == 0) return 0;
    l -= 2 * nq;
    memmove(string, string + nq, l);
    string[l] = 0;
    return nq;
}

/**
 * @brief sl_get_keyval - get key name and its value from string pair
 * @param pair - empty string, `key = value` or just `key`
 * @param key - substring with `key`
 * @param value - its value or empty line
 * @return 0 if no key found (or line is a comment with '#' first); 1 if only key found and 2 if found both key and value
 * this function removes all leading and trailing spaces in `key` and `value`;
 * also if `key` have several words only first saved, `value` can be long string with or without quotations
 */
int sl_get_keyval(const char *pair, char key[SL_KEY_LEN], char value[SL_VAL_LEN]){
    //DBG("got pair: '%s'", pair);
    if(!pair || !*pair) return 0; // empty line
    char *kstart = sl_omitspaces(pair);
    if(!*kstart || *kstart == SL_COMMENT_CHAR) return 0; // only spaces
    int ret = 1;
    char *eq = strchr(kstart, '='), *kend = kstart + 1;
    if(eq == kstart) return 0; // only = (and maybe val)
    char *cmnt = strchr(kstart, SL_COMMENT_CHAR);
    if(eq && cmnt && cmnt < eq) eq = NULL; // comment starting before equal sign
    if(eq){ do{
        //DBG("got equal symbol: '%s'", eq);
        char *vstart = sl_omitspaces(eq + 1);
        if(!*vstart) break; // empty line or only spaces after `=`
        char *vend = sl_omitspacesr(vstart);
        size_t l = SL_VAL_LEN - 1; // truncate value to given length
        if(l > (size_t)(vend - vstart)) l = vend - vstart;
        //DBG("l=%zd", l);
        strncpy(value, vstart, l);
        value[l] = 0;
        cmnt = strchr(value, SL_COMMENT_CHAR);
        if(cmnt){ // remove trailing spaces before comment
            *cmnt = 0;
            *sl_omitspacesr(value) = 0;
        }
        if(!*value) break;
        //DBG("Got value: '%s'", value);
        ret = 2;
    }while(0);
    }else eq = kstart + strlen(kstart);
    for(; kend < eq && !isspace(*kend); ++kend);
    size_t l = SL_KEY_LEN - 1;
    if(l > (size_t)(kend - kstart)) l = kend - kstart;
    else if(l < (size_t)(kend - kstart)) WARNX(_("sl_get_keyval(): key would be trunkated to %d symbols"), l);
    //DBG("kend=%c, kstart=%c, l=%zd", *kend, *kstart, l);
    strncpy(key, kstart, l);
    key[l] = 0;
    cmnt = strchr(key, SL_COMMENT_CHAR);
    if(cmnt) *cmnt = 0;
    //DBG("Got key: '%s'", key);
    return ret;
}

// Read key/value from file; return -1 when file is over
static int read_key(FILE *file, char key[SL_KEY_LEN], char value[SL_VAL_LEN]){
    char *line = NULL;
    size_t n = 0;
    ssize_t got = getline(&line, &n, file);
    if(!line) return 0;
    if(got < 0){ // EOF
        free(line);
        return -1;
    }
    int r = sl_get_keyval(line, key, value);
    free(line);
    return r;
}

// print option value
static size_t pr_val(sl_argtype_e type, void *argptr, char **buffer, size_t *buflen, size_t pos){
    size_t maxlen = *buflen - pos - 1;
    char *buf = *buffer + pos;
    switch(type){
        case arg_none:
        case arg_int:
            DBG("int %d", *(int*) argptr);
            return snprintf(buf, maxlen, "%d", *(int*) argptr);
        case arg_longlong:
            DBG("long long %lld", *(long long*) argptr);
            return snprintf(buf, maxlen, "%lld", *(long long*)argptr);
        case arg_float:
            DBG("float %g", *(float*) argptr);
            return snprintf(buf, maxlen, "%g", *(float*) argptr);
        case arg_double:
            DBG("double %g", *(double*) argptr);
            return snprintf(buf, maxlen, "%g", *(double*) argptr);
        case arg_string:
            if(!argptr || !(*(char**)argptr)){
                return snprintf(buf, maxlen, "(null)");
            }else if(!(**(char**)argptr)){
                return snprintf(buf, maxlen, "(empty)");
            }
            char *str = *(char**) argptr;
            DBG("string %s", str);
            size_t z = strlen(str);
            while(pos + z > *buflen + 5){
                *buflen += BUFSIZ;
                *buffer = realloc(*buffer, *buflen);
                if(!*buffer) ERRX("realloc()");
                maxlen += BUFSIZ;
                buf = *buffer + pos;
            }
            return snprintf(buf, maxlen, "\"%s\"", str);
        default:
            DBG("function");
            return snprintf(buf, maxlen, "\"(unsupported)\"");
    }
    return 0;
}

// print one option
static size_t print_opt(sl_option_t *opt, char **buffer, size_t *buflen, size_t pos){
    if((ssize_t)*buflen - pos < 3 *(SL_KEY_LEN + SL_VAL_LEN)){
        *buflen += BUFSIZ;
        *buffer = realloc(*buffer, *buflen);
        if(!*buffer) ERR("realloc()");
    }
    char *buf = *buffer + pos;
    size_t l = 0, maxlen = *buflen - pos - 1;
    size_t got = snprintf(buf, maxlen, "%s = ", opt->name);
    l = got; maxlen -= got; buf += got;
    if(opt->flag){
        DBG("got flag '%d'", *opt->flag);
        l += snprintf(buf, maxlen, "%d\n", *opt->flag);
        return l;
    }
    if(!opt->argptr){ // ERR!
        l += snprintf(buf, maxlen, "\"(no argptr)\"\n");
        WARNX("Parameter \"%s\" have no argptr!", opt->name);
        return l;
    }
    DBG("type: %d", opt->type);
    got = pr_val(opt->type, opt->argptr, buffer, buflen, pos + l);
    l += got; maxlen -= got; buf += got;
    l += snprintf(buf, maxlen, "\n");
    return l;
}

/**
 * @brief sl_print_opts - fills string buffer with information from opt
 * @param opt - options buffer
 * @param showall - TRUE to show even uninitialized options with NO_ARGS
 * @return allocated string (should be free'd)
 */
char *sl_print_opts(sl_option_t *opt, int showall){
    char *buf = MALLOC(char, BUFSIZ);
    size_t L = BUFSIZ, l = 0;
    for(; opt->help; ++opt){
        if(!opt->name) continue; // only show help - not config option!
        DBG("check %s", opt->name);
        if(!showall && opt->has_arg == NO_ARGS) continue; // show NO_ARGS only when `showall==TRUE`
        if(!showall && opt->type == arg_string && !opt->argptr) continue; // empty string
        if(opt->has_arg == MULT_PAR){
            sl_option_t tmpopt = *opt;
            DBG("type: %d", tmpopt.type);
            if(!opt->argptr){ DBG("No pointer to array!"); continue; }
#if 0
            void ***pp = (void***)opt->argptr;
            if(!*(char***)pp){ DBG("Array is empty"); continue; }
            while(**pp){
                if(opt->type == arg_string){
                    DBG("str");
                    tmpopt.argptr = *pp; // string is pointer to pointer!
                }else tmpopt.argptr = **pp;
                if(!tmpopt.argptr){ DBG("null"); break; }
                l += print_opt(&tmpopt, &buf, &L, l);
                ++(*pp);
            }
#endif
            void **pp = *(void***)opt->argptr;
            if(!(char**)pp){ DBG("Array is empty"); continue; }
            while(*pp){
                if(opt->type == arg_string){
                    DBG("str");
                    tmpopt.argptr = pp; // string is pointer to pointer!
                }else tmpopt.argptr = *pp;
                if(!tmpopt.argptr){ DBG("null"); break; }
                l += print_opt(&tmpopt, &buf, &L, l);
                ++(pp);
            }
        }else l += print_opt(opt, &buf, &L, l);
    }
    return buf;
}


/**
 * @brief sl_conf_readopts - simplest configuration:
 *  read file with lines "par" or "par = val" and set variables like they are options
 * @param filename - configuration file name
 * @param options - array with options (could be the same like for sl_parseargs)
 * @return amount of data read
 */
int sl_conf_readopts(const char *filename, sl_option_t *options){
    if(!filename || !options) return 0;
    FILE *f = fopen(filename, "r");
    if(!f){
        WARN(_("Can't open %s"), filename);
        return 0;
    }
    int argc = 1;
#define BUFSZ   (SL_KEY_LEN+SL_VAL_LEN+8)
    char key[SL_KEY_LEN], val[SL_VAL_LEN], obuf[BUFSZ];
    int argvsize = 0;
    char **argv = NULL;
    do{
        int r = read_key(f, key, val);
        if(r < 0) break;
        if(r == 0) continue;
        DBG("key='%s', val='%s'", key, (r == 2) ? val : "(absent)");
        ++argc;
        if(argvsize <= argc){
            argvsize += 256;
            argv = realloc(argv, sizeof(char*) * argvsize);
            if(!argv) ERRX("sl_conf_readopts: realloc() error");
        }
        if(argc == 2) argv[0] = strdup(__progname); // all as should be
        if(r == 2){
            // remove trailing/ending quotes
            sl_remove_quotes(val);
            snprintf(obuf, BUFSZ-1, "--%s=%s", key, val);
        }else snprintf(obuf, BUFSZ-1, "--%s", key);
        DBG("next argv: '%s'", obuf);
        argv[argc-1] = strdup(obuf);
    }while(1);
    if(!argc) return 0;
    int N = argc; char **a = argv;
    sl_parseargs_hf(&argc, &a, options, sl_conf_showhelp);
    for(int n = 0; n < N; ++n) free(argv[n]);
    free(argv);
    return N - argc; // amount of recognized options
}

// sort only by long options
static int confsort(const void *a1, const void *a2){
    const sl_option_t *o1 = (sl_option_t*)a1, *o2 = (sl_option_t*)a2;
    const char *l1 = o1->name, *l2 = o2->name;
    // move empty options to end of list
    if(!l1 && !l2) return 1;
    if(!l1) return 1;
    if(!l2) return -1;
    return strcmp(l1, l2);
}

static void pr_helpstring(sl_option_t *opt){
    if(!opt->name || !opt->help) return;
    printf("  %s", opt->name);
    if(opt->has_arg == NEED_ARG || opt->has_arg == MULT_PAR) // required argument
        printf(" = arg");
    else if(opt->has_arg == OPT_ARG) // optional argument
        printf(" [= arg]");
    printf(" -- %s", opt->help);
    if(opt->has_arg == MULT_PAR) printf(" (can occur multiple times)");
    printf("\n");
}

/**
 * @brief sl_conf_showhelp - show help for config file
 *      (the same as `sl_showhelp`, but without "--", short opts and exit()
 * @param options - config options (only long opts used)
 */
void sl_conf_showhelp(int idx, sl_option_t *options){
    if(!options || !(*options).help) return;
    // count amount of options
    sl_option_t *opts = options;
    int N; for(N = 0; opts->help; ++N, ++opts);
    if(N == 0) exit(-2);
    if(idx > -1){
        if(idx >=N) WARNX(_("sl_conf_showhelp(): wrong index"));
        else pr_helpstring(&options[idx]);
        return;
    }
    sl_option_t *tmpopts = MALLOC(sl_option_t, N);
    memcpy(tmpopts, options, N*sizeof(sl_option_t));
    printf(_("Configuration file options (format: key=value):\n"));
    qsort(tmpopts, N, sizeof(sl_option_t), confsort);
    for(int _ = 0; _ < N; ++_){
        if(!tmpopts[_].name) continue;
        pr_helpstring(&tmpopts[_]);
    }
    free(tmpopts);
}

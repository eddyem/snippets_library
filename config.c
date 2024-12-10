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

// search `opt` record for given `key`
static sl_option_t *opt_search(const char *key, sl_option_t *options){
    while(options->name){
        if(0 == strcmp(key, options->name)) return options;
        ++options;
    }
    return NULL;
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
    for(; opt->name; ++opt){
        DBG("check %s", opt->name);
        if(!showall && opt->has_arg == NO_ARGS) continue; // show NO_ARGS only when `showall==TRUE`
        if(!showall && opt->type == arg_string && !opt->argptr) continue; // empty string
        if((ssize_t)L - l < SL_KEY_LEN + SL_VAL_LEN + 5){
            L += BUFSIZ;
            buf = realloc(buf, L);
            if(!buf) ERR("realloc()");
        }
        l += sprintf(buf + l, "%s=", opt->name);
        if(opt->flag){
            DBG("got flag '%d'", *opt->flag);
            l += sprintf(buf + l, "%d\n", *opt->flag);
            continue;
        }
        if(!opt->argptr){ // ERR!
            l += sprintf(buf + l, "\"(no argptr)\"\n");
            WARNX("Parameter \"%s\" have no argptr!", opt->name);
            continue;
        }
        int z = 0;
        DBG("type: %d", opt->type);
        switch(opt->type){
            case arg_none:
            case arg_int:
                DBG("int %d", *(int*) opt->argptr);
                l += sprintf(buf + l, "%d", *(int*) opt->argptr);
            break;
            case arg_longlong:
                DBG("long long %lld", *(long long*) opt->argptr);
                l += sprintf(buf + l, "%lld", *(long long*) opt->argptr);
            break;
            case arg_float:
                DBG("float %g", *(float*) opt->argptr);
                l += sprintf(buf + l, "%g", *(float*) opt->argptr);
            break;
            case arg_double:
                DBG("double %g", *(double*) opt->argptr);
                l += sprintf(buf + l, "%g", *(double*) opt->argptr);
            break;
            case arg_string:
                if(!opt->argptr || !(*(char*)opt->argptr)){
                    l += sprintf(buf + l, "(null)");
                    break;
                }else if(!(**(char**)opt->argptr)){
                    l += sprintf(buf + l, "(empty)");
                    break;
                }
                DBG("string %s", *(char**) opt->argptr);
                z = strlen(*(char**) opt->argptr);
                while(l + z > L + 3){
                    L += BUFSIZ;
                    buf = realloc(buf, L);
                }
                l += sprintf(buf + l, "%s", *(char**) opt->argptr);
            break;
            default:
                DBG("function");
                l += sprintf(buf + l, "\"(unsupported)\"");
            break;
        }
        l += sprintf(buf + l, "\n");
    }
    return buf;
}

/**
 * @brief sl_set_optval - convert `val` to `oval` parameter according to argument type
 * @param oval (o) - value in int/long long/double/float
 * @param opt (i) - record with options
 * @param val (i) - new value
 * @return FALSE if failed (or wrong data range)
 */
int sl_set_optval(sl_optval *oval, sl_option_t *opt, const char *val){
    if(!oval || !opt || !val) return FALSE;
    long long ll;
    double d;
    switch(opt->type){
        case arg_none:
        case arg_int:
        case arg_longlong:
            do{
                if(!sl_str2ll(&ll, val)){
                    break;
                }
                if(opt->type != arg_longlong){
                    if(ll > INT_MAX || ll < INT_MIN){
                        break;
                    }
                    oval->ival = (int)ll;
                }else oval->llval = ll;
                return TRUE;
            }while(0);
        break;
        case arg_double:
        case arg_float:
            do{
                if(!sl_str2d(&d, val)){
                    break;
                }
                if(opt->type == arg_double){
                    oval->dval = d;
                }else{
                    if(d > FLT_MAX || d < FLT_MIN){
                        break;
                    }
                    oval->fval = (float)d;
                }
                return TRUE;
            }while(0);
        break;
        case arg_string:
            return TRUE;
        break;
        default:
            WARNX(_("Unsupported option type"));
            return FALSE;
    }
    WARNX(_("Wrong number format '%s'"), val);
    return FALSE;
}

// increment opt->argptr or set it to val
static void setoa(sl_option_t *opt, const char *val){
    if(!opt || !opt->argptr || opt->type == arg_function) return;
    sl_optval O;
    if(!sl_set_optval(&O, opt, val)) return;
    switch(opt->type){
        case arg_none: // increment integer
            *(int*) opt->argptr += O.ival;
        break;
        case arg_int:
            *(int*) opt->argptr = O.ival;
        break;
        case arg_longlong:
            *(long long*) opt->argptr = O.llval;
        break;
        case arg_double:
            *(double*) opt->argptr = O.dval;
        break;
        case arg_float:
            *(float*) opt->argptr = O.fval;
        break;
        case arg_string:
            *(char**)opt->argptr = strdup(val);
        break;
        default:
        break;
    }
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
    int N = 0;
    char key[SL_KEY_LEN], val[SL_VAL_LEN];
    do{
        int r = read_key(f, key, val);
        if(r < 0) break;
        if(r == 0) continue;
        sl_option_t *opt = opt_search(key, options);
        if(!opt){
            WARNX(_("Wrong key: '%s'"), key);
            continue;
        }
        if(opt->flag) *opt->flag = opt->val;
        if(r == 1){ // only key
            if(opt->has_arg != NO_ARGS && opt->has_arg != OPT_ARG){
                WARNX(_("Key '%s' need value"), opt->name);
                continue;
            }
            if(opt->argptr) setoa(opt, "1");
        }else{ // key + value
            if(opt->argptr) setoa(opt, val);
            else WARNX(_("Key '%s' have no argptr!"), opt->name);
        }
        ++N;
    }while(1);
    return N;
}

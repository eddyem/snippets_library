/*                                                                                                  geany_encoding=koi8-r
 * parseargs.c - parsing command line arguments & print help
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

#include <stdio.h>  // printf
#include <getopt.h> // getopt_long
#include <stdlib.h> // calloc, exit, strtoll
#include <assert.h> // assert
#include <string.h> // strdup, strchr, strlen
#include <strings.h>// strcasecmp
#include <limits.h> // INT_MAX & so on
#include <libintl.h>// gettext
#include <ctype.h>  // isalpha
#include "usefull_macros.h"

const char *helpstring = NULL; // will be inited later, can't init with gettext on this stage

/**
 * @brief sl_helpstring - change standard help header
 * @param str (i) - new format (MAY consist ONE "%s" for progname)
 */
void sl_helpstring(char *s){
    int pcount = 0, scount = 0;
    char *str = s;
    // check `helpstring` and set it to default in case of error
    for(; pcount < 2; str += 2){
        if(!(str = strchr(str, '%'))) break;
        if(str[1] != '%') pcount++; // increment '%' counter if it isn't "%%"
        else{
            str += 2; // pass next '%'
            continue;
        }
        if(str[1] == 's') scount++; // increment "%s" counter
    };
    if(pcount > 1 || pcount != scount){ // amount of pcount and/or scount wrong
        ERRX(_("Wrong helpstring!"));
    }
    helpstring = s;
}

/**
 * @brief myatoll - carefull atoll/atoi
 * @param num (o) - returning value (or NULL if you wish only check number) - allocated by user
 * @param str (i) - string with number must not be NULL
 * @param t   (i) - T_INT for integer or T_LLONG for long long (if argtype would be wided, may add more)
 * @return TRUE if conversion done without errors, FALSE otherwise
 */
static int myatoll(void *num, char *str, sl_argtype_e t){
    long long tmp, *llptr;
    int *iptr;
    char *endptr;
    assert(str);
    assert(num);
    tmp = strtoll(str, &endptr, 0);
    if(endptr == str || *str == '\0' || *endptr != '\0')
        return FALSE;
    switch(t){
        case arg_longlong:
            llptr = (long long*) num;
            *llptr = tmp;
        break;
        case arg_int:
        default:
            if(tmp < INT_MIN || tmp > INT_MAX){
                WARNX(_("Integer out of range"));
                return FALSE;
            }
            iptr = (int*)num;
            *iptr = (int)tmp;
    }
    return TRUE;
}

// the same as myatoll but for double
// There's no NAN & INF checking here (what if they would be needed?)
static int myatod(void *num, const char *str, sl_argtype_e t){
    double tmp, *dptr;
    float *fptr;
    char *endptr;
    assert(str);
    tmp = strtod(str, &endptr);
    if(endptr == str || *str == '\0' || *endptr != '\0')
        return FALSE;
    switch(t){
        case arg_double:
            dptr = (double *) num;
            *dptr = tmp;
        break;
        case arg_float:
        default:
            fptr = (float *) num;
            *fptr = (float)tmp;
        break;
    }
    return TRUE;
}


/**
 * @brief get_optind  - get index of current option in array options
 * @param key (i)     - original key (short or long)
 * @param opt (i)     - returning val of getopt_long
 * @param options (i) - array of options
 * @return index in array
 */
static int get_optind(const char *key, int opt, sl_option_t *options, void (*helpfun)(int, sl_option_t*)){
    int oind = 0, theopt = opt;
    sl_option_t *opts = options;
    assert(opts);
    // `opt` should be ':' for "missed arguments", '?' for "not found" and short flag if found and checked
    if(opt == '?'){ // not found
        fprintf(stderr, _("No such parameter: `%s`\n"), key);
        helpfun(-1, options);
        return -1; // never reached until `helpfun` changed
    }else if(opt == ':') theopt = optopt; // search to show helpstring "need parameter"
    for(oind = 0; opts->help && opts->val != theopt; oind++, opts++){
        DBG("cmp %c and %c", theopt, opts->val);
    }
    if(!opts->help) return -1;
    if(opt == ':'){
        fprintf(stderr, _("Parameter `%s` needs value\n"), key);
        helpfun(oind, options);
        return -1; // never reached until `helpfun` changed
    }
    return oind;
}


/**
 * @brief get_aptr - reallocate new value in array of multiple repeating arguments
 * @arg paptr (io) - address of pointer to array (**void)
 * @arg type       - its type (for realloc)
 * @return pointer to new (next) value
 */
void *get_aptr(void *paptr, sl_argtype_e type){
    int i = 1;
    void **aptr = *((void***)paptr);
    if(aptr){ // there's something in array
        void **p = aptr;
        while(*p++) ++i;
    }
    size_t sz = 0;
    switch(type){
        default:
        case arg_none:
            ERRX(_("Can't use multiple args with arg_none!"));
        break;
        case arg_int:
            sz = sizeof(int);
        break;
        case arg_longlong:
            sz = sizeof(long long);
        break;
        case arg_double:
            sz = sizeof(double);
        break;
        case arg_float:
            sz = sizeof(float);
        break;
        case arg_string:
            sz = 0;
        break;
    /*  case arg_function:
            sz = sizeof(argfn *);
        break;*/
    }
    aptr = realloc(aptr, (i + 1) * sizeof(void*));
    *((void***)paptr) = aptr;
    aptr[i] = NULL;
    if(sz){
        aptr[i - 1] = malloc(sz);
    }else
        aptr[i - 1] = &aptr[i - 1];
    return aptr[i - 1];
}

/**
 * @brief sl_parseargs_hf - parse arguments with user help funtion
 * @param argc - amount of arguments
 * @param argv - arguments
 * @param options - array with opts
 * @param helpfun - function called in case of wrong arg
 */
void sl_parseargs_hf(int *argc, char ***argv, sl_option_t *options, void (*helpfun)(int, sl_option_t*)){
    char *short_options, *soptr;
    struct option *long_options, *loptr;
    int optsize = 0;
    sl_option_t *opts = options;
    // check whether there is at least one options
    assert(opts);
    //assert(opts[0].name);
    // first we count how much values are in opts
    for(optsize = 0; opts->help; optsize++, opts++);
    // now we can allocate memory
    // TODO: FREE all unneeded memory at end of function
    short_options = calloc(optsize * 3 + 2, 1); // multiply by three for '::' in case of args in opts, add 1 for starting ':'
    long_options = calloc(optsize + 1, sizeof(struct option));
    opts = options; loptr = long_options; soptr = short_options;
    *soptr++ = ':';
    // check the parameters are not repeated
    char **longlist = MALLOC(char*, optsize);
    char *shortlist = MALLOC(char, optsize);
    // fill short/long parameters and make a simple checking
    for(int i = 0; i < optsize; i++, loptr++, opts++){
        // check
        //assert(opts->name); // check name
        DBG("opts: val=%c, name=%s", opts->val, opts->name);
        longlist[i] = opts->name ? strdup(opts->name) : NULL;
        if(opts->has_arg){
            assert(opts->type != arg_none); // check error with arg type
            assert(opts->argptr);  // check pointer
        }
        if(opts->type != arg_none) // if there is a flag without arg, check its pointer
            assert(opts->argptr);
        // fill long_options
        // don't do memcmp: what if there would be different alignment?
        loptr->name     = opts->name ? opts->name : "";
        loptr->has_arg  = (opts->has_arg < MULT_PAR) ? opts->has_arg : 1;
        loptr->flag     = opts->flag;
        loptr->val      = opts->val;
        // fill short options if they are:
        if(!opts->flag && opts->val){
            shortlist[i] = (char) opts->val;
            *soptr++ = opts->val;
            if(loptr->has_arg) // add ':' if option has required argument
                *soptr++ = ':';
            if(loptr->has_arg == 2) // add '::' if option has optional argument
                *soptr++ = ':';
        }
    }
    // sort all lists & check for repeating
    int cmpstringp(const void *p1, const void *p2){
        if(!p1 || !p2) return 0;
        const char *str1 = * (char * const *) p1, *str2 = * (char * const *) p2;
        if(!str1 && !str2) return 0;
        else if(!str1) return 1;
        else if(!str2) return -1;
        return strcmp(str1, str2);
    }
    int cmpcharp(const void *p1, const void *p2){
        return (int)(*(char * const)p1 - *(char *const)p2);
    }
    qsort(longlist, optsize, sizeof(char *), cmpstringp);
    qsort(shortlist,optsize, sizeof(char), cmpcharp);
    char *prevl = longlist[0], prevshrt = shortlist[0];
    // check for repeated args
    for(int i = 1; i < optsize; ++i){
        if(longlist[i] && *longlist[i]){
            if(prevl){
                if(strcmp(prevl, longlist[i]) == 0) ERRX(_("double long arguments: --%s"), prevl);
            }
            prevl = longlist[i];
        }
        if(shortlist[i]){
            if(prevshrt){
                if(prevshrt == shortlist[i]) ERRX(_("double short arguments: -%c"), prevshrt);
            }
            prevshrt = shortlist[i];
        }
    }
    FREE(longlist); FREE(shortlist);
#ifdef EBUG
    DBG("Argc=%d, argv[0]=%s, argv[1]=%s, short=%s", *argc, (*argv)[0], (*argv)[1], short_options);
    for(int _ = 0; _ <= optsize; ++_) fprintf(stderr, "\tlo[%d]='%s'\n", _, long_options[_].name);
    DBG("AND argv:");
    for(int _ = 0; _ < *argc; ++_) fprintf(stderr, "\t[%d]='%s'\n", _ ,(*argv)[_]);
#endif
    // reinit global `optind` for ability of sequentional run
    optind = 1;
    // now we have both long_options & short_options and can parse `getopt_long`
    while(1){
        int opt;
        int /*oindex = -1,*/ loptind = -1; // oindex - number of option in long_options, optind - number in options[]
        DBG("optind=%d", optind);
        const char *curopt = (*argv)[optind];
        if((opt = getopt_long(*argc, *argv, short_options, long_options, &loptind)) == -1) break;
        DBG("search `%s`, %c(%d) = getopt_long(argc, argv, %s, long_options, &%d); optopt=%c(%d), errno=%d", curopt, opt, opt, short_options, loptind, optopt, optopt, errno);
        if(loptind < 0 ) loptind = get_optind(curopt, opt, options, helpfun); // find short option -> need to know index of long
        if(loptind < 0 || loptind >= optsize) continue;
        // be careful with "-?" flag: all wrong or ambiguous flags will be interpreted as this!
        DBG("index=%d", loptind);
        opts = &options[loptind];
        DBG("Got option %s (%c)", opts->name, opts->val);
        // now check option
        if(opts->has_arg == NEED_ARG || opts->has_arg == MULT_PAR)
            if(!optarg) helpfun(loptind, options); // need argument
        void *aptr;
        if(opts->has_arg == MULT_PAR){
            aptr = get_aptr(opts->argptr, opts->type);
        }else
            aptr = opts->argptr;
        int result = TRUE;
        // even if there is no argument, but argptr != NULL, think that optarg = "1"
        if(!optarg) optarg = "1";
        const char *type = NULL;
        switch(opts->type){
            default:
            case arg_none:
                if(opts->argptr) *((int*)aptr) += 1; // increment value
                break;
            case arg_int:
                result = myatoll(aptr, optarg, arg_int);
                type = "integer";
                break;
            case arg_longlong:
                result = myatoll(aptr, optarg, arg_longlong);
                type = "long long";
                break;
            case arg_double:
                result = myatod(aptr, optarg, arg_double);
                type = "double";
                break;
            case arg_float:
                result = myatod(aptr, optarg, arg_float);
                type = "float";
                break;
            case arg_string:
                result = (*((void**)aptr) = (void*)strdup(optarg)) != NULL;
                type = "string";
                break;
            case arg_function:
                result = ((sl_argfn_t)aptr)(optarg);
                break;
        }
        if(!result){
            if(type) fprintf(stderr, _("Need argument with %s type\n"), type);
            helpfun(loptind, options);
        }
    }
    FREE(short_options); FREE(long_options);
    *argc -= optind;
    *argv += optind;
}
/**
 * @brief sl_parseargs - parse command line arguments
 * ! If arg is string, then value will be strdup'ed!
 *
 * @param argc (io) - address of argc of main(), return value of argc stay after `getopt`
 * @param argv (io) - address of argv of main(), return pointer to argv stay after `getopt`
 * BE CAREFUL! if you wanna use full argc & argv, save their original values before
 *      calling this function
 * @param options (i) - array of `myoption` for arguments parcing
 *
 * @exit: in case of error this function show help & make `exit(-1)`
  */
void sl_parseargs(int *argc, char ***argv, sl_option_t *options){
    sl_parseargs_hf(argc, argv, options, sl_showhelp);
}

/**
 * @brief argsort - compare function for qsort
 * first - sort by short options; second - sort arguments without sort opts (by long options)
 */
static int argsort(const void *a1, const void *a2){
    const sl_option_t *o1 = (sl_option_t*)a1, *o2 = (sl_option_t*)a2;
    const char *l1 = o1->name, *l2 = o2->name;
    int s1 = o1->val, s2 = o2->val;
    int *f1 = o1->flag, *f2 = o2->flag;
    // check if both options has short arg
    if(f1 == NULL && f2 == NULL && s1 && s2){ // both have short arg
        return (s1 - s2);
    }else if((f1 != NULL || !s1) && (f2 != NULL || !s2)){ // both don't have short arg - sort by long
        assert(l1); assert(l2); // no way to omit long option if short is absent
        return strcmp(l1, l2);
    }else{ // only one have short arg -- return it
        if(f2 || !s2) return -1; // a1 have short - it is 'lesser'
        else return 1;
    }
}

// print one string of help
static void pr_helpstring(sl_option_t *opt, char *buf, int indent, size_t bufsz){
    size_t p = sprintf(buf, "  "); // a little indent
    int havelongopt = opt->name && *opt->name;
    if(!opt->flag && opt->val){ // .val is short argument
        p += snprintf(buf+p, bufsz-p, "-%c", opt->val);
        if(havelongopt) p += snprintf(buf+p, bufsz-p, ", "); // show comma only it there's shor arg
    }
    if(havelongopt){
        p += snprintf(buf+p, bufsz-p, "--%s", opt->name);
        if(opt->has_arg == NEED_ARG || opt->has_arg == MULT_PAR) // required argument
            p += snprintf(buf+p, bufsz-p, "=arg");
        else if(opt->has_arg == OPT_ARG) // optional argument
            p += snprintf(buf+p, bufsz-p, "[=arg]");
    }else{
        if(opt->has_arg == NEED_ARG || opt->has_arg == MULT_PAR)
            p += snprintf(buf+p, bufsz-p, " arg");
        else if(opt->has_arg == OPT_ARG)
            p += snprintf(buf+p, bufsz-p, " [arg]");
    }
    if(indent > 0){
        assert(p < (size_t)indent);
        printf("%-*s%s", indent+1, buf, _(opt->help)); // write options & at least 2 spaces after
    }else printf("%s   %s", buf, _(opt->help));
    if(opt->has_arg == MULT_PAR) printf(" (can occur multiple times)");
    printf("\n");
}

/**
 * @brief sl_showhelp - show help information based on sl_option_t->help values
 * @param oindex (i)  - if non-negative, show only help by sl_option_t[oindex].help
 * @param options (i) - array of `sl_option_t`
 *
 * @exit:  run `exit(-1)` !!!
 */
void sl_showhelp(int oindex, sl_option_t *options){
    int max_opt_len = 0; // max len of options substring - for right indentation
    const int bufsz = 255;
    char buf[bufsz+1];
    sl_option_t *opts = options;
    assert(opts);
    assert(opts[0].help); // check whether there is at least one options
    // count amount of options
    int N; for(N = 0; opts->help; ++N, ++opts);
    if(N == 0) exit(-2);
    DBG("got %d options, oindex=%d", N, oindex);
    if(oindex > -1){ // print only one message
        if(oindex >= N || oindex < 0) ERRX(_("sl_showhelp(): option index out of range"));
        pr_helpstring(&options[oindex], buf, 0, bufsz);
        exit(-1);
    }
    // header, by default is just "progname\n"
    printf("\n");
    if(!helpstring) helpstring = _("Usage: %s [arguments]\n");
    if(strstr(helpstring, "%s")) // print progname
        printf(helpstring, __progname);
    else // only text
        printf("%s", helpstring);
    printf("\n");
    // count max_opt_len
    for(int _ = 0; _ < N; ++_){
        if(!options[_].name) continue;
        int L = strlen(options[_].name);
        if(max_opt_len < L) max_opt_len = L;
    }
    max_opt_len += 14; // format: '-S, --long[=arg]' - get addition 13 symbols
    // Now print all help (sorted)
    qsort(options, N, sizeof(sl_option_t), argsort);
    do{
        pr_helpstring(options++, buf, max_opt_len, bufsz);
    }while(--N);
    printf("\n\n");
    exit(-1);
}

/**
 * @brief sl_get_suboption - get suboptions from parameter string
 * @param str - parameter string
 * @param opt - pointer to suboptions structure
 * @return TRUE if all OK
 */
int sl_get_suboption(char *str, sl_suboption_t *opt){
    int findsubopt(char *par, sl_suboption_t *so){
        int idx = 0;
        if(!par) return -1;
        while(so[idx].name){
            if(strcasecmp(par, so[idx].name) == 0) return idx;
            ++idx;
        }
        return -1; // badarg
    }
    int opt_setarg(sl_suboption_t *so, int idx, char *val){
        sl_suboption_t *soptr = &so[idx];
        int result = FALSE;
        void *aptr = soptr->argptr;
        switch(soptr->type){
            default:
            case arg_none:
                if(soptr->argptr) *((int*)aptr) += 1; // increment value
                result = TRUE;
            break;
            case arg_int:
                result = myatoll(aptr, val, arg_int);
            break;
            case arg_longlong:
                result = myatoll(aptr, val, arg_longlong);
            break;
            case arg_double:
                result = myatod(aptr, val, arg_double);
            break;
            case arg_float:
                result = myatod(aptr, val, arg_float);
            break;
            case arg_string:
                result = (*((void**)aptr) = (void*)strdup(val)) != NULL;
            break;
            case arg_function:
                result = ((sl_argfn_t)aptr)(val);
            break;
        }
        return result;
    }
    char *tok;
    int ret = FALSE;
    char *tmpbuf;
    tok = strtok_r(str, ":,", &tmpbuf);
    do{
        char *val = strchr(tok, '=');
        int noarg = 0;
        if(val == NULL){ // no args
            val = "1";
            noarg = 1;
        }else{
            *val++ = '\0';
            if(!*val || *val == ':' || *val == ','){ // no argument - delimeter after =
                val = "1"; noarg = 1;
            }
        }
        int idx = findsubopt(tok, opt);
        if(idx < 0){
            WARNX(_("Wrong parameter: %s"), tok);
            goto returning;
        }
        if(noarg && opt[idx].has_arg == NEED_ARG){
            WARNX(_("%s: need argument!"), tok);
            goto returning;
        }
        if(!opt_setarg(opt, idx, val)){
            WARNX(_("Wrong argument \"%s\" of parameter \"%s\""), val, tok);
            goto returning;
        }
    }while((tok = strtok_r(NULL, ":,", &tmpbuf)));
    ret = TRUE;
returning:
    return ret;
}

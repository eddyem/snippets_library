/*
 * This file is part of the Snippets project.
 * Copyright 2026 Edward V. Emelianov <edward.emelianoff@gmail.com>.
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

// different sorting options

#include <string.h>

// compare chars
int sl_cmpchar(const void *p1, const void *p2){
    if(!p1 || !p2) return 0;
    return (int)(*(char * const)p1 - *(char *const)p2);
}

// compare integers
int sl_cmpint(const void *p1, const void *p2){
    if(!p1 || !p2) return 0;
    int i1 = *(const int *) p1, i2 = *(const int *) p2;
    return i1 - i2;
}

// compare doubles
int sl_cmpdouble(const void *p1, const void *p2){
    if(!p1 || !p2) return 0;
    double d1 = *(const double *) p1, d2 = *(const double *) p2;
    if(d1 > d2) return 1;
    else if(d1 < d2) return -1;
    return 0;
}

// compare strings
int sl_cmpstring(const void *p1, const void *p2){
    if(!p1 || !p2) return 0;
    const char *str1 = (const char *) p1, *str2 = (const char *) p2;
    return strcmp(str1, str2);
}

// compare strings by pointer to them
int sl_cmpstringp(const void *p1, const void *p2){
    if(!p1 || !p2) return 0;
    const char *str1 = * (char * const *) p1, *str2 = * (char * const *) p2;
    if(!str1 && !str2) return 0;
    else if(!str1) return 1;
    else if(!str2) return -1;
    return strcmp(str1, str2);
}


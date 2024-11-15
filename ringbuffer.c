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

#include <pthread.h>
#include <string.h>
#include "usefull_macros.h"

/**
 * @brief sl_RB_new - create ringbuffer with `size` bytes
 * @param size - RB size
 * @return RB
 */
sl_ringbuffer *sl_RB_new(size_t size){
    sl_ringbuffer *b = MALLOC(sl_ringbuffer, 1);
    b->data = MALLOC(uint8_t, size);
    pthread_mutex_init(&b->busy, NULL);
    b->head = b->tail = 0;
    b->length = size;
    return b;
}

/**
 * @brief sl_RB_delete - free ringbuffer
 * @param b - buffer to free
 */
void sl_RB_delete(sl_ringbuffer **b){
    if(!b || !*b) return;
    sl_ringbuffer *bptr = *b;
    pthread_mutex_lock(&bptr->busy);
    FREE(bptr->data);
    *b = 0;
    pthread_mutex_unlock(&bptr->busy);
    FREE(bptr);
}

/**
 * @brief datalen - amount of bytes in buffer
 * @param b - buffer
 * @return N
 */
static size_t datalen(sl_ringbuffer *b){
    if(b->tail >= b->head) return (b->tail - b->head);
    else return (b->length - b->head + b->tail);
}

// datalen but with blocking of RB
size_t sl_RB_datalen(sl_ringbuffer *b){
    pthread_mutex_lock(&b->busy);
    size_t l = datalen(b);
    pthread_mutex_unlock(&b->busy);
    return l;
}

/**
 * @brief hasbyte - check if RB have given byte
 * @param b - rb
 * @param byte - what to find
 * @return index of byte or -1 if not found or no data
 */
static ssize_t hasbyte(sl_ringbuffer *b, uint8_t byte){
    if(b->head == b->tail) return -1; // no data in buffer
    size_t startidx = b->head;
    if(b->head > b->tail){
        for(size_t found = b->head; found < b->length; ++found)
            if(b->data[found] == byte) return found;
        startidx = 0;
    }
    for(size_t found = startidx; found < b->tail; ++found)
        if(b->data[found] == byte) return found;
    return -1;
}

// hasbyte with block
ssize_t sl_RB_hasbyte(sl_ringbuffer *b, uint8_t byte){
    pthread_mutex_lock(&b->busy);
    size_t idx = hasbyte(b, byte);
    pthread_mutex_unlock(&b->busy);
    return idx;
}
// increment head or tail
inline void incr(sl_ringbuffer *b, volatile size_t *what, size_t n){
    *what += n;
    if(*what >= b->length) *what -= b->length;
}

static size_t rbread(sl_ringbuffer *b, uint8_t *s, size_t len){
    size_t l = datalen(b);
    if(!l) return 0;
    if(l > len) l = len;
    size_t _1st = b->length - b->head;
    if(_1st > l) _1st = l;
    if(_1st > len) _1st = len;
    memcpy(s, b->data + b->head, _1st);
    if(_1st < len && l > _1st){
        memcpy(s+_1st, b->data, l - _1st);
        incr(b, &b->head, l);
        return l;
    }
    incr(b, &b->head, _1st);
    return _1st;
}

/**
 * @brief sl_RB_read - read data from rb
 * @param b - rb
 * @param s - buffer for data
 * @param len - length of `s`
 * @return amount of bytes read
 */
size_t sl_RB_read(sl_ringbuffer *b, uint8_t *s, size_t len){
    pthread_mutex_lock(&b->busy);
    size_t got = rbread(b, s, len);
    pthread_mutex_unlock(&b->busy);
    return got;
}

/**
 * @brief sl_RB_readto - read until meet byte `byte`
 * @param b - rb
 * @param byte - byte to find
 * @param s - receiver
 * @param len - length of `s`
 * @return amount of bytes read or -1 if `s` have insufficient size
 */
ssize_t sl_RB_readto(sl_ringbuffer *b, uint8_t byte, uint8_t *s, size_t len){
    ssize_t got = 0;
    pthread_mutex_lock(&b->busy);
    ssize_t idx = hasbyte(b, byte);
    if(idx < 0) goto ret;
    size_t partlen = idx + 1 - b->head;
    // now calculate length of new data portion
    if((size_t)idx < b->head) partlen += b->length;
    if(partlen > len) got = -1;
    else got = rbread(b, s, partlen);
ret:
    pthread_mutex_unlock(&b->busy);
    return got;
}

/**
 * @brief sl_RB_readline - read string of text ends with '\n'
 * @param b - rb
 * @param s - string buffer
 * @param len - length of `s`
 * @return amount of characters read or -1 if buffer too small
 * !!! this function changes '\n' to 0 in `s`; `len` should include trailing '\0' too
 */
ssize_t sl_RB_readline(sl_ringbuffer *b, char *s, size_t len){
    ssize_t got = 0;
    pthread_mutex_lock(&b->busy);
    ssize_t idx = hasbyte(b, '\n');
    if(idx < 0) goto ret;
    size_t partlen = idx + 1 - b->head;
    // now calculate length of new data portion
    if((size_t)idx < b->head) partlen += b->length;
    if(partlen > len) got = -1;
    else{
        got = rbread(b, (uint8_t*)s, partlen);
        s[partlen - 1] = 0; // substitute '\n' with trailing zero
    }
ret:
    pthread_mutex_unlock(&b->busy);
    return got;
}

/**
 * @brief sl_RB_putbyte - put one byte into rb
 * @param b - rb
 * @param byte - data byte
 * @return FALSE if there's no place for `byte` in `b`
 */
int sl_RB_putbyte(sl_ringbuffer *b, uint8_t byte){
    int rtn = FALSE;
    pthread_mutex_lock(&b->busy);
    size_t s = datalen(b);
    if(b->length == s + 1) goto ret;
    b->data[b->tail] = byte;
    incr(b, &b->tail, 1);
    rtn = TRUE;
ret:
    pthread_mutex_unlock(&b->busy);
    return rtn;
}

/**
 * @brief sl_RB_write - write data to buffer
 * @param b - buffer
 * @param str - data
 * @param len - data length
 * @return amount of bytes wrote (can be less than `len`)
 */
size_t sl_RB_write(sl_ringbuffer *b, const uint8_t *str, size_t len){
    pthread_mutex_lock(&b->busy);
    size_t r = b->length - 1 - datalen(b); // rest length
    if(len > r) len = r;
    if(!len){ r = 0; goto ret; }
    size_t _1st = b->length - b->tail;
    if(_1st > len) _1st = len;
    memcpy(b->data + b->tail, str, _1st);
    if(_1st < len){ // add another piece from start
        memcpy(b->data, str+_1st, len-_1st);
    }
    incr(b, &b->tail, len);
ret:
    pthread_mutex_unlock(&b->busy);
    return r;
}

/**
 * @brief sl_RB_clearbuf - reset buffer
 * @param b - rb
  */
void sl_RB_clearbuf(sl_ringbuffer *b){
    pthread_mutex_lock(&b->busy);
    b->head = 0;
    b->tail = 0;
    pthread_mutex_unlock(&b->busy);
}

/**
 * @brief sl_RB_writestr - write FULL string `s` to buffer (without trailing zero!)
 * @param b - rb
 * @param s - string
 * @return amount of bytes written (strlen of s) or 0
 */
size_t sl_RB_writestr(sl_ringbuffer *b, char *s){
    size_t len = strlen(s);
    pthread_mutex_lock(&b->busy);
    size_t r = b->length - 1 - datalen(b); // rest length
    if(len > r){ len = 0; goto ret; } // insufficient space - don't even try to write a part
    size_t _1st = b->length - b->tail;
    if(_1st > len) _1st = len;
    memcpy(b->data + b->tail, s, _1st);
    if(_1st < len){ // add another piece from start
        memcpy(b->data, s+_1st, len-_1st);
    }
    incr(b, &b->tail, len);
ret:
    pthread_mutex_unlock(&b->busy);
    return len;
}

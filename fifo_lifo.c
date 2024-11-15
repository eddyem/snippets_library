/*
 * fifo_lifo.c - simple FIFO/LIFO
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

#include "usefull_macros.h"

/**
 * @brief sl_list_push_tail - push data into the tail of a stack (like FIFO)
 * @param lst (io) - list
 * @param v (i)    - data to push
 * @return pointer to just pused node or NULL in case of error
 */
sl_list_t *sl_list_push_tail(sl_list_t **lst, void *v){
    sl_list_t *node;
    if(!lst) return NULL;
    if((node = MALLOC(sl_list_t, 1)) == NULL)
        return NULL; // allocation error
    node->data = v; // insert data
    if(!*lst){
        *lst = node;
    }else{
        (*lst)->last->next = node;
    }
    (*lst)->last = node;
    return node;
}

/**
 * @brief sl_list_push - push data into the head of a stack (like LIFO)
 * @param lst (io) - list
 * @param v (i)    - data to push
 * @return pointer to just pused node
 */
sl_list_t *sl_list_push(sl_list_t **lst, void *v){
    sl_list_t *node;
    if(!lst) return NULL;
    if((node = MALLOC(sl_list_t, 1)) == NULL)
        return NULL; // allocation error
    node->data = v; // insert data
    if(!*lst){
        *lst = node;
        (*lst)->last = node;
        return node;
    }
    node->next = *lst;
    node->last = (*lst)->last;
    *lst = node;
    return node;
}

/**
 * @brief sl_list_pop - get data from head of list. Don't forget to FREE list data after `pop`
 * @param lst (io) - list
 * @return data from lst head
 */
void *sl_list_pop(sl_list_t **lst){
    void *ret;
    sl_list_t *node = *lst;
    if(!lst || !*lst) return NULL;
    ret = (*lst)->data;
    *lst = (*lst)->next;
    FREE(node);
    return ret;
}

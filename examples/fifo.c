/*
 * This file is part of the usefull_macros project.
 * Copyright 2018 Edward V. Emelianov <edward.emelianoff@gmail.com>, <eddy@sao.ru>.
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
#include <usefull_macros.h>
#include <stdio.h>

/*
 * Example of FIFO/LIFO usage
 */

int main(int argc, char *argv[argc]) {
    sl_list_t *f = NULL;
    printf("Available memory: %luMB\n", sl_mem_avail()/1024/1024);
    sl_init();
    // specific output in non-tty
    if(argc == 1){
        green("Usage:\n\t%s args - fill fifo with arguments\n", __progname);
        return 1;
    }
    red("\n\nLIFO example\n");
    for(int i = 1; i < argc; ++i){
        if(!sl_list_push(&f, argv[i])) ERR("Allocation error!");
        green("push to list ");
        printf("%s\n", argv[i]);
    }
    char *d;
    printf("\n");
    while(f){
        d = sl_list_pop(&f);
        green("pull: ");
        printf("%s\n", d);
    }
    red("\n\nFIFO example\n");
    for(int i = 1; i < argc; ++i){
        if(!sl_list_push_tail(&f, argv[i])) ERR("Allocation error!");
        green("push to list ");
        printf("%s\n", argv[i]);
    }
    printf("\n");
    while(f){
        d = sl_list_pop(&f);
        green("pull: ");
        printf("%s\n", d);
    }
    return 0;
}

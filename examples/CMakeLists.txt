cmake_minimum_required(VERSION 3.9)
project(examples)

# common includes & library
include_directories(../)
link_libraries(usefull_macros)

# exe list
add_executable(helloworld helloworld.c)
add_executable(options options.c cmdlnopts.c)
add_executable(fifo fifo.c)
add_executable(conffile conffile.c)
add_executable(clientserver clientserver.c)
add_executable(ringbuffer ringbuffer.c)

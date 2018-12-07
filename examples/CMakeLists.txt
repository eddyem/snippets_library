cmake_minimum_required(VERSION 3.9)
project(examples)

# common includes & library
include_directories(../)
link_libraries(usefull_macros)

# exe list
add_executable(helloworld helloworld.c)
add_executable(options options.c cmdlnopts.c)

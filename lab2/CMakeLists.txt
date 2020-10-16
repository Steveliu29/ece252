cmake_minimum_required(VERSION 3.17)
project(lab2 C)

set(CMAKE_C_STANDARD 11)

include_directories(starter/times)

add_executable(lab2
        starter/cURL/main_simple.c
        starter/cURL/main_write_callback.c
        starter/cURL/main_write_header_cb.c
        starter/fn_ptr/main.c
        starter/getopt/main_getopt.c
        starter/pthreads/main.c
        starter/times/main.c
        starter/times/util_times.c
        starter/times/util_times.h
        code/lab_png.h
        code/lab_png.c
        code/helper.c
        code/paster.c)

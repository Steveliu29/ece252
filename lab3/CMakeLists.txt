cmake_minimum_required(VERSION 3.17)
project(lab3 C)

set(CMAKE_C_STANDARD 11)

include_directories(code)
include_directories(starter/shm)

add_executable(lab3
        code/catpng.c
        code/crc.c
        code/crc.h
        code/helper.c
        code/lab_png.c
        code/lab_png.h
        code/paster2.c
        code/zutil.c
        code/zutil.h
        starter/cURL_IPC/main.c
        starter/cURL_IPC/main_2proc.c
        starter/fork/forkN.c
        starter/fork/zombie.c
        starter/sem/sem_simple.c
        starter/shm/shm_simple.c
        starter/shm/shm_stack.c
        starter/shm/shm_stack.h
        starter/shm/shm_struct.c)

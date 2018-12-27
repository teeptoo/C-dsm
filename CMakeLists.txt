cmake_minimum_required(VERSION 3.12)
project(PR204)

set(CMAKE_CXX_STANDARD 14)

add_executable(PR204_cmake
        common.c
        common_impl.h
        dsm.c
        dsm.h
        dsmexec.c
        dsmwrap.c
        exemple.c)

add_custom_target(PR204 COMMAND make clean -C ${PR204_SOURCE_DIR}
        COMMAND make -C ${PR204_SOURCE_DIR}
        CLION_EXE_DIR=${PROJECT_BINARY_DIR})

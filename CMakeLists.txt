cmake_minimum_required(VERSION 3.25)

project(executable LANGUAGES C CXX)

find_package(Threads REQUIRED)

add_executable(syscallmeter ./main.c ./progress.c ./w_open.c ./w_rename.c ./w_write_unlink.c ./w_write_sync.c ./w_clock_gettime.c)

target_link_libraries(syscallmeter ${CMAKE_THREAD_LIBS_INIT} rt)

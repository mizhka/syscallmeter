cmake_minimum_required(VERSION 3.31)

project(executable LANGUAGES C CXX)

add_executable(syscallmeter ./main.c ./w_open.c ./w_rename.c ./w_write_unlink.c)

cmake_minimum_required(VERSION 3.31)
project(slog C)

set(CMAKE_C_STANDARD 90)

add_library(slog slog.c slog.h)
add_executable(slog_example example.c)
target_link_libraries(slog_example PRIVATE slog)

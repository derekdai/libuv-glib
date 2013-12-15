cmake_minimum_required(VERSION 2.8)

project(test-uv-gtk)

find_package(PkgConfig)
pkg_check_modules(GTK REQUIRED gtk+-3.0)
include_directories(${GTK_INCLUDE_DIRS})
link_directories(${GTK_LIBRARY_DIRS})

add_executable(test-uv-gtk test-uv-gtk)
target_link_libraries(test-uv-gtk ${GTK_LIBRARIES} uv)

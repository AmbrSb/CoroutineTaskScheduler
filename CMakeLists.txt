# BSD 3-Clause License
#
# Copyright (c) 2021, Amin Saba
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived from
#    this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(deq VERSION 0.0.1 DESCRIPTION "coroutine-based task manager")

set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_COMPILER "/usr/bin/g++-10")

option(LIBRARY_MODE "build a library with minimal header code" ON)

#include(InstallRequiredSystemLibraries)
#include(GNUInstallDirs)

set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

find_package(Git)
#find_package(Threads)

find_program(GIT_EXE NAMES git)

add_compile_options(
    # -Wall -Wpedantic -fexceptions
    "$<$<CONFIG:Debug>:-O0;-g3;-ggdb;-fcoroutines>"
    "$<$<CONFIG:Release>:-O2;-fcoroutines>"
)

add_compile_definitions(
    FORTIFY_SOURCE=2
    "$<$<CONFIG:Debug>:MALLOC_CHECK_=3;_GLIBCXX_DEBUG>"
)

if(GIT_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    message(WARNING "Configre Git!")
elseif(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --max-count=1 HEAD
            OUTPUT_VARIABLE GIT_REVISION
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            ERROR_QUIET
            )
    if(NOT ${GIT_REVISION} STREQUAL "")
            string(STRIP ${GIT_REVISION} GIT_REVISION)
    endif()
endif()

include_directories(/usr/local/include src)
link_directories(/usr/local/lib)

add_executable(sched_test_01 tests/sched_test_01.cpp)

if (LIBRARY_MODE)
    add_compile_definitions(LIBRARY_MODE)
    add_library(scheduler SHARED ${CMAKE_SOURCE_DIR}/src/scheduler_lib.cpp)
    target_link_libraries(sched_test_01 scheduler gtest gtest_main pthread)
else()
    target_link_libraries(sched_test_01 gtest gtest_main pthread)
endif()

enable_testing()

add_test(SCHED_TEST     sched_test_01)

include(CTest)
include(CPack)

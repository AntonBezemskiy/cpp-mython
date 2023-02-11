cmake_minimum_required(VERSION 3.11)

project(Mython CXX)
set(CMAKE_CXX_STANDARD 17)

set(HEADER_FILES mython/runtime.h mython/test_runner_p.h mython/lexer.h mython/parse.h mython/statement.h mython/test_runner_p.h)

set(SOURSE_FILES mython/main.cpp mython/runtime.cpp mython/runtime_test.cpp mython/lexer.cpp mython/parse.cpp mython/statement.cpp
                 mython/lexer_test_open.cpp mython/parse_test.cpp mython/runtime_test.cpp mython/statement_test.cpp)

add_executable(mython ${HEADER_FILES} ${SOURSE_FILES})

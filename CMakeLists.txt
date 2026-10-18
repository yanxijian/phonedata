cmake_minimum_required(VERSION 3.15)
project(phonedata LANGUAGES CXX)

option(PHONEDATA_BUILD_TESTS "Build phonedata tests" ON)

add_library(phonedata STATIC phonedata/phonedata.cpp)
target_include_directories(phonedata PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/phonedata")
target_compile_features(phonedata PUBLIC cxx_std_17)

if(MSVC)
    # 源码与数据文件都是 UTF-8，必须显式告诉 MSVC，否则非中文区域的机器会把字面量当成 ANSI。
    target_compile_options(phonedata PUBLIC /utf-8)
    target_compile_options(phonedata PRIVATE /W4)
else()
    target_compile_options(phonedata PRIVATE -Wall -Wextra)
endif()

if(PHONEDATA_BUILD_TESTS)
    enable_testing()

    add_executable(phonedata_test phonedata/test.cpp)
    target_link_libraries(phonedata_test PRIVATE phonedata)
    target_compile_definitions(phonedata_test PRIVATE
        PHONEDATA_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/phonedata/phone.dat")
    add_test(NAME phonedata_test COMMAND phonedata_test)

    # 数据文件完整性校验（需要 Python 3，属于可选步骤）。
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME phonedata_data_validator
                 COMMAND ${Python3_EXECUTABLE}
                         "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_phonedata.py"
                         "${CMAKE_CURRENT_SOURCE_DIR}/phonedata/phone.dat"
                         --expect-count 364528 --expect-version 1707)
    endif()
endif()

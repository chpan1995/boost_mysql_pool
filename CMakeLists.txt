cmake_minimum_required(VERSION 3.5)

project(sqlpoll VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenSSL REQUIRED)
find_package(Boost REQUIRED charconv)
find_package(Threads REQUIRED)

add_subdirectory(demo)

set(PROJECT_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/SqlPool.hpp
        ${CMAKE_CURRENT_LIST_DIR}/SqlPool.cpp
)

add_library(${PROJECT_NAME} ${PROJECT_SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    OpenSSL::SSL
    Threads::Threads
    Boost::charconv
    OpenSSL::Crypto
)

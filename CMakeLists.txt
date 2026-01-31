cmake_minimum_required(VERSION 3.10)
project(temperature_logger CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Поиск SQLite3
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

include_directories(${SQLITE3_INCLUDE_DIRS} include src)

# Основная программа с сервером
add_executable(logger src/main.cpp)
target_link_libraries(logger ${SQLITE3_LIBRARIES} pthread)

# Симулятор (без изменений)
add_executable(simulator src/simulator.cpp)
target_link_libraries(simulator)

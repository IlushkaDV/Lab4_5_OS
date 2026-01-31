cmake_minimum_required(VERSION 3.10)
project(temperature_logger CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(logger src/main.cpp)
target_link_libraries(logger)

add_executable(simulator src/simulator.cpp)
target_link_libraries(simulator)

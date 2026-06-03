cmake_minimum_required(VERSION 3.16)

project(serial-terminal-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets SerialPort Core)

# Find Python and Pybind11
find_package(Python REQUIRED COMPONENTS Interpreter Development.Embed)
include(FetchContent)
FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.12.0
)
FetchContent_MakeAvailable(pybind11)

set(APP_ICON_RESOURCE "")
if(WIN32)
  set(APP_ICON_RESOURCE app.rc)
endif()

add_executable(serial-terminal-qt WIN32
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/SerialWorker.cpp
    src/SerialWorker.h
    src/PluginManager.cpp
    src/PluginManager.h
    resources.qrc
    ${APP_ICON_RESOURCE}
)

target_link_libraries(serial-terminal-qt PRIVATE
    Qt6::Widgets
    Qt6::SerialPort
    Qt6::Core
    pybind11::embed
)
sh
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(serial-terminal-qt PRIVATE -lssp)
endif()

cmake_minimum_required(VERSION 2.8)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/cmake/ArduinoToolchain.cmake)
set(PROJECT_NAME testRTC)
project(${PROJECT_NAME})

link_directories(~/Arduino/libraries)


set(${CMAKE_PROJECT_NAME}_SKETCH testRTC.ino)

generate_arduino_firmware(${CMAKE_PROJECT_NAME}
        SERIAL picocom @SERIAL_PORT@ -b 57600 -l
        PORT  /dev/ttyACM0
        BOARD uno
        )


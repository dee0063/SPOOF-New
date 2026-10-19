cmake_minimum_required(VERSION 3.15)
project(SpoofDriverProject LANGUAGES CXX)

# === Настройки проекта ===
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# === Папки проекта ===
set(SPOOF_DRIVER_SRC "SpoofDriver/src")
set(SPOOF_HOST_SRC "SpoofHost/src")
set(SPOOF_CLI_SRC "SpoofCLI/src")

# === Включаем NT headers для драйвера ===
include_directories(${SPOOF_DRIVER_SRC}/include)

# === 1. СБОРКА ДРАЙВЕРА (SpoofDriver.sys) ===
add_executable(SpoofDriverDriver ${SPOOF_DRIVER_SRC}/SpoofDriver.cpp)

if(WIN32 AND MSVC)
    target_link_options(SpoofDriverDriver PRIVATE /SUBSYSTEM:CONSOLE)
endif()

# Копируем .def файл в директорию драйвера
add_custom_command(TARGET SpoofDriverDriver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/SpoofDriver/SpoofDriver.def"
        "${CMAKE_BINARY_DIR}/bin/SpoofDriver.def")

# === 2. СБОРКА HOST-SERVICE (SpoofHost.exe) ===
add_executable(SpoofHost ${SPOOF_HOST_SRC}/HostMain.cpp)

target_include_directories(SpoofHost PRIVATE
    ${SPOOF_DRIVER_SRC}/include
    ${SPOOF_HOST_SRC}
)

# === 3. СБОРКА CLI (SpoofCLI.exe) ===
add_executable(SpoofCLI ${SPOOF_CLI_SRC}/CLI.cpp)

target_include_directories(SpoofCLI PRIVATE
    ${SPOOF_DRIVER_SRC}/include
    ${SPOOF_HOST_SRC}
    ${SPOOF_CLI_SRC}
)

# === Копирование файлов в директорию сборки ===
add_custom_command(TARGET SpoofHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/SpoofDriver/SpoofDriver.sys"
        "${CMAKE_BINARY_DIR}/bin/")

add_custom_command(TARGET SpoofCLI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/SpoofDriver/SpoofDriver.sys"
        "${CMAKE_BINARY_DIR}/bin/")

# === Установка прав доступа для драйвера (Windows) ===
if(WIN32 AND MSVC)
    add_custom_command(TARGET SpoofHost POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Driver built successfully at: $<TARGET_FILE:SpoofDriverDriver>"
        COMMENT "SpoofDriverProject build complete")
endif()

# === Пакетная установка для Visual Studio ===
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    message(STATUS "=== SpoofDriverProject Configuration ===")
    message(STATUS "  Driver:   SpoofDriver.sys")
    message(STATUS "  Host:     SpoofHost.exe")
    message(STATUS "  CLI:      SpoofCLI.exe")
endif()

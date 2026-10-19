cmake_minimum_required(VERSION 3.15)
project(SpoofDriverProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ВАЖНО: CMakeLists.txt в папке Build, поэтому поднимаемся на уровень выше
set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# === 1. ДРАЙВЕР (SpoofDriver.sys) ===
if(EXISTS "${ROOT_DIR}/SpoofDriver/src/SpoofDriver.cpp")
    add_library(SpoofDriverDriver SHARED ${ROOT_DIR}/SpoofDriver/src/SpoofDriver.cpp)
    target_include_directories(SpoofDriverDriver PRIVATE ${ROOT_DIR}/SpoofDriver/include)
    
    if(MSVC)
        target_compile_options(SpoofDriverDriver PRIVATE /kernel /W4 /GR- /EHs-c-)
        target_link_options(SpoofDriverDriver PRIVATE /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /NODEFAULTLIB)
        target_link_libraries(SpoofDriverDriver PRIVATE ntoskrnl.lib hal.lib)
    endif()
    
    set_target_properties(SpoofDriverDriver PROPERTIES SUFFIX ".sys" PREFIX "")
else()
    message(WARNING "Файл драйвера не найден, сборка драйвера пропущена.")
endif()

# === 2. HOST-SERVICE (SpoofHost.exe) ===
set(SPOOF_HOST_SOURCES
    ${ROOT_DIR}/SpoofHost/src/HostMain.cpp
    ${ROOT_DIR}/SpoofHost/src/HostConfig.cpp
    ${ROOT_DIR}/SpoofHost/src/BehavioralAnalyzer.cpp
)

if(EXISTS "${ROOT_DIR}/SpoofHost/src/HostMain.cpp")
    add_executable(SpoofHost ${SPOOF_HOST_SOURCES})
    target_include_directories(SpoofHost PRIVATE 
        ${ROOT_DIR}/SpoofHost/include
        ${ROOT_DIR}/SpoofHost/src
        ${ROOT_DIR}/SpoofDriver/include
    )
else()
    message(FATAL_ERROR "Не найден HostMain.cpp! Путь: ${ROOT_DIR}/SpoofHost/src/HostMain.cpp")
endif()

# === 3. CLI (SpoofCLI.exe) ===
if(EXISTS "${ROOT_DIR}/SpoofCLI/src/CLI.cpp")
    add_executable(SpoofCLI ${ROOT_DIR}/SpoofCLI/src/CLI.cpp)
    target_include_directories(SpoofCLI PRIVATE 
        ${ROOT_DIR}/SpoofCLI/include
        ${ROOT_DIR}/SpoofHost/include
        ${ROOT_DIR}/SpoofHost/src
        ${ROOT_DIR}/SpoofDriver/include
    )
else()
    message(FATAL_ERROR "Не найден CLI.cpp!")
endif()

# Копирование драйвера
if(TARGET SpoofDriverDriver)
    add_custom_command(TARGET SpoofDriverDriver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SpoofDriverDriver>"
            "${CMAKE_BINARY_DIR}/bin/SpoofDriver.sys"
    )
endif()

message(STATUS "=== Конфигурация завершена ===")
message(STATUS "Корневая папка: ${ROOT_DIR}")
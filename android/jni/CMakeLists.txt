cmake_minimum_required(VERSION 3.22.1)
project(rvm_jni C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

get_filename_component(RVM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

file(GLOB_RECURSE RVM_CORE_SRC CONFIGURE_DEPENDS "${RVM_ROOT}/src/*.c")
list(REMOVE_ITEM RVM_CORE_SRC "${RVM_ROOT}/src/main.c")

if(NOT RVM_CORE_SRC)
    message(FATAL_ERROR "no RVM sources found under ${RVM_ROOT}/src")
endif()

add_library(rvm_jni SHARED rvm_jni.c ${RVM_CORE_SRC})

target_include_directories(rvm_jni PRIVATE
    "${RVM_ROOT}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(rvm_jni PRIVATE
    _POSIX_C_SOURCE=200809L
    _DEFAULT_SOURCE
    RVM_PLATFORM_ANDROID=1)

target_compile_options(rvm_jni PRIVATE
    -fno-omit-frame-pointer
    -fno-strict-aliasing)

find_library(log-lib log)
find_library(android-lib android)

set(RVM_JNI_LIBS m ${log-lib})
find_library(aaudio-lib aaudio)
if(aaudio-lib)
    list(APPEND RVM_JNI_LIBS ${aaudio-lib})
    target_compile_definitions(rvm_jni PRIVATE RVM_HAVE_AAUDIO=1)
    message(STATUS "rvm_jni: AAudio found, virtio-snd output enabled")
else()
    message(STATUS "rvm_jni: AAudio not found, building without audio")
endif()
if(android-lib)
    list(APPEND RVM_JNI_LIBS ${android-lib})
endif()

target_link_libraries(rvm_jni ${RVM_JNI_LIBS})

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set_target_properties(rvm_jni PROPERTIES LINK_FLAGS "-Wl,--gc-sections -Wl,--as-needed")
endif()

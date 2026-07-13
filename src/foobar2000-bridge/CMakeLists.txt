cmake_minimum_required(VERSION 3.28...3.30)
project(foo_obsbridge VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT FOOBAR2000_SDK_DIR)
  set(FOOBAR2000_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foobar2000-sdk")
endif()

if(NOT EXISTS "${FOOBAR2000_SDK_DIR}/foobar2000/SDK/component.h")
  set(_SDK_URL "https://github.com/pnck/foobar2000_sdk/archive/refs/tags/2025-03-07.zip")
  set(_SDK_ZIP "${CMAKE_CURRENT_BINARY_DIR}/foobar2000_sdk.zip")

  message(STATUS "Downloading foobar2000 SDK...")
  file(DOWNLOAD "${_SDK_URL}" "${_SDK_ZIP}" STATUS _dl_status)
  list(GET _dl_status 0 _dl_code)
  if(NOT _dl_code EQUAL 0)
    message(FATAL_ERROR "Failed to download foobar2000 SDK from ${_SDK_URL}")
  endif()

  execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf "${_SDK_ZIP}"
                  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
  file(RENAME "${CMAKE_CURRENT_BINARY_DIR}/foobar2000_sdk-2025-03-07"
       "${FOOBAR2000_SDK_DIR}")
  file(REMOVE "${_SDK_ZIP}")
endif()

if(NOT EXISTS "${FOOBAR2000_SDK_DIR}/foobar2000/SDK/component.h")
  message(
    FATAL_ERROR
      "foobar2000 SDK not found at ${FOOBAR2000_SDK_DIR}\n"
      "Please set FOOBAR2000_SDK_DIR to the foobar2000 SDK root.\n"
      "Download from: https://www.foobar2000.org/SDK")
endif()

file(GLOB _sdk_sources "${FOOBAR2000_SDK_DIR}/foobar2000/SDK/*.cpp")
file(GLOB _pfc_sources "${FOOBAR2000_SDK_DIR}/pfc/*.cpp")
list(FILTER _pfc_sources EXCLUDE REGEX "nix-objects\\.cpp$")
list(FILTER _pfc_sources EXCLUDE REGEX "synchro_nix\\.cpp$")
list(FILTER _pfc_sources EXCLUDE REGEX "stdafx\\.cpp$")
list(FILTER _pfc_sources EXCLUDE REGEX "pfc-fb2k-hooks\\.cpp$")

add_library(foo_obsbridge SHARED
  foo_obsbridge.cpp
  "${FOOBAR2000_SDK_DIR}/foobar2000/foobar2000_component_client/component_client.cpp"
  ${_sdk_sources}
  ${_pfc_sources})

target_include_directories(foo_obsbridge PRIVATE
  "${FOOBAR2000_SDK_DIR}"
  "${FOOBAR2000_SDK_DIR}/foobar2000"
  "${FOOBAR2000_SDK_DIR}/pfc")

target_link_libraries(foo_obsbridge PRIVATE
  "${FOOBAR2000_SDK_DIR}/foobar2000/shared/shared-x64.lib")

target_compile_definitions(foo_obsbridge PRIVATE
  WIN32 _WINDOWS _WIN32 _WIN32_WINNT=0x0601
  FB2K_TARGET_VERSION=20000
  FB2K_TARGET_VERSION_COMPATIBLE=20000
  _CRT_SECURE_NO_WARNINGS)

target_compile_options(foo_obsbridge PRIVATE /W3 /WX- /wd4996)

set_target_properties(
  foo_obsbridge
  PROPERTIES OUTPUT_NAME "foo_obsbridge" PREFIX "" SUFFIX ".dll"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
             RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/out"
             RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
             RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/out"
             LIBRARY_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/out"
             LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
             LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/out")

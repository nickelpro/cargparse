cmake_minimum_required(VERSION 4.4)

include(vcpkg/BootstrapVcpkg.cmake)

project(cargparse
  LANGUAGES CXX
  VERSION 0.0.1
  SPDX_LICENSE CC0-1.0
)

if(NOT DEFINED BUILD_TESTING)
  set(BUILD_TESTING ${PROJECT_IS_TOP_LEVEL})
endif()

option(CARGPARSE_BUILD_TESTS
  "Build cargparse tests. Default: ${BUILD_TESTING}"
  ${BUILD_TESTING}
)
option(CARGPARSE_BUILD_EXAMPLE
  "Build the example. Default: OFF"
  OFF
)

add_library(cargparse)
set_target_properties(cargparse
  PROPERTIES
    SPDX_LICENSE CC0-1.0
    CXX_MODULE_STD ON
)
target_sources(cargparse
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      src/cargparse.cppm
      src/cargparse-hash.cppm

  PRIVATE
    FILE_SET SOURCES
    FILES
      src/cargparse.cpp
)
target_compile_features(cargparse
  PUBLIC
    cxx_std_26
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(cargparse PUBLIC -freflection)
endif()

if(CARGPARSE_BUILD_TESTS)
  find_package(Catch2 REQUIRED CONFIG)

  add_executable(cargparse_tests)
  set_target_properties(cargparse_tests
    PROPERTIES
      CXX_MODULE_STD ON
  )
  target_sources(cargparse_tests
    PRIVATE
      test/tests.cpp
  )
  target_link_libraries(cargparse_tests
    PRIVATE
      cargparse
      Catch2::Catch2WithMain
  )
  target_compile_features(cargparse_tests
    PRIVATE
      cxx_std_26
  )

  enable_testing()
  include(Catch)
  catch_discover_tests(cargparse_tests)
endif()

if(CARGPARSE_BUILD_EXAMPLE)
  add_executable(cargparse_example)
  set_target_properties(cargparse_example
    PROPERTIES
      CXX_MODULE_STD ON
  )
  target_sources(cargparse_example
    PRIVATE
      example/main.cpp
  )
  target_link_libraries(cargparse_example
    PRIVATE
      cargparse
  )
  target_compile_features(cargparse_example
    PRIVATE
      cxx_std_26
  )
endif()

include(GNUInstallDirs)

install(
  TARGETS cargparse
  EXPORT cargparse-targets
  FILE_SET CXX_MODULES
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cargparse
)
install(
  PACKAGE_INFO cargparse
  EXPORT cargparse-targets
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cps/cargparse
  CXX_MODULES_DIRECTORY cxxmodules
)

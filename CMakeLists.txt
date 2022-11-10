cmake_minimum_required(VERSION 3.16.3)
project(tftp)

# Custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR})
set(WARNINGS_FLAGS "-Wall -Wextra -pedantic -Wcast-qual -Wcast-align -Wduplicated-cond -Wshadow -Wmissing-noreturn")

set(LIB_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/protocol.cc
)

if (UNIX)
  set(OS_LIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/OS/Linux/Socket.cc
  )
endif()

add_library(tftp ${LIB_SOURCES} ${OS_LIB_SOURCES})
target_include_directories(tftp PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tftp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/tftp)
set_target_properties(tftp PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE ON
  COMPILE_FLAGS ${WARNINGS_FLAGS}
)

add_executable(server examples/server.cc)
target_link_libraries(server tftp)
set_target_properties(server PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE ON
  COMPILE_FLAGS ${WARNINGS_FLAGS}
)

add_executable(client examples/client.cc)
target_link_libraries(client tftp)
set_target_properties(client PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED YES
  CXX_EXTENSIONS NO
  POSITION_INDEPENDENT_CODE ON
  COMPILE_FLAGS ${WARNINGS_FLAGS}
)

#option(BUILD_UNIT_TESTS "Build unit tests" ON)
#if (BUILD_UNIT_TESTS)
#  find_package(GTest QUIET)
#  if (NOT GTest_FOUND)
#    message(STATUS "GTest not found: Unit tests will NOT be built")
#  endif()
#endif()

#if (GTest_FOUND)
#  add_executable(tftp_unit unit/protocol-t.cc
#  )
#
#  target_link_libraries(tftp_unit tftp GTest::gmock_main)
#  set_target_properties(tftp_unit PROPERTIES
#    CXX_STANDARD 17
#    CXX_STANDARD_REQUIRED YES
#    CXX_EXTENSIONS NO
#    POSITION_INDEPENDENT_CODE ON
#    COMPILE_FLAGS ${WARNINGS_FLAGS}
#  )
#  add_test(NAME tftp COMMAND tftp_unit WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
#
#  option(CODE_COVERAGE "Enable code coverage - gcovr shall be in the PATH" FALSE)
#  if (${CODE_COVERAGE})
#    include(CodeCoverage)
#    append_coverage_compiler_flags()
#    set(GCOVR_ADDITIONAL_ARGS "--exclude-unreachable-branches")
#    setup_target_for_coverage_gcovr_html(
#        NAME coverage
#        EXECUTABLE tftp_unit
#        EXCLUDE "unit/*" ".*gtest.*" "examples" ".*gmock.*"
#        )
#  endif()
#endif()

#add_subdirectory(examples)

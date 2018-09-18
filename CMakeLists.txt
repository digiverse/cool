project (cool)
cmake_minimum_required(VERSION 2.8)

## #######################################################################
#
# Input variables
#
#  COOL_HOME         top level source directory
#  COOL_LIBRARY_DIR  location of build library
#  COOL_LIBRARY_NAME the name of the library
#  COOL_DYNAMIC      if true builds dynamic library, static otherwise
#  COOL_TEST_DIR     location of built tests, if enabled
#  COOL_BUILD_TEST   build test programs (true/false)
#  COOL_BUILD_DOC    build HTML documentation, requires doxygen (true/false)
#  COOL_DOC_DIR      location of generated documentation, if built
#
# Output variables:
#  COOL_LIBS         cool library and dependent libraries
#  COOL_INCLUDE_DIR  include path for cool header files
#  COOL_VERSION      version number
#  COOL_SOVERSION    SOVERSION of the dynamic library (for Unixy platforms)
#
## #######################################################################

# version number and SOVERSION
set (COOL_VERSION "0.1.0")
set (COOL_SOVERSION "1")

# defaults if input variables aren't set

if (NOT DEFINED COOL_HOME)
  set (COOL_HOME ${CMAKE_CURRENT_LIST_DIR})
endif()
if (NOT DEFINED COOL_LIBRARY_DIR)
  if (WIN32)
    set (COOL_LIBRARY_DIR ${CMAKE_BINARY_DIR}/bin)
  else()
    set (COOL_LIBRARY_DIR ${CMAKE_BINARY_DIR}/lib)
  endif()
endif()

if (NOT DEFINED COOL_DYNAMIC)
  set (COOL_DYNAMIC true)
endif()
if (NOT DEFINED COOL_TEST_DIR)
  if (WIN32)
    set (COOL_TEST_DIR ${CMAKE_BINARY_DIR}/bin)
  else()
    set (COOL_TEST_DIR ${CMAKE_BINARY_DIR}/test)
  endif()
endif()
if (NOT DEFINED COOL_LIBRARY_NAME)
  set (COOL_LIBRARY_NAME cool)
endif()
if (NOT DEFINED COOL_BUILD_TEST)
  set (COOL_BUILD_TEST true)
endif()
if (NOT DEFINED COOL_BUILD_DOC)
  set (COOL_BUILD_DOC true)
endif()
if (NOT DEFINED COOL_DOC_DIR)
  set (COOL_DOC_DIR ${CMAKE_BINARY_DIR}/doc)
endif()

if (COOL_HAVE_GTEST)
  if (NOT DEFINED COOL_GTEST_LIB)
    message (FATAL_ERROR "COOL_GTEST_LIB must be set if COOL_HAVE_GTEST is true")
  endif()
else()
  set (COOL_GTEST_LIB gtest)
endif()

# platform detection
set (OSX false)
set (LINUX false)
set (WIN32 false)
if (${CMAKE_SYSTEM_NAME} MATCHES  "Darwin")
  message("   Build COOL library on Apple OS/X")
  set (OSX true)
  set (TARGET APPLE_TARGET)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  message("   Build COOL library on Linux")
  set (TARGET LINUX_TARGET)
  set (LINUX true)
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
  message("   Build COOL library on Microsoft Windows from ${COOL_HOME}")
  set (TARGET WIN32_TARGET)
  set (WIN32 true)
  set (xdispatch_DIR externals/xdispatch)
else()
  message("*********************** Unknown platform, expect errors")
  error("Told you so")
endif()

if (COOL_DYNAMIC)
  message ("-- Library format: dynamic library")
else()
  message ("-- Library format: static library")
endif()
message ("-- Build test programs: ${COOL_BUILD_TEST}")
message ("-- Build documentation: ${COOL_BUILD_DOC}")


# set output variables
if (OSX)
  set (COOL_LINK_LIBRARIES)
elseif(LINUX)
  set (COOL_LINK_LIBRARIES dispatch pthread)
else()
  set (COOL_LINK_LIBRARIES dispatch)
endif()
set (COOL_LIBS ${COOL_LIBRARY_NAME} ${COOL_LINK_LIBRARIES})

get_directory_property(hasParent PARENT_DIRECTORY)
if (hasParent)
set (COOL_INCLUDE ${COOL_HOME}/include PARENT_SCOPE)
set (COOL_LIBS ${COOL_LIBS} PARENT_SCOPE)
set (COOL_VERSION ${COOL_VERSION} PARENT_SCOPE)
set (COOL_SOVERSION ${COOL_SOVERSION} PARENT_SCOPE)
endif()

add_definitions(-D${TARGET})

#include_directories(${COOL_HOME}/include /usr/local/include)
#if (WIN32)
#  include_directories(${xdispatch_DIR}/libdispatch/include)
#endif()

if (OSX)
  set(DISABLED_WARNINGS -Wno-undefined-bool-conversion)
endif()

if ( NOT COOL_DO_NOT_SET_COMPILER_FLAGS )

  if (WIN32)
    set (CMAKE_CXX_FLAGS "/EHsc")
  else()
    set (CMAKE_CXX_FLAGS "-std=c++11 -g ${DISABLED_WARNINGS}")
  endif()

endif()

if (WIN32)
  add_definitions(-DDISPATCH_DEBUG=0)
  add_subdirectory(externals/xdispatch)
endif()

set (COOL_SRCS
     ${COOL_HOME}/lib/miscellaneous.cpp
     ${COOL_HOME}/lib/vow.cpp
     ${COOL_HOME}/lib/gcd_task.cpp
     ${COOL_HOME}/lib/gcd_async.cpp
)
if (OSX OR LINUX)
  set (COOL_SRCS ${COOL_SRCS} ${COOL_HOME}/lib/ip_address.cpp )
endif()

set (COOL_HEADERS
    ${COOL_HOME}/include/cool/cool.h
    ${COOL_HOME}/include/cool/exception.h
    ${COOL_HOME}/include/cool/miscellaneous.h
    ${COOL_HOME}/include/cool/vow.h
    ${COOL_HOME}/include/cool/gcd_task.h
    ${COOL_HOME}/include/cool/gcd_async.h
    ${COOL_HOME}/include/cool/binary.h
)
if (OSX OR LINUX)
  set (COOL_HEADERS ${COOL_HEADERS} ${COOL_HOME}/include/cool/ip_address.h)
endif()

set (COOL_IMPL_HEADERS
    ${COOL_HOME}/include/cool/entrails/vow.h
    ${COOL_HOME}/include/cool/entrails/gcd_task.h
    ${COOL_HOME}/include/cool/entrails/gcd_async.h
    ${COOL_HOME}/include/cool/entrails/platform.h
)
if (OSX OR LINUX)
  set (COOL_IMPL_HEADERS ${COOL_IMPL_HEADERS} ${COOL_HOME}/include/cool/entrails/ip_address.h)
endif()

source_group("Api Header Files" FILES ${COOL_HEADERS})
source_group("Impl Header Files" FILES ${COOL_IMPL_HEADERS})
source_group("Impl Source Files" FILES ${COOL_SRCS})

if (COOL_DYNAMIC)
  add_library(${COOL_LIBRARY_NAME} SHARED ${COOL_SRCS} ${COOL_HEADERS} ${COOL_IMPL_HEADERS})
  set_target_properties( ${COOL_LIBRARY_NAME} PROPERTIES VERSION ${COOL_VERSION} SOVERSION ${COOL_SOVERSION} )
  if (WIN32)
    set_target_properties(${COOL_LIBRARY_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${COOL_LIBRARY_DIR})
    set_target_properties(${COOL_LIBRARY_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${COOL_LIBRARY_DIR})
  else()
    set_target_properties(${COOL_LIBRARY_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${COOL_LIBRARY_DIR})
  endif()
else()
  add_library(${COOL_LIBRARY_NAME} STATIC ${COOL_SRCS} ${COOL_HEADERS} ${COOL_IMPL_HEADERS})
  set_target_properties(${COOL_LIBRARY_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${COOL_LIBRARY_DIR})
endif()

target_link_libraries(${COOL_LIBRARY_NAME} ${COOL_LINK_LIBRARIES})
target_include_directories(${COOL_LIBRARY_NAME} PUBLIC ${COOL_HOME}/include)
if (WIN32)
  target_include_directories(${COOL_LIBRARY_NAME} PUBLIC ${xdispatch_DIR}/libdispatch/include)
endif()

# --- Tests
set (TESTS
   runner
   on_exception
)

if( NOT WIN32 )
  set (TESTS ${TESTS}
    reader_writer
  )
endif()

# --- Doxygen documentation

if (COOL_BUILD_DOC)
  find_package(Doxygen)
  if (NOT Doxygen_FOUND)
    message(WARNING "Doxygen package not found, will not generate documentation")
  else ()
    make_directory (${COOL_DOC_DIR})
    configure_file(
      ${COOL_HOME}/cool.dox.in
      ${COOL_DOC_DIR}/cool.dox
      @ONLY
    )
    add_custom_command(
      OUTPUT ${COOL_DOC_DIR}/doc/html/index.html
      COMMAND ${DOXYGEN_EXECUTABLE} ${COOL_DOC_DIR}/cool.dox
      DEPENDS ${COOL_HEADERS}
      WORKING_DIRECTORY ${COOL_DOC_DIR}
    )
    add_custom_target(doc DEPENDS ${COOL_DOC_DIR}/doc/html/index.html)
    add_dependencies(${COOL_LIBRARY_NAME} doc)
  endif()
endif()

# --- Google test

if (COOL_BUILD_TEST)
##########################################################################
# Google Test library
#
# Output variables:
#     GOOGLETEST_HOME            Google Test install directory
#     GOOGLETEST_INCLUDE_DIR     Self explanatory
#     GOOGLETEST_LIBRARIES       Google Test runtime libraries
#     GOOGLETEST_LIBRARIES_ALL   Google Test runtime libraries with main()

  set (GOOGLETEST_HOME ${CMAKE_CURRENT_BINARY_DIR}/gtest-1.7.0)
  set (GOOGLETEST_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/externals/gtest-1.7.0/include)

  add_subdirectory(externals/gtest-1.7.0)

  set (GOOGLETEST_LIBRARIES gtest)
  set (GOOGLETEST_LIBRARIES_ALL gtest gtest_main)

  include_directories(${GOOGLETEST_INCLUDE_DIR})
  foreach (test ${TESTS})
    add_executable (test_${test} ${COOL_HOME}/test/test_${test}.cpp)
    target_link_libraries(test_${test} ${COOL_LIBS} ${GOOGLETEST_LIBRARIES_ALL})
    set_target_properties(test_${test} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${COOL_TEST_DIR})
  endforeach()


endif()

cmake_minimum_required(VERSION 2.8.6)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(CheckSymbolExists)
include(CheckCXXSymbolExists)
include(${CMAKE_CURRENT_LIST_DIR}/ClangVersion.cmake)

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions(-DRTAGS_DEBUG)
endif()

ClangVersion("${PROJECT_SOURCE_DIR}/3rdparty/clang_install" CLANG_VERSION)
message("Using Clang version ${CLANG_VERSION}")

add_definitions(-Wall)
add_definitions("-DCLANG_INCLUDEPATH=\"${PROJECT_SOURCE_DIR}/3rdparty/clang_install/lib/clang/${CLANG_VERSION}/include/\"")
add_definitions("-DCLANG_BIN=\"${PROJECT_SOURCE_DIR}/3rdparty/clang_install/bin/\"")
add_definitions("-DMAKE=\"${PROJECT_SOURCE_DIR}/3rdparty/make_install/bin/make\"")
add_definitions(-DOS_${CMAKE_SYSTEM_NAME})

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(system_LIBS rt)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
#  add_definitions("-DMAKE=\"gmake\"")
  add_definitions(-D__LONG_LONG_SUPPORTED)
  set(system_LIBS pthread)
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  find_program(SW_VER sw_vers)
  execute_process(COMMAND "${SW_VER}" -productVersion OUTPUT_VARIABLE osver)
  string(REPLACE "\n" "" osver "${osver}")
  if(${osver} VERSION_LESS "10.7")
    message("OS X ${osver} is too old for FSEvents, falling back to kqueue")
  # else()
  #   set(HAVE_FSEVENTS 1)
  endif()
  find_library(CORESERVICES_LIBRARY CoreServices)
  find_path(CORESERVICES_INCLUDE "CoreServices/CoreServices.h")
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_path(COREFOUNDATION_INCLUDE "CoreFoundation/CoreFoundation.h")
endif()

check_cxx_symbol_exists(backtrace "execinfo.h" HAVE_BACKTRACE)
check_cxx_symbol_exists(CLOCK_MONOTONIC_RAW "time.h" HAVE_CLOCK_MONOTONIC_RAW)
check_cxx_symbol_exists(CLOCK_MONOTONIC "time.h" HAVE_CLOCK_MONOTONIC)
check_cxx_symbol_exists(mach_absolute_time "mach/mach.h;mach/mach_time.h" HAVE_MACH_ABSOLUTE_TIME)
check_cxx_symbol_exists(inotify_init "sys/inotify.h" HAVE_INOTIFY)
check_cxx_symbol_exists(kqueue "sys/types.h;sys/event.h" HAVE_KQUEUE)
check_cxx_symbol_exists(SO_NOSIGPIPE "sys/types.h;sys/socket.h" HAVE_NOSIGPIPE)
check_cxx_symbol_exists(MSG_NOSIGNAL "sys/types.h;sys/socket.h" HAVE_NOSIGNAL)
check_cxx_symbol_exists(SA_SIGINFO "signal.h" HAVE_SIGINFO)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

include(rtags.cmake)

include_directories(${CMAKE_CURRENT_LIST_DIR} ${CORESERVICES_INCLUDE} ${COREFOUNDATION_INCLUDE})
add_executable(rdm rdm.cpp)
target_link_libraries(rdm rtags ${clang_LIBS} ${system_LIBS} ${CORESERVICES_LIBRARY} ${COREFOUNDATION_LIBRARY})
add_dependencies(rdm rtags)

include_directories(${CMAKE_CURRENT_LIST_DIR} ${CORESERVICES_INCLUDE} ${COREFOUNDATION_INCLUDE})
add_executable(clangtest clangtest.cpp)
target_link_libraries(clangtest rtags ${clang_LIBS} ${system_LIBS} ${CORESERVICES_LIBRARY} ${COREFOUNDATION_LIBRARY})
add_dependencies(clangtest rtags)

add_executable(rc rc.cpp ${rtags_client_SRCS})
target_link_libraries(rc ${system_LIBS})
add_dependencies(rc rtags)

add_executable(gr ${grtags_SRCS})
include_directories(${PROJECT_SOURCE_DIR}
  ${CORESERVICES_INCLUDE}
  ${COREFOUNDATION_INCLUDE}
  ${PROJECT_SOURCE_DIR}/3rdparty/leveldb/include)
target_link_libraries(gr
  ${clang_LIBS}
  ${system_LIBS}
  ${CORESERVICES_LIBRARY}
  ${COREFOUNDATION_LIBRARY}
  ${PROJECT_SOURCE_DIR}/3rdparty/leveldb/libleveldb.a)

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
  add_custom_command(TARGET rdm POST_BUILD
                     COMMAND ${CMAKE_CURRENT_LIST_DIR}/fix-dylib.sh ${PROJECT_SOURCE_DIR} rdm)
  add_custom_command(TARGET rc POST_BUILD
                     COMMAND ${CMAKE_CURRENT_LIST_DIR}/fix-dylib.sh ${PROJECT_SOURCE_DIR} rc)
  add_custom_command(TARGET gr POST_BUILD
                     COMMAND ${CMAKE_CURRENT_LIST_DIR}/fix-dylib.sh ${PROJECT_SOURCE_DIR} gr)
endif()

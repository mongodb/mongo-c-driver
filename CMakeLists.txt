cmake_minimum_required(VERSION 2.8)

project (libmongoc)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/build/cmake)

include(InstallRequiredSystemLibraries)
include(FindOpenSSL)

include(FindBSON REQUIRED)

set (SOURCE_DIR "${PROJECT_SOURCE_DIR}/")

set (MONGOC_MAJOR_VERSION 1)
set (MONGOC_MINOR_VERSION 1)
set (MONGOC_MICRO_VERSION 1)
set (MONGOC_API_VERSION 1.0)
set (MONGOC_VERSION 1.1.1)

set (CPACK_RESOURCE_FILE_LICENSE "${SOURCE_DIR}/COPYING")
set (CPACK_PACKAGE_VERSION_MAJOR ${MONGOC_MAJOR_VERSION})
set (CPACK_PACKAGE_VERSION_MINOR ${MONGOC_MINOR_VERSION})

include (CPack)

if (OPENSSL_FOUND)
   set (MONGOC_ENABLE_SSL 1)
else()
   set (MONGOC_ENABLE_SSL 0)
endif ()

set (MONGOC_ENABLE_SASL 0)

configure_file (
   "${SOURCE_DIR}/src/mongoc/mongoc-config.h.in"
   "${PROJECT_BINARY_DIR}/src/mongoc/mongoc-config.h"
)

configure_file (
   "${SOURCE_DIR}/src/mongoc/mongoc-version.h.in"
   "${PROJECT_BINARY_DIR}/src/mongoc/mongoc-version.h"
)

include_directories("${PROJECT_BINARY_DIR}/src/mongoc")
include_directories("${SOURCE_DIR}/src/mongoc")
include_directories(${BSON_INCLUDE_DIR})

set(LIBS ${LIBS} ${BSON_LIBRARIES})

if (UNIX AND NOT APPLE)
   set(LIBS ${LIBS} rt)
endif()

add_definitions(-D_GNU_SOURCE)
add_definitions(-D_BSD_SOURCE)
add_definitions("-DBINARY_DIR=\"${SOURCE_DIR}/tests/binary\"")

set (SOURCES
   ${SOURCE_DIR}/src/mongoc/mongoc-array.c
   ${SOURCE_DIR}/src/mongoc/mongoc-bulk-operation.c
   ${SOURCE_DIR}/src/mongoc/mongoc-buffer.c
   ${SOURCE_DIR}/src/mongoc/mongoc-b64.c
   ${SOURCE_DIR}/src/mongoc/mongoc-client.c
   ${SOURCE_DIR}/src/mongoc/mongoc-client-pool.c
   ${SOURCE_DIR}/src/mongoc/mongoc-cluster.c
   ${SOURCE_DIR}/src/mongoc/mongoc-collection.c
   ${SOURCE_DIR}/src/mongoc/mongoc-counters.c
   ${SOURCE_DIR}/src/mongoc/mongoc-cursor.c
   ${SOURCE_DIR}/src/mongoc/mongoc-cursor-array.c
   ${SOURCE_DIR}/src/mongoc/mongoc-cursor-cursorid.c
   ${SOURCE_DIR}/src/mongoc/mongoc-cursor-transform.c
   ${SOURCE_DIR}/src/mongoc/mongoc-database.c
   ${SOURCE_DIR}/src/mongoc/mongoc-init.c
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs.c
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs-file.c
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs-file-page.c
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs-file-list.c
   ${SOURCE_DIR}/src/mongoc/mongoc-index.c
   ${SOURCE_DIR}/src/mongoc/mongoc-list.c
   ${SOURCE_DIR}/src/mongoc/mongoc-log.c
   ${SOURCE_DIR}/src/mongoc/mongoc-matcher.c
   ${SOURCE_DIR}/src/mongoc/mongoc-matcher-op.c
   ${SOURCE_DIR}/src/mongoc/mongoc-queue.c
   ${SOURCE_DIR}/src/mongoc/mongoc-read-prefs.c
   ${SOURCE_DIR}/src/mongoc/mongoc-rpc.c
   ${SOURCE_DIR}/src/mongoc/mongoc-socket.c
   ${SOURCE_DIR}/src/mongoc/mongoc-stream.c
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-buffered.c
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-file.c
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-gridfs.c
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-socket.c
   ${SOURCE_DIR}/src/mongoc/mongoc-uri.c
   ${SOURCE_DIR}/src/mongoc/mongoc-util.c
   ${SOURCE_DIR}/src/mongoc/mongoc-write-command.c
   ${SOURCE_DIR}/src/mongoc/mongoc-write-concern.c
)

set (HEADERS
   ${PROJECT_BINARY_DIR}/src/mongoc/mongoc-config.h
   ${PROJECT_BINARY_DIR}/src/mongoc/mongoc-version.h
   ${SOURCE_DIR}/src/mongoc/mongoc.h
   ${SOURCE_DIR}/src/mongoc/mongoc-bulk-operation.h
   ${SOURCE_DIR}/src/mongoc/mongoc-client.h
   ${SOURCE_DIR}/src/mongoc/mongoc-client-pool.h
   ${SOURCE_DIR}/src/mongoc/mongoc-collection.h
   ${SOURCE_DIR}/src/mongoc/mongoc-cursor.h
   ${SOURCE_DIR}/src/mongoc/mongoc-database.h
   ${SOURCE_DIR}/src/mongoc/mongoc-error.h
   ${SOURCE_DIR}/src/mongoc/mongoc-flags.h
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs.h
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs-file.h
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs-file-page.h
   ${SOURCE_DIR}/src/mongoc/mongoc-gridfs-file-list.h
   ${SOURCE_DIR}/src/mongoc/mongoc-host-list.h
   ${SOURCE_DIR}/src/mongoc/mongoc-init.h
   ${SOURCE_DIR}/src/mongoc/mongoc-index.h
   ${SOURCE_DIR}/src/mongoc/mongoc-iovec.h
   ${SOURCE_DIR}/src/mongoc/mongoc-log.h
   ${SOURCE_DIR}/src/mongoc/mongoc-matcher.h
   ${SOURCE_DIR}/src/mongoc/mongoc-opcode.h
   ${SOURCE_DIR}/src/mongoc/mongoc-read-prefs.h
   ${SOURCE_DIR}/src/mongoc/mongoc-socket.h
   ${SOURCE_DIR}/src/mongoc/mongoc-stream.h
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-buffered.h
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-file.h
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-gridfs.h
   ${SOURCE_DIR}/src/mongoc/mongoc-stream-socket.h
   ${SOURCE_DIR}/src/mongoc/mongoc-uri.h
   ${SOURCE_DIR}/src/mongoc/mongoc-write-concern.h
)

if (OPENSSL_FOUND)
   set (HEADERS ${HEADERS}
      ${SOURCE_DIR}/src/mongoc/mongoc-rand.h
      ${SOURCE_DIR}/src/mongoc/mongoc-stream-tls.h
      ${SOURCE_DIR}/src/mongoc/mongoc-ssl.h)
   set (SOURCES ${SOURCES}
      ${SOURCE_DIR}/src/mongoc/mongoc-rand.c
      ${SOURCE_DIR}/src/mongoc/mongoc-scram.c
      ${SOURCE_DIR}/src/mongoc/mongoc-stream-tls.c
      ${SOURCE_DIR}/src/mongoc/mongoc-ssl.c
   )
   set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
   include_directories(${OPENSSL_INCLUDE_DIR})
endif()

if (MSVC)
   if (OPENSSL_FOUND)
      set(MONGOC_SHARED_SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/build/cmake/libmongoc-ssl.def)
   else ()
      set(MONGOC_SHARED_SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/build/cmake/libmongoc.def)
   endif()
else()
   set(MONGOC_SHARED_SOURCES ${SOURCES})
endif()

add_library(mongoc_shared SHARED ${MONGOC_SHARED_SOURCES} ${HEADERS})
add_library(mongoc_static STATIC ${SOURCES} ${HEADERS})

target_link_libraries(mongoc_shared ${LIBS})
target_link_libraries(mongoc_static ${LIBS})

set_target_properties(mongoc_shared PROPERTIES COMPILE_DEFINITIONS "MONGOC_COMPILATION")
set_target_properties(mongoc_static PROPERTIES COMPILE_DEFINITIONS "MONGOC_COMPILATION")

set_target_properties(mongoc_shared PROPERTIES VERSION ${MONGOC_VERSION} SOVERSION ${MONGOC_MAJOR_VERSION})
set_target_properties(mongoc_static PROPERTIES VERSION ${MONGOC_VERSION} SOVERSION ${MONGOC_MAJOR_VERSION})

set_target_properties(mongoc_shared PROPERTIES OUTPUT_NAME "mongoc-${MONGOC_API_VERSION}" PREFIX "lib")
set_target_properties(mongoc_static PROPERTIES OUTPUT_NAME "mongoc-static-${MONGOC_API_VERSION}")
enable_testing()

function(mongoc_add_test test use_shared)
   add_executable(${test} ${ARGN})
   if (${use_shared})
      target_link_libraries(${test} mongoc_shared)
   else()
      set_target_properties(${test} PROPERTIES COMPILE_DEFINITIONS "MONGOC_COMPILATION")
      target_link_libraries(${test} mongoc_static)
   endif()
   if(WIN32)
      target_link_libraries(${test} shlwapi)
   endif()
endfunction()

function(mongoc_add_example example use_shared)
   add_executable(${example} ${ARGN})
   if (${use_shared})
      target_link_libraries(${example} mongoc_shared)
   else()
      set_target_properties(${example} PROPERTIES COMPILE_DEFINITIONS "MONGOC_COMPILATION")
      target_link_libraries(${example} mongoc_static)
   endif()
   if(WIN32)
      target_link_libraries(${example} shlwapi)
   endif()
   set (EXAMPLES ${EXAMPLES} ${example})
endfunction()

set(test-libmongoc-sources
   ${SOURCE_DIR}/tests/ha-test.c
   ${SOURCE_DIR}/tests/test-libmongoc.c
   ${SOURCE_DIR}/tests/test-mongoc-array.c
   ${SOURCE_DIR}/tests/test-mongoc-buffer.c
   ${SOURCE_DIR}/tests/test-mongoc-client.c
   ${SOURCE_DIR}/tests/mock-server.c
   ${SOURCE_DIR}/tests/test-bulk.c
   ${SOURCE_DIR}/tests/test-mongoc-client-pool.c
   ${SOURCE_DIR}/tests/test-mongoc-collection.c
   ${SOURCE_DIR}/tests/test-mongoc-cursor.c
   ${SOURCE_DIR}/tests/test-mongoc-database.c
   ${SOURCE_DIR}/tests/test-mongoc-gridfs.c
   ${SOURCE_DIR}/tests/test-mongoc-gridfs-file-page.c
   ${SOURCE_DIR}/tests/test-mongoc-list.c
   ${SOURCE_DIR}/tests/test-mongoc-matcher.c
   ${SOURCE_DIR}/tests/test-mongoc-queue.c
   ${SOURCE_DIR}/tests/test-mongoc-read-prefs.c
   ${SOURCE_DIR}/tests/test-mongoc-rpc.c
   ${SOURCE_DIR}/tests/test-mongoc-socket.c
   ${SOURCE_DIR}/tests/test-mongoc-stream.c
   ${SOURCE_DIR}/tests/test-mongoc-uri.c
   ${SOURCE_DIR}/tests/test-mongoc-write-concern.c
   ${SOURCE_DIR}/tests/test-write-commands.c
   ${SOURCE_DIR}/tests/TestSuite.c
)

if (OPENSSL_FOUND)
   set(test-libmongoc-sources ${test-libmongoc-sources}
      ${SOURCE_DIR}/tests/test-x509.c
      ${SOURCE_DIR}/tests/ssl-test.c
      ${SOURCE_DIR}/tests/test-mongoc-stream-tls.c)
   mongoc_add_test(test-replica-set-ssl FALSE
      ${SOURCE_DIR}/tests/test-replica-set-ssl.c
      ${SOURCE_DIR}/tests/ha-test.c
      ${SOURCE_DIR}/tests/mongoc-tests.c
   )
endif()

mongoc_add_test(test-load FALSE
   ${SOURCE_DIR}/tests/test-load.c
   ${SOURCE_DIR}/tests/mongoc-tests.c)
mongoc_add_test(test-secondary FALSE
   ${SOURCE_DIR}/tests/test-secondary.c
   ${SOURCE_DIR}/tests/mongoc-tests.c)
mongoc_add_test(test-replica-set FALSE
   ${SOURCE_DIR}/tests/test-replica-set.c
   ${SOURCE_DIR}/tests/mongoc-tests.c
   ${SOURCE_DIR}/tests/ha-test.c)
mongoc_add_test(test-sharded-cluster FALSE
   ${SOURCE_DIR}/tests/test-sharded-cluster.c
   ${SOURCE_DIR}/tests/mongoc-tests.c
   ${SOURCE_DIR}/tests/ha-test.c)

mongoc_add_test(test-libmongoc FALSE ${test-libmongoc-sources})
add_test(NAME test-libmongoc COMMAND test-libmongoc -f -p)

mongoc_add_example(example-gridfs TRUE ${SOURCE_DIR}/examples/example-gridfs.c)
mongoc_add_example(example-client TRUE ${SOURCE_DIR}/examples/example-client.c)
mongoc_add_example(example-scram TRUE ${SOURCE_DIR}/examples/example-scram.c)
mongoc_add_example(mongoc-dump TRUE ${SOURCE_DIR}/examples/mongoc-dump.c)
mongoc_add_example(mongoc-ping TRUE ${SOURCE_DIR}/examples/mongoc-ping.c)
mongoc_add_example(mongoc-rpc-validate FALSE ${SOURCE_DIR}/examples/mongoc-rpc-validate.c)
mongoc_add_example(mongoc-tail TRUE ${SOURCE_DIR}/examples/mongoc-tail.c)

file(COPY ${SOURCE_DIR}/tests/binary DESTINATION ${PROJECT_BINARY_DIR}/tests)
file(COPY ${SOURCE_DIR}/tests/certificates DESTINATION ${PROJECT_BINARY_DIR}/tests)

#if (OPENSSL_FOUND)
#   file(COPY ${SOURCE_DIR}/tests/trust_dir DESTINATION ${PROJECT_BINARY_DIR}/tests)
#endif()

install(
  TARGETS mongoc_shared mongoc_static ${EXAMPLES}
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)
install(
  FILES ${HEADERS}
  DESTINATION "include/libmongoc-${MONGOC_API_VERSION}"
)

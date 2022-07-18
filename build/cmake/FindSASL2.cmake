include (CheckSymbolExists)
include (CMakePushCheckState)

message (STATUS "Searching for sasl/sasl.h")
find_path (
    SASL_INCLUDE_DIRS NAMES sasl/sasl.h
    PATHS /include /usr/include /usr/local/include /usr/share/include /opt/include c:/sasl/include
    DOC "Searching for sasl/sasl.h")

if (SASL_INCLUDE_DIRS)
    message (STATUS "  Found in ${SASL_INCLUDE_DIRS}")
else ()
    message (STATUS "  Not found (specify -DCMAKE_INCLUDE_PATH=/path/to/sasl/include for SASL support)")
endif ()

message (STATUS "Searching for libsasl2")
find_library (
    SASL_LIBRARIES NAMES sasl2
    PATHS /usr/lib /lib /usr/local/lib /usr/share/lib /opt/lib /opt/share/lib /var/lib c:/sasl/lib
    DOC "Searching for libsasl2")

if (SASL_LIBRARIES)
    message (STATUS "  Found ${SASL_LIBRARIES}")
else ()
    message (STATUS "  Not found (specify -DCMAKE_LIBRARY_PATH=/path/to/sasl/lib for SASL support)")
endif ()

if (SASL_INCLUDE_DIRS AND SASL_LIBRARIES)
    set (SASL_FOUND 1)

    cmake_push_check_state ()
    list (APPEND CMAKE_REQUIRED_INCLUDES ${SASL_INCLUDE_DIRS})
    list (APPEND CMAKE_REQUIRED_LIBRARIES ${SASL_LIBRARIES})
    check_symbol_exists (
        sasl_client_done
        sasl/sasl.h
        HAVE_SASL_CLIENT_DONE)
    cmake_pop_check_state()

    if (HAVE_SASL_CLIENT_DONE)
        set (MONGOC_HAVE_SASL_CLIENT_DONE 1)
    else ()
        set (MONGOC_HAVE_SASL_CLIENT_DONE 0)
    endif ()
else ()
    if (ENABLE_SASL STREQUAL AUTO)
        set (SASL_FOUND 0)
        set (SASL_INCLUDE_DIRS "")
        set (SASL_LIBRARIES "")
        set (MONGOC_HAVE_SASL_CLIENT_DONE 0)
    else ()
        message (FATAL_ERROR "  SASL not found")
    endif ()
endif ()

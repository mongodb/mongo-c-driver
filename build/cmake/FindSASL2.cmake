message ("Searching for sasl/sasl.h")
find_path (
    SASL2_INCLUDE_DIR NAMES sasl/sasl.h
    PATHS /include /usr/include /usr/local/include /usr/share/include /opt/include
    DOC "Searching for sasl/sasl.h")

if (SASL2_INCLUDE_DIR)
    message (STATUS "  Found in ${SASL2_INCLUDE_DIR}")
endif ()

message ("Searching for libsasl2")
find_library(
    SASL2_LIBRARY NAMES sasl2
    PATHS /usr/lib /lib /usr/local/lib /usr/share/lib /opt/lib /opt/share/lib /var/lib
    DOC "Searching for libsasl2")

if (SASL2_LIBRARY)
    message (STATUS "  Found ${SASL2_LIBRARY}")
endif ()

if (SASL2_INCLUDE_DIR AND SASL2_LIBRARY)
    set (SASL2_FOUND 1)

    check_symbol_exists (
        sasl_client_done
        ${SASL2_INCLUDE_DIR}/sasl/sasl.h
        MONGOC_HAVE_SASL_CLIENT_DONE)

    if (MONGOC_HAVE_SASL_CLIENT_DONE)
        set (MONGOC_HAVE_SASL_CLIENT_DONE 1)
    else ()
        set (MONGOC_HAVE_SASL_CLIENT_DONE 0)
    endif ()
else ()
    set (SASL2_FOUND 0)
    set (MONGOC_HAVE_SASL_CLIENT_DONE 0)
endif ()

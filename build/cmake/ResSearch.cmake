include(CheckSymbolExists)

# The name of the library that performs name resolution, suitable for giving to the "-l" link flag
set(RESOLVE_LIB_NAME)
# If TRUE, then the C runtime provides the name resolution that we need
set(resolve_is_libc FALSE)

if(WIN32)
    set(RESOLVE_LIB_NAME Dnsapi)
    set(_MONGOC_HAVE_DNSAPI 1)
else()
    # Thread-safe DNS query function for _mongoc_client_get_srv.
    # Could be a macro, not a function, so use check_symbol_exists.
    check_symbol_exists(res_nsearch resolv.h _MONGOC_HAVE_RES_NSEARCH)
    check_symbol_exists(res_ndestroy resolv.h _MONGOC_HAVE_RES_NDESTROY)
    check_symbol_exists(res_nclose resolv.h _MONGOC_HAVE_RES_NCLOSE)
    if(MONGOC_HAVE_RES_NSEARCH)
        # We have res_nsearch. Call res_ndestroy (BSD/Mac) or res_nclose (Linux)?
        set(RESOLVE_LIB_NAME resolv)
    elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        # On FreeBSD, the following line does not properly detect res_search,
        # which is included in libc on FreeBSD:
        # check_symbol_exists (res_search resolv.h MONGOC_HAVE_RES_SEARCH)
        #
        # Attempting to link with libresolv on FreeBSD will fail with this error:
        # ld: error: unable to find library -lresolv
        #
        # Since res_search has existed since 4.3 BSD (which is the predecessor
        # of FreeBSD), it is safe to assume that this function will exist in
        # libc on FreeBSD.
        set(_MONGOC_HAVE_RES_SEARCH 1)
        set(resolve_is_libc TRUE)
    else()
        # Thread-unsafe function.
        check_symbol_exists(res_search resolv.h _MONGOC_HAVE_RES_SEARCH)
        if(_MONGOC_HAVE_RES_SEARCH)
            set(RESOLVE_LIB_NAME resolv)
        endif()
    endif()
endif()

_mongo_pick(MONGOC_HAVE_DNSAPI 1 0 _MONGOC_HAVE_DNSAPI)
_mongo_pick(MONGOC_HAVE_RES_NSEARCH 1 0 _MONGOC_HAVE_RES_NSEARCH)
_mongo_pick(MONGOC_HAVE_RES_NDESTROY 1 0 _MONGOC_HAVE_RES_NDESTROY)
_mongo_pick(MONGOC_HAVE_RES_NCLOSE 1 0 _MONGOC_HAVE_RES_NCLOSE)
_mongo_pick(MONGOC_HAVE_RES_SEARCH 1 0 _MONGOC_HAVE_RES_SEARCH)

if(RESOLVE_LIB_NAME OR resolve_is_libc)
    # Define the resolver interface:
    add_library(_mongoc-resolve INTERFACE)
    add_library(mongo::detail::c_resolve ALIAS _mongoc-resolve)
    set_target_properties(_mongoc-resolve PROPERTIES
        INTERFACE_LINK_LIBRARIES "${RESOLVE_LIB_NAME}"
        EXPORT_NAME detail::c_resolve)
    install(TARGETS _mongoc-resolve EXPORT mongoc-targets)
endif()

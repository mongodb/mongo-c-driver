if (ENABLE_SRV STREQUAL ON OR ENABLE_SRV STREQUAL AUTO)
   if (WIN32)
      set (RESOLV_LIBS Dnsapi)
      set (MONGOC_HAVE_DNSAPI 1)
      set (MONGOC_HAVE_RES_NQUERY 0)
      set (MONGOC_HAVE_RES_NDESTROY 0)
      set (MONGOC_HAVE_RES_NCLOSE 0)
      set (MONGOC_HAVE_RES_QUERY 0)
   else ()
      set (MONGOC_HAVE_DNSAPI 0)
      # Thread-safe DNS query function for _mongoc_client_get_srv.
      # Could be a macro, not a function, so use check_symbol_exists.
      check_symbol_exists (res_nquery resolv.h MONGOC_HAVE_RES_NQUERY)
      if (MONGOC_HAVE_RES_NQUERY)
         set (RESOLV_LIBS resolv)
         set (MONGOC_HAVE_RES_QUERY 0)

         # We have res_nquery. Call res_ndestroy (BSD/Mac) or res_nclose (Linux)?
         check_symbol_exists (res_ndestroy resolv.h MONGOC_HAVE_RES_NDESTROY)
         if (MONGOC_HAVE_RES_NDESTROY)
            set (MONGOC_HAVE_RES_NCLOSE 0)
         else ()
            check_symbol_exists (res_nclose resolv.h MONGOC_HAVE_RES_NCLOSE)
         endif ()
      else ()
         set (MONGOC_HAVE_RES_NQUERY 0)
         set (MONGOC_HAVE_RES_NDESTROY 0)
         set (MONGOC_HAVE_RES_NCLOSE 0)

         # Thread-unsafe function.
         check_symbol_exists (res_query resolv.h MONGOC_HAVE_RES_QUERY)
         if (MONGOC_HAVE_RES_QUERY)
            set (RESOLV_LIBS resolv)
         endif()
      endif ()
   endif ()
endif ()

if (ENABLE_SRV STREQUAL ON AND NOT RESOLV_LIBS)
   message (
      FATAL_ERROR
      "Cannot find libresolv or dnsapi. Try setting ENABLE_SRV=OFF")
endif ()


<<<<<<< HEAD:src/common/common-config.h.in
#define MONGOC_ENABLE_TESTING 1
=======
#define MONGOC_ENABLE_TESTING 1
>>>>>>> CDRIVER-3702 added assertions:src/common/common-config.h

#if MONGOC_ENABLE_TESTING != 1
#  undef MONGOC_ENABLE_TESTING
#endif

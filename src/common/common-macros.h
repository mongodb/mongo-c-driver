#ifndef MONGO_C_DRIVER_COMMON_MACROS_H
#define MONGO_C_DRIVER_COMMON_MACROS_H

/*
 * configure with option -DENABLE_TESTING=ON
 */
#if defined(MONGOC_ENABLE_TESTING) && defined(BSON_OS_UNIX)
#define MONGOC_TEST_ASSERT(statement) BSON_ASSERT (statement)
#else
#define MONGOC_TEST_ASSERT(statement) ((void) 0)
#endif


#endif

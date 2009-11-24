/* mongo_except.h */

/* This file is based loosely on cexcept (http://www.nicemice.net/cexcept/). I
 * have modified it to work better with mongo's API.
 *
 * The MONGO_TRY, MONGO_CATCH, and MONGO_TROW macros assume that a pointer to
 * the current connection is available as 'conn'. If you would like to use a
 * different name, #define MONGO_CONNECTION_NAME at the top of your file. You
 * can use &conn if conn is the name of the connection rather than a pointer.
 */

#ifdef MONGO_CODE_EXAMPLE
    mongo_connection conn[1]; /* makes conn a ptr to the connection */

    MONGO_TRY{
        mongo_find_one(...);
        MONGO_THROW(conn, MONGO_EXCEPT_NETWORK);
    }MONGO_CATCH{
        switch(conn->exception->type){
            case MONGO_EXCEPT_NETWORK:
                do_something();
            case MONGO_EXCEPT_FIND_ERR:
                do_something();
            default:
                MONGO_RETHROW();
        }
    }
#endif

 /* ORIGINAL CEXEPT COPYRIGHT:
cexcept: README 2.0.1 (2008-Jul-23-Wed)
http://www.nicemice.net/cexcept/
Adam M. Costello
http://www.nicemice.net/amc/

The package is both free-as-in-speech and free-as-in-beer:

    Copyright (c) 2000-2008 Adam M. Costello and Cosmin Truta.
    This package may be modified only if its author and version
    information is updated accurately, and may be redistributed
    only if accompanied by this unaltered notice.  Subject to those
    restrictions, permission is granted to anyone to do anything with
    this package.  The copyright holders make no guarantees regarding
    this package, and are not responsible for any damage resulting from
    its use.
 */

#ifndef _MONGO_EXCEPT_H_
#define _MONGO_EXCEPT_H_

#include <setjmp.h>

/* always non-zero */
typedef enum{
    MONGO_EXCEPT_NETWORK=1,
    MONGO_EXCEPT_FIND_ERR
}mongo_exception_type;


typedef struct {
  jmp_buf base_handler;
  jmp_buf *penv;
  int caught;
  volatile mongo_exception_type type;
}mongo_exception_context;

#ifndef MONGO_CONNECTION_NAME
#define MONGO_CONNECTION_NAME conn
#endif

/* the rest of this file is implementation details */

/* this is done in mongo_connect */
#define MONGO_INIT_EXCEPTION(exception_ptr) \
    do{ \
        mongo_exception_type t; /* exception_ptr won't be available */\
        (exception_ptr)->penv = &(exception_ptr)->base_handler; \
        if ((t = setjmp((exception_ptr)->base_handler))) { /* yes, '=' is correct */ \
            switch(t){ \
                case MONGO_EXCEPT_NETWORK: bson_fatal_msg(0, "network error"); \
                case MONGO_EXCEPT_FIND_ERR: bson_fatal_msg(0, "error in find"); \
                default: bson_fatal_msg(0, "unknown exception"); \
            } \
        } \
    }while(0)

#define MONGO_TRY \
  { \
    jmp_buf *exception__prev, exception__env; \
    exception__prev = (MONGO_CONNECTION_NAME)->exception.penv; \
    (MONGO_CONNECTION_NAME)->exception.penv = &exception__env; \
    if (setjmp(exception__env) == 0) { \
      do

#define MONGO_CATCH \
      while ((MONGO_CONNECTION_NAME)->exception.caught = 0, \
             (MONGO_CONNECTION_NAME)->exception.caught); \
    } \
    else { \
      (MONGO_CONNECTION_NAME)->exception.caught = 1; \
    } \
    (MONGO_CONNECTION_NAME)->exception.penv = exception__prev; \
  } \
  if (!(MONGO_CONNECTION_NAME)->exception.caught ) { } \
  else

/* Try ends with do, and Catch begins with while(0) and ends with     */
/* else, to ensure that Try/Catch syntax is similar to if/else        */
/* syntax.                                                            */
/*                                                                    */
/* The 0 in while(0) is expressed as x=0,x in order to appease        */
/* compilers that warn about constant expressions inside while().     */
/* Most compilers should still recognize that the condition is always */
/* false and avoid generating code for it.                            */

#define MONGO_THROW(type_in) \
  for (;; longjmp(*(MONGO_CONNECTION_NAME)->exception.penv, type_in)) \
    (MONGO_CONNECTION_NAME)->exception.type = type_in

#define MONGO_RETHROW() MONGO_THROW((MONGO_CONNECTION_NAME)->exception.type)


#endif

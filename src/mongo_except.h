/* mongo_except.h */

/*    Copyright 2009 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/* This file is based loosely on cexcept (http://www.nicemice.net/cexcept/). I
 * have modified it to work better with mongo's API.
 *
 * The MONGO_TRY, MONGO_CATCH, and MONGO_TROW macros assume that a pointer to
 * the current connection is available as 'conn'. If you would like to use a
 * different name, use the _GENERIC version of these macros.
 *
 * WARNING: do not return or otherwise jump (excluding MONGO_TRHOW()) out of a
 * MONGO_TRY block as the nessesary clean-up code will not be called. Jumping
 * out of the MONGO_CATCH block is OK.
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

#define MONGO_TRY MONGO_TRY_GENERIC(conn)
#define MONGO_CATCH MONGO_CATCH_GENERIC(conn)
#define MONGO_THROW(e) MONGO_THROW_GENERIC(conn, e)
#define MONGO_RETHROW() MONGO_RETHROW_GENERIC(conn)

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

#define MONGO_TRY_GENERIC(connection) \
  { \
    jmp_buf *exception__prev, exception__env; \
    exception__prev = (connection)->exception.penv; \
    (connection)->exception.penv = &exception__env; \
    if (setjmp(exception__env) == 0) { \
      do

#define MONGO_CATCH_GENERIC(connection) \
      while ((connection)->exception.caught = 0, \
             (connection)->exception.caught); \
    } \
    else { \
      (connection)->exception.caught = 1; \
    } \
    (connection)->exception.penv = exception__prev; \
  } \
  if (!(connection)->exception.caught ) { } \
  else

/* Try ends with do, and Catch begins with while(0) and ends with     */
/* else, to ensure that Try/Catch syntax is similar to if/else        */
/* syntax.                                                            */
/*                                                                    */
/* The 0 in while(0) is expressed as x=0,x in order to appease        */
/* compilers that warn about constant expressions inside while().     */
/* Most compilers should still recognize that the condition is always */
/* false and avoid generating code for it.                            */

#define MONGO_THROW_GENERIC(connection, type_in) \
  for (;; longjmp(*(connection)->exception.penv, type_in)) \
    (connection)->exception.type = type_in

#define MONGO_RETHROW_GENERIC(connection) \
    MONGO_THROW_GENERIC(connection, (connection)->exception.type)


#endif

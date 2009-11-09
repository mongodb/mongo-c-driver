# -*- mode: python; -*-

import os

env = Environment()

if "darwin" == os.sys.platform or "linux2" == os.sys.platform:
    env.Append( CPPFLAGS=" -ansi -pedantic -Wall " )
    env.Append( CPPPATH=["/opt/local/include/"] )
    env.Append( LIBPATH=["/opt/local/lib/"] )

env.Append( CPPPATH=["src/"] )

coreFiles = []

m = env.Library( "mongoc" , coreFiles + [ "src/mongo.c"] )
b = env.Library( "bson" , coreFiles + [ "src/bson.c"] )

env.Default( env.Alias( "lib" , [ m[0] , b[0] ] ) )

testEnv = env.Clone()
testEnv.Append( LIBS=["json"] )
testEnv.Append( LIBS=["mongoc","bson"] )
testEnv.Prepend( LIBPATH=["."] )

testCoreFiles = [ "test/md5.c" ]

testEnv.Program( 'testsimple' , testCoreFiles + ["test/test.c"]  )
testEnv.Program( 'testjson' , testCoreFiles + ["test/json.c"]  )
testEnv.Program( 'testendian_swap' , testCoreFiles + ["test/endian_swap.c"]  )


# -*- mode: python; -*-

import os
import sys

env = Environment()

if "darwin" == os.sys.platform or "linux2" == os.sys.platform:
    env.Append( CPPFLAGS=" -ansi -pedantic -Wall -ggdb " )
    env.Append( CPPPATH=["/opt/local/include/"] )
    env.Append( LIBPATH=["/opt/local/lib/"] )

if sys.byteorder == 'big':
    env.Append( CPPFLAGS=" -DMONGO_BIG_ENDIAN " )

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

for name in Split('simple json endian_swap sizes'):
    filename = "test/%s.c" % name
    exe = "test" + name
    test = testEnv.Program( exe , testCoreFiles + [filename]  )
    test_alias = testEnv.Alias('test', [test], test[0].abspath + ' 2> /dev/null')
    AlwaysBuild(test_alias)

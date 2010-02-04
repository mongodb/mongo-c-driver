# -*- mode: python; -*-


# --- options ----
AddOption('--test-server',
          dest='test_server',
          default='127.0.0.1',
          type='string',
          nargs=1,
          action='store',
          help='IP address of server to use for testing')

AddOption('--c99',
          dest='use_c99',
          default=False,
          action='store_true',
          help='Compile with c99 (recommended for gcc)')

AddOption('--d',
          dest='optimize',
          default=True,
          action='store_false',
          help='disable optimizations')

import os
import sys

env = Environment( ENV=os.environ )

if "darwin" == os.sys.platform or "linux2" == os.sys.platform:
    env.Append( CPPFLAGS=" -pedantic -Wall -ggdb " )
    env.Append( CPPPATH=["/opt/local/include/"] )
    env.Append( LIBPATH=["/opt/local/lib/"] )

    if GetOption('use_c99'):
        env.Append( CFLAGS=" -std=c99 " )
        env.Append( CXXDEFINES="MONGO_HAVE_STDINT" )
    else:
        env.Append( CFLAGS=" -ansi " )

    if GetOption('optimize'):
        env.Append( CPPFLAGS=" -O3 " )
        # -O3 benchmarks *significantly* faster than -O2 when disabling networking
        

#we shouldn't need these options in c99 mode
if not GetOption('use_c99'):
    conf = Configure(env)

    if not conf.CheckType('int64_t'):
        if conf.CheckType('int64_t', '#include <stdint.h>\n'):
            conf.env.Append( CPPDEFINES="MONGO_HAVE_STDINT" )
        elif conf.CheckType('int64_t', '#include <unistd.h>\n'):
            conf.env.Append( CPPDEFINES="MONGO_HAVE_UNISTD" )
        elif conf.CheckType('__int64'):
            conf.env.Append( CPPDEFINES="MONGO_USE__INT64" )
        elif conf.CheckType('long long int'):
            conf.env.Append( CPPDEFINES="MONGO_USE_LONG_LONG_INT" )
        else:
            print "*** what is your 64 bit int type? ****"
            Exit(1)

    env = conf.Finish()

if sys.byteorder == 'big':
    env.Append( CPPDEFINES="MONGO_BIG_ENDIAN" )

env.Append( CPPPATH=["src/"] )

coreFiles = ["src/md5.c" ]

m = env.Library( "mongoc" , coreFiles + [ "src/mongo.c"] )
b = env.Library( "bson" , coreFiles + [ "src/bson.c", "src/numbers.c"] )

env.Default( env.Alias( "lib" , [ m[0] , b[0] ] ) )

benchmarkEnv = env.Clone()
benchmarkEnv.Append( CPPDEFINES=[('DTEST_SERVER', '"%s"'%GetOption('test_server'))] )
benchmarkEnv.Append( LIBS=[m, b] )
benchmarkEnv.Prepend( LIBPATH=["."] )
benchmarkEnv.Program( "benchmark" ,  [ "test/benchmark.c"] )

testEnv = benchmarkEnv.Clone()
testEnv.Append( LIBS=["json"] )

testCoreFiles = [ ]

for name in Split('sizes resize endian_swap all_types json simple update errors count_delete auth pair'):
    filename = "test/%s.c" % name
    exe = "test_" + name
    test = testEnv.Program( exe , testCoreFiles + [filename]  )
    test_alias = testEnv.Alias('test', [test], test[0].abspath + ' 2> /dev/null')
    AlwaysBuild(test_alias)

# special case for cpptest
test = testEnv.Program( 'test_cpp' , testCoreFiles + ['test/cpptest.cpp']  )
test_alias = testEnv.Alias('test', [test], test[0].abspath + ' 2> /dev/null')
AlwaysBuild(test_alias)

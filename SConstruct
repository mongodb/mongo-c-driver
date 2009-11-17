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

import os
import sys

env = Environment()

if "darwin" == os.sys.platform or "linux2" == os.sys.platform:
    env.Append( CPPFLAGS=" -ansi -pedantic -Wall -ggdb " )
    env.Append( CPPPATH=["/opt/local/include/"] )
    env.Append( LIBPATH=["/opt/local/lib/"] )
    if GetOption('use_c99'):
        env.Append( CPPFLAGS=" -std=c99 " )
        

#we shouldn't need these options in c99 mode
if not GetOption('use_c99'):
    conf = Configure(env)

    if not conf.CheckType('int64_t'):
        if conf.CheckType('int64_t', '#include <stdint.h>\n'):
            conf.env.Append( CPPFLAGS=" -DMONGO_HAVE_STDINT " )
        elif conf.CheckType('int64_t', '#include <unistd.h>\n'):
            conf.env.Append( CPPFLAGS=" -DMONGO_HAVE_UNISTD " )
        elif conf.CheckType('__int64'):
            conf.env.Append( CPPFLAGS=" -DMONGO_USE__INT64 " )
        elif conf.CheckType('long long int'):
            conf.env.Append( CPPFLAGS=" -DMONGO_USE_LONG_LONG_INT " )
        else:
            print "*** what is your 64 bit int type? ****"
            Exit(1)

    if conf.CheckType('bool'):
        conf.env.Append( CPPFLAGS=" -DMONGO_HAVE_BOOL " )
    elif conf.CheckType('_Bool', '#include <stdbool.h>\n'):
        conf.env.Append( CPPFLAGS=" -DMONGO_HAVE_STDBOOL " )
    #if we don't have a bool type we default to char

    env = conf.Finish()

if sys.byteorder == 'big':
    env.Append( CPPFLAGS=" -DMONGO_BIG_ENDIAN " )

env.Append( CPPPATH=["src/"] )

coreFiles = []

m = env.Library( "mongoc" , coreFiles + [ "src/mongo.c"] )
b = env.Library( "bson" , coreFiles + [ "src/bson.c"] )

env.Default( env.Alias( "lib" , [ m[0] , b[0] ] ) )

testEnv = env.Clone()
testEnv.Append( LIBS=["json"] )
testEnv.Append( LIBS=[m, b] )
testEnv.Append( CFLAGS=['-DTEST_SERVER=\\"%s\\"'%GetOption('test_server')] )
testEnv.Prepend( LIBPATH=["."] )

testCoreFiles = [ "test/md5.c" ]

for name in Split('sizes endian_swap json simple update errors'):
    filename = "test/%s.c" % name
    exe = "test" + name
    test = testEnv.Program( exe , testCoreFiles + [filename]  )
    test_alias = testEnv.Alias('test', [test], test[0].abspath + ' 2> /dev/null')
    AlwaysBuild(test_alias)

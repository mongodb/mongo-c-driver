# -*- mode: python; -*-

import os

env = Environment()

if "darwin" == os.sys.platform or "linux2" == os.sys.platform:
    env.Append( CPPFLAGS=" -ansi -pedantic -Wall " )
    env.Append( CPPPATH=["/opt/local/include/"] )
    env.Append( LIBPATH=["/opt/local/lib/"] )

env.Append( CPPPATH=["src/"] )

coreFiles = Glob( "src/*.c" );

env.Default( env.Library( "mongoc" , coreFiles ) )

jsonEnv = env.Clone()
jsonEnv.Append( LIBS=["json"] )
jsonEnv.Append( LIBS=["mongoc"] )
jsonEnv.Prepend( LIBPATH=["."] )

env.Program( 'testsimple' , coreFiles + ["test/test.c"]  )
jsonEnv.Program( 'testjson' , ["test/json.c"]  )


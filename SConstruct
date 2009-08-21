# -*- mode: python; -*-

import os

env = Environment()

if "darwin" == os.sys.platform or "linux2" == os.sys.platform:
    env.Append( CPPFLAGS=" -ansi -pedantic " )

env.Append( CPPPATH=["src/"] )

coreFiles = Glob( "src/*.c" );

env.Program( 'simpletest' , coreFiles + ["test/test.c"]  )
env.Library( "mongoc" , coreFiles )

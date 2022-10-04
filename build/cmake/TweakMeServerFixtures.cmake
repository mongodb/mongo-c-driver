# TWEAK ME: Define server test fixtures within this file.

if (0)
    # Examples:
    set (version 5.2.0)
    mongodb_create_fixture (
        "server[topo=single,version=${version}]"
        VERSION ${version}
        DEFAULT
        SERVER_ARGS --setParameter enableTestCommands=1
        )
    mongodb_create_replset_fixture (
        "server[topo=repl,version=${version}]"
        VERSION ${version}
        REPLSET_NAME rs-${version}
        DEFAULT
        )
    mongodb_create_sharded_fixture (
        "server[topo=sharded,version=${version}]"
        VERSION ${version}
        DEFAULT
        )
endif ()

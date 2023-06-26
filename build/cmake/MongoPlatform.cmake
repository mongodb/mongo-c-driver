#[[

Defines a target mongo::detail::c_platform (alias of mongo-platform), which
exposes system-level supporting compile and link usage requirements. All targets
should link to this target with level PUBLIC.

Use mongo_add_platform_compile_options and mongo_add_platform_link_options to
add usage requirements to this library.

The mongo::detail::c_platform library is installed and exported with the
bson-targets export set as an implementation detail. It is installed with this
export set so that it is available to both libbson and libmongoc (attempting to
install this target in both bson-targets and mongoc-targets export sets would
lead to duplicate definitions of mongo::detail::c_platform for downstream users).

]]

add_library(_mongo-platform INTERFACE)
add_library(mongo::detail::c_platform ALIAS _mongo-platform)
set_property(TARGET _mongo-platform PROPERTY EXPORT_NAME detail::c_platform)
install(TARGETS _mongo-platform EXPORT bson-targets)


#[[
Define additional platform-support compile options

These options are added to the mongo::detail::c_platform INTERFACE library.
]]
function (mongo_add_platform_compile_options)
    list(APPEND CMAKE_MESSAGE_CONTEXT ${CMAKE_CURRENT_FUNCTION})
    message(DEBUG "Add platform-support compilation options: ${ARGN}")
    target_compile_options(_mongo-platform INTERFACE ${ARGN})
endfunction ()

#[[
Define additional platform-support link options.

These options are added to the mongo::detail::c_platform INTERFACE library.
]]
function(mongo_add_platform_link_options)
    list(APPEND CMAKE_MESSAGE_CONTEXT ${CMAKE_CURRENT_FUNCTION})
    message(DEBUG "Add platform-support runtime linking options: ${ARGN}")
    target_link_options(_mongo-platform INTERFACE ${ARGN})
endfunction()

# Enable multi-threading:
find_package(Threads REQUIRED)
target_link_libraries(_mongo-platform INTERFACE Threads::Threads)

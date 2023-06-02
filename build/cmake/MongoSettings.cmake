include_guard(DIRECTORY)

#[[ Bool: Set to TRUE if the environment variable MONGODB_DEVELOPER is a true value ]]
set(MONGODB_DEVELOPER FALSE)
#[[ Bool: Set to TRUE if the environment variable MONGODB_BUILD_AUDIT is a true value ]]
set(MONGODB_BUILD_AUDIT FALSE)

# Detect developer mode:
set(_is_dev "$ENV{MONGODB_DEVELOPER}")
if(_is_dev)
    message(STATUS "MONGODB_DEVELOPER is detected")
    set(MONGODB_DEVELOPER TRUE)
endif()

# Detect audit mode:
set(_is_audit "$ENV{MONGODB_BUILD_AUDIT}")
if(_is_audit)
    message(STATUS "MONGODB_BUILD_AUDIT is enabled")
    set(MONGODB_BUILD_AUDIT TRUE)
endif()

#[==[
Define a new configure-time build setting::
    mongo_setting(
        <name> <doc>
        [TYPE <BOOL|PATH|FILEPATH|STRING>]
        [DEFAULT [[DEVEL|AUDIT] [VALUE <value> | EVAL <code>]] ...]
        [OPTIONS [<opts> ...]]
        [VALIDATE [CODE <code>]]
        [ADVANCED] [ALLOW_OTHER_VALUES]
    )

The `<name>` will be the name of the setting, while `<doc>` will be the
documentation string shown to the user. Newlines in the doc string will be
replaced with single spaces. If no other arguments are provided, the default
`TYPE` will be `STRING`, and the DEFAULT value will be an empty string. If the
previous cached value is AUTO, and AUTO is not listed in OPTIONS, then the cache
value will be cleared and reset to the default value.

Package maintainers Note: Setting a variable `<name>-FORCE` to TRUE will make
this function a no-op.

TYPE <BOOL|PATH|FILEPATH|STRING>
    Sets the type for the generated cache variable. If the type is BOOL and
    ALLOW_OTHER_VALUES is not specified, this call will validate that the
    setting is a valid boolean value.

OPTIONS [<opts> ...]
    Specify the valid set of values available for this setting. This will set
    the STRINGS property on the cache variable and add an information message to
    the doc string. Unless ALLOW_OTHER_VALUES is specified, this call will also
    validate that the setting's value is one of these options, failing with an
    error if it is not.

DEFAULT [[DEVEL|AUDIT] [VALUE <value> | EVAL <code>]] ...
    Specify the default value of the generated variable. If given VALUE, then
    `<value>` will be used as the default, otherwise if given EVAL, `<code>`
    will be executed and is expected to define a variable DEFAULT that will
    contain the default value.

    - If neither MONGODB_DEVELOPER nor MONGODB_BUILD_AUDIT are true, then the
      non-qualified defaults will be used. (If no non-qualified defaults are
      provided, then the default value is an empty string.)
    - Otherwise, If DEVEL defaults are provided, and MONGODB_DEVELOPER is true,
      then the DEVEL defaults will be used.
    - Otherwise, if AUDIT defaults are provided, and either MONGODB_DEVELOPER or
      MONGODB_BUILD_AUDIT is true, then the AUDIT defaults will be used.

VALIDATE [CODE <code>]
    If specified, then `<code>` will be evaluated after the setting value is
    defined. `<code>` may issue warnings and errors about the value of the
    setting.

ADVANCED
    If specified, the cache variable will be marked as an advanced setting

ALLOW_OTHER_VALUES
    If *not* specified, this call will validate that the setting's value is
    valid according to TYPE and OPTIONS.

]==]
function(mongo_setting setting_NAME setting_DOC)
    list(APPEND CMAKE_MESSAGE_CONTEXT mongo setting "${setting_NAME}")
    # Allow bypassing this code:
    set(force "${${setting_NAME}-FORCE}")
    if(force)
        return()
    endif()

    cmake_parse_arguments(
        PARSE_ARGV 2 setting
        "ALLOW_OTHER_VALUES;ADVANCED"
        "TYPE"
        "OPTIONS;DEFAULT;VALIDATE")
    # Check for unknown arguments:
    foreach(arg IN LISTS setting_UNPARSED_ARGUMENTS)
        message(SEND_ERROR "Unrecognized argument: “${arg}”")
    endforeach()

    # By default, settings are strings:
    if(NOT DEFINED setting_TYPE)
        set(setting_TYPE STRING)
    endif()

    # More arg validation:
    if(setting_TYPE STREQUAL "BOOL")
        if(DEFINED setting_OPTIONS)
            message(FATAL_ERROR [["OPTIONS" cannot be specified with type "BOOL"]])
        endif()
    endif()

    # Normalize the doc string for easier writing of doc strings at call sites:
    string(REGEX REPLACE "\n[ ]*" " " doc "${setting_DOC}")
    # Update the doc string with options:
    if(DEFINED setting_OPTIONS)
        string(REPLACE ";" ", " opts "${setting_OPTIONS}")
        string(APPEND doc " (Options: ${opts})")
    endif()

    # Get the default option value:
    unset(DEFAULT)
    if(DEFINED setting_DEFAULT)
        _mongo_compute_default(DEFAULT "${setting_DEFAULT}")
    endif()

    if(DEFINED DEFAULT)
        # Add that to the doc message:
        string(APPEND doc " (Default is “${DEFAULT}”)")
        # Check that the default is actually a valid option:
        if(DEFINED setting_OPTIONS
            AND NOT DEFAULT IN_LIST setting_OPTIONS
            AND NOT setting_ALLOW_OTHER_VALUES)
            message(AUTHOR_WARNING "${setting_NAME}: Setting's default value is “${DEFAULT}”, which is not one of the provided setting options (${opts})")
        endif()

        # Reset "AUTO" values to the default
        if(NOT "AUTO" IN_LIST setting_OPTIONS AND "$CACHE{${setting_NAME}}" STREQUAL "AUTO")
            message(WARNING "Replacing ${setting_NAME}=“AUTO” with default value ${setting_NAME}=“${DEFAULT}”")
            unset("${setting_NAME}" CACHE)
        endif()
    endif()

    # Detect the previous value
    unset(prev_val)
    if(DEFINED "CACHE{${setting_NAME}-PREV}")
        set(prev_val "$CACHE{${setting_NAME}-PREV}")
        message(DEBUG "Detected previous value was “${prev_val}”")
    elseif(DEFINED "CACHE{${setting_NAME}}")
        message(DEBUG "Externally defined to be “${${setting_NAME}}”")
    else()
        message(DEBUG "No previous value detected")
    endif()

    # Actually define it now:
    set("${setting_NAME}" "${DEFAULT}" CACHE "${setting_TYPE}" "${doc}")
    # Variable properties:
    set_property(CACHE "${setting_NAME}" PROPERTY HELPSTRING "${doc}")
    set_property(CACHE "${setting_NAME}" PROPERTY TYPE "${setting_TYPE}")
    set_property(CACHE "${setting_NAME}" PROPERTY ADVANCED "${setting_ADVANCED}")
    if(setting_OPTIONS)
        set_property(CACHE "${setting_NAME}" PROPERTY STRINGS "${setting_OPTIONS}")
    endif()

    # Report what we set:
    if(NOT DEFINED prev_val)
        message(VERBOSE "Setting: ${setting_NAME} := “${${setting_NAME}}”")
    elseif("${${setting_NAME}}" STREQUAL prev_val)
        message(DEBUG "Setting: ${setting_NAME} := “${${setting_NAME}}” (Unchanged)")
    else()
        message(VERBOSE "Setting: ${setting_NAME} := “${${setting_NAME}}” (Old value was “${prev_val}”)")
    endif()
    set("${setting_NAME}-PREV" "${${setting_NAME}}" CACHE INTERNAL "Prior value of ${setting_NAME}")

    # Validation of options:
    if(NOT setting_ALLOW_OTHER_VALUES AND (DEFINED setting_OPTIONS) AND (NOT ("${${setting_NAME}}" IN_LIST setting_OPTIONS)))
        message(FATAL_ERROR "The value of “${setting_NAME}” must be one of [${opts}] (Got ${setting_NAME}=“${${setting_NAME}}”)")
    endif()
    if(setting_TYPE STREQUAL "BOOL"
        AND NOT setting_ALLOW_OTHER_VALUES
        AND NOT "${${setting_NAME}}" MATCHES "^(TRUE|FALSE|ON|OFF|YES|NO|0|1)$")
        message(WARNING "The value of ${setting_NAME}=“${${setting_NAME}}” is not a regular boolean value")
    endif()

    # Custom validation:
    if(DEFINED setting_VALIDATE)
        cmake_parse_arguments(validate "" "CODE" "" ${setting_VALIDATE})
        if(DEFINED validate_CODE)
            _mongo_eval_cmake("" "${validate_CODE}")
        endif()
        if(validate_UNPARSED_ARGUMENTS)
            message(FATAL_ERROR "Unrecognized VALIDATE options: ${validate_UNPARSED_ARGUMENTS}")
        endif()
    endif()
endfunction()

#[[ Implements DEFAULT setting value logic ]]
function(_mongo_compute_default outvar arglist)
    list(APPEND CMAKE_MESSAGE_CONTEXT default)
    # Clear the value in the caller:
    unset("${outvar}" PARENT_SCOPE)

    # Parse arguments:
    cmake_parse_arguments(dflt "" "" "DEVEL;AUDIT" ${arglist})

    # Developer-mode options:
    if(DEFINED dflt_DEVEL AND MONGODB_DEVELOPER)
        list(APPEND CMAKE_MESSAGE_CONTEXT "devel")
        _mongo_compute_default(tmp "${dflt_DEVEL}")
        message(DEBUG "Detected MONGODB_DEVELOPER: Default of ${setting_NAME} is “${tmp}”")
        set("${outvar}" "${tmp}" PARENT_SCOPE)
        return()
    # Audit-mode options:
    elseif(DEFINED dflt_AUDIT AND (MONGODB_DEVELOPER OR MONGODB_BUILD_AUDIT))
        list(APPEND CMAKE_MESSAGE_CONTEXT "audit")
        _mongo_compute_default(tmp "${dflt_AUDIT}")
        message(DEBUG "Detected MONGODB_BUILD_AUDIT: Default of ${setting_NAME} is “${tmp}”")
        set("${outvar}" "${tmp}" PARENT_SCOPE)
        return()
    endif()

    # Parse everything else:
    set(other_args "${dflt_UNPARSED_ARGUMENTS}")
    cmake_parse_arguments(dflt "" "VALUE;EVAL" "" ${other_args})

    if(DEFINED dflt_VALUE)
        # Simple value for the default
        if(DEFINED dflt_EVAL)
            message(FATAL_ERROR "Only one of VALUE or EVAL may be specified for a DEFAULT")
        endif()
        set("${outvar}" "${dflt_VALUE}" PARENT_SCOPE)
    elseif(DEFINED dflt_EVAL)
        # Evaluate some code to determine the default
        _mongo_eval_cmake(DEFAULT "${dflt_EVAL}")
        set("${outvar}" "${DEFAULT}" PARENT_SCOPE)
        if(DEFINED DEFAULT)
            message(DEBUG "Computed default ${setting_NAME} value to be “${DEFAULT}”")
        else()
            message(DEBUG "No default for ${setting_NAME} was computed. Default will be an empty string.")
        endif()
    elseif(dflt_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
                "${setting_NAME}: "
                "DEFAULT got unexpected arguments: ${dflt_UNPARSED_ARGUMENTS}")
    endif()
endfunction()

#[==[
Define a new boolean build setting::

    mongo_bool_setting(
        <name> <doc>
        [DEFAULT [[DEVEL|AUDIT] [VALUE <value> | EVAL <code>]] ...]
        [VALIDATE [CODE <code>]]
        [ADVANCED] [ALLOW_OTHER_VALUES]
    )

This is a shorthand for defining a boolean setting. See mongo_setting() for more
option information. The TYPE of the setting will be BOOL, and the implicit
default value for the setting will be ON if no DEFAULT is provided.

]==]
function(mongo_bool_setting name doc)
    set(args ${ARGN})
    # Inject "ON" as a default:
    if(NOT "DEFAULT" IN_LIST args)
        list(APPEND args DEFAULT VALUE ON)
    endif()
    mongo_setting("${name}" "${doc}" TYPE BOOL ${args})
endfunction()

# Set the variable named by 'out' to the 'if_true' or 'if_false' value based on 'cond'
function(_mongo_pick out if_true if_false cond)
    string(REPLACE "'" "\"" cond "${cond}")
    _mongo_eval_cmake("res" "set(res [[${if_false}]])\nif(${cond})\nset(res [[${if_true}]])\nendif()")
    set("${out}" "${res}" PARENT_SCOPE)
endfunction()

# Evaluate CMake code <code>, and lift the given variables into the caller's scope.
function(_mongo_eval_cmake get_variables code)
    # Set a name that is unlikely to collide:
    set(__eval_liftvars "${get_variables}")
    # Clear the values before we evaluate the code:
    foreach(__varname IN LISTS __eval_liftvars)
        unset("${__varname}" PARENT_SCOPE)
        unset("${__varname}")
    endforeach()
    # We do the "eval" the old fashion way, since we can't yet use cmake_language()
    message(TRACE "Evaluating CMake code:\n\n${code}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/_eval.tmp.cmake" "${code}")
    include("${CMAKE_CURRENT_BINARY_DIR}/_eval.tmp.cmake")
    # Lift the variables into the caller's scope
    foreach(__varname IN LISTS __eval_liftvars)
        if(DEFINED "${__varname}")
            message(TRACE "Eval variable result: ${__varname}=${${__varname}}")
            set("${__varname}" "${${__varname}}" PARENT_SCOPE)
        endif()
    endforeach()
endfunction()
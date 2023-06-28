include_guard(GLOBAL)

include(GNUInstallDirs)

if(NOT DEFINED CMAKE_SCRIPT_MODE_FILE)
    define_property(
        TARGET PROPERTY pkg_config_REQUIRES INHERITED
        BRIEF_DOCS "pkg-config 'Requires:' items"
        FULL_DOCS "Specify 'Requires:' items for the targets' pkg-config file"
        )
    define_property(
        TARGET PROPERTY pkg_config_NAME INHERITED
        BRIEF_DOCS "The 'Name' for pkg-config"
        FULL_DOCS "The 'Name' of the pkg-config target"
        )
    define_property(
        TARGET PROPERTY pkg_config_DESCRIPTION INHERITED
        BRIEF_DOCS "The 'Description' pkg-config property"
        FULL_DOCS "The 'Description' property to add to a target's pkg-config file"
        )
    define_property(
        TARGET PROPERTY pkg_config_VERSION INHERITED
        BRIEF_DOCS "The 'Version' pkg-config property"
        FULL_DOCS "The 'Version' property to add to a target's pkg-config file"
        )
    define_property(
        TARGET PROPERTY pkg_config_CFLAGS INHERITED
        BRIEF_DOCS "The 'Cflags' pkg-config property"
        FULL_DOCS "Set a list of options to add to a target's pkg-config file 'Cflags' field"
        )
    define_property(
        TARGET PROPERTY pkg_config_INCLUDE_DIRECTORIES INHERITED
        BRIEF_DOCS "Add -I options to the 'Cflags' pkg-config property"
        FULL_DOCS "Set a list of directories that will be added using -I for the 'Cflags' pkg-config field"
        )
    define_property(
        TARGET PROPERTY pkg_config_LIBS INHERITED
        BRIEF_DOCS "Add linker options to the 'Libs' pkg-config field"
        FULL_DOCS "Set a list of linker options that will joined in a string for the 'Libs' pkg-config field"
        )
endif()

# Given a string, escape any generator-expression-special characters
function(_genex_escape out in)
    # Escape '>'
    string(REPLACE ">" "$<ANGLE-R>" str "${in}")
    # Escape "$"
    string(REPLACE "$" "$<1:$>" str "${str}")
    # Undo the escaping of the dollar for $<ANGLE-R>
    string(REPLACE "$<1:$><ANGLE-R>" "$<ANGLE-R>" str "${str}")
    # Escape ","
    string(REPLACE "," "$<COMMA>" str "${str}")
    # Escape ";"
    string(REPLACE ";" "$<SEMICOLON>" str "${str}")
    set("${out}" "${str}" PARENT_SCOPE)
endfunction()

#[==[
Generate a pkg-config .pc file for the Given CMake target, and optionally a
rule to install it::

    generate_pkg_config(
        <target>
        [FILENAME <filename>]
        [LIBDIR <libdir>]
        [INSTALL [DESTINATION <dir>] [RENAME <filename>]]
        [CONDITION <cond>]
    )

The `<target>` must name an existing target. The following options are accepted:

FILENAME <filename>
    - The generated .pc file will have the given `<filename>`. This name *must*
      be only the filename, and not a qualified path. If omitted, the default
      filename is generated based on the target name. If using a multi-config
      generator, the default filename will include the name of the configuration
      for which it was generated.

LIBDIR <libdir>
    - Specify the subdirectory of the install prefix in which the target binary
      will live. If unspecified, uses `CMAKE_INSTALL_LIBDIR`, which comes from
      the GNUInstallDirs module, which has a default of `lib`.

INSTALL [DESTINATION <dir>] [RENAME <filename>]
    - Generate a rule to install the generated pkg-config file. This is better
      than using a `file(INSTALL)` on the generated file directly, since it
      ensures that the installed .pc file will have the correct install prefix
      value. The following additional arguments are also accepted:

      DESTINATION <dir>
        - If provided, specify the *directory* (relative to the install-prefix)
          in which the generated file will be installed. If unspecfied, the default
          destination is `<libdir>/pkgconfig`

      RENAME <filename>
        - If provided, set the filename of the installed pkg-config file. If
          unspecified, the top-level `<filename>` will be used. (Note that the
          default top-level `<filename>` will include the configuration type
          when built/installed using a multi-config generator!)

CONDITION <cond>
    - The file will only be generated/installed if the condition `<cond>`
      results in the string "1" after evaluating generator expressions.

All named parameters accept generator expressions.

]==]
function(mongo_generate_pkg_config target)
    # Collect some target properties:
    set(tprop "TARGET_PROPERTY:${target}")
    # The name:
    _genex_escape(proj_name "${PROJECT_NAME}")
    string(CONCAT pc_name
        $<IF:$<STREQUAL:,$<${tprop},pkg_config_NAME>>,
            ${proj_name},
            $<${tprop},pkg_config_NAME>>)
    # Version:
    string(CONCAT pc_version
        $<IF:$<STREQUAL:,$<${tprop},pkg_config_VERSION>>,
            ${PROJECT_VERSION},
            $<${tprop},pkg_config_VERSION>>)
    # Description:
    _genex_escape(proj_desc "${PROJECT_DESCRIPTION}")
    string(CONCAT pc_desc
        $<IF:$<STREQUAL:,$<${tprop},pkg_config_DESCRIPTION>>,
            ${proj_desc},
            $<${tprop},pkg_config_DESCRIPTION>>)

    # Parse and validate arguments:
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "FILENAME;LIBDIR;CONDITION" "INSTALL")

    if(NOT ARG_FILENAME)
        # No filename given. Pick a default:
        if(DEFINED CMAKE_CONFIGURATION_TYPES)
            # Multi-conf: We may want to generate more than one, so qualify the
            # filename with the configuration type:
            set(ARG_FILENAME "$<TARGET_FILE_BASE_NAME:${target}>-$<LOWER_CASE:$<CONFIG>>.pc")
        else()
            # Just generate a file based on the basename of the target:
            set(ARG_FILENAME "$<TARGET_FILE_BASE_NAME:${target}>.pc")
        endif()
    endif()
    if(NOT DEFINED ARG_CONDITION)
        set(ARG_CONDITION 1)
    endif()
    if(NOT ARG_LIBDIR)
        set(ARG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
    endif()
    _genex_escape(filename "${ARG_FILENAME}")
    set(filename $<TARGET_GENEX_EVAL:${target},${filename}>)
    if(IS_ABSOLUTE "${ARG_FILENAME}")
        set(generate_file "${filename}")
    else()
        get_filename_component(generate_file "${CMAKE_CURRENT_BINARY_DIR}/${filename}" ABSOLUTE)
    endif()

    _generate_pkg_config_content(content
        NAME "${pc_name}"
        VERSION "${pc_version}"
        DESCRIPTION "${pc_desc}"
        PREFIX "%INSTALL_PLACEHOLDER%"
        LIBDIR "${ARG_LIBDIR}"
        GENEX_TARGET "${target}"
        )
    string(REPLACE "%INSTALL_PLACEHOLDER%" "${CMAKE_INSTALL_PREFIX}" with_prefix "${content}")
    file(GENERATE OUTPUT "${generate_file}" CONTENT "${with_prefix}" CONDITION "${ARG_CONDITION}")
    set(this_file "${CMAKE_CURRENT_FUNCTION_LIST_FILE}")
    if(NOT DEFINED ARG_INSTALL AND NOT "INSTALL" IN_LIST ARG_KEYWORDS_MISSING_VALUES)
        # Nothing more to do here.
        return()
    endif()
    # Use file(GENERATE) to generate a temporary file to be picked up at install-time.
    # (For some reason, injecting this directly into install(CODE) fails in corner cases)
    set(tmpfile "${CMAKE_CURRENT_BINARY_DIR}/${target}-$<LOWER_CASE:$<CONFIG>>-pc-install.txt")
    file(GENERATE OUTPUT "${tmpfile}"
            CONTENT "${content}"
            CONDITION "${ARG_CONDITION}")
    # Parse the install args that we will inspect:
    cmake_parse_arguments(inst "" "DESTINATION;RENAME" "" ${ARG_INSTALL})
    if(NOT DEFINED inst_DESTINATION)
        # Install based on the libdir:
        set(inst_DESTINATION "${ARG_LIBDIR}/pkgconfig")
    endif()
    if(NOT DEFINED inst_RENAME)
        set(inst_RENAME "${ARG_FILENAME}")
    endif()
    # install(CODE) will write a simple temporary file:
    set(inst_tmp "${CMAKE_CURRENT_BINARY_DIR}/${target}-pkg-config-tmp.txt")
    _genex_escape(cond_genex_str "${ARG_CONDITION}")
    string(CONFIGURE [==[
        $<@ARG_CONDITION@:
            # Installation of pkg-config for target @target@
            message(STATUS "Generating pkg-config file: @inst_RENAME@")
            file(READ [[@tmpfile@]] content)
            # Insert the install prefix:
            string(REPLACE "%INSTALL_PLACEHOLDER%" "${CMAKE_INSTALL_PREFIX}" content "${content}")
            # Write it before installing again:
            file(WRITE [[@inst_tmp@]] "${content}")
        >
        $<$<NOT:@ARG_CONDITION@>:
            # Installation was disabled for this generation.
            if(EXISTS [[@inst_tmp@]])
                file(REMOVE [[@inst_tmp@]])
            endif()
            message(STATUS "Skipping install of file [@inst_RENAME@]: Disabled by CONDITION “@cond_genex_str@”")
        >
    ]==] code @ONLY)
    install(CODE "${code}")
    install(FILES "$<${ARG_CONDITION}:${inst_tmp}>"
            DESTINATION "${inst_DESTINATION}"
            RENAME "${inst_RENAME}"
            ${inst_UNPARSED_ARGUMENTS})
endfunction()

# Generates the actual content of a .pc file.
function(_generate_pkg_config_content out)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "PREFIX;NAME;VERSION;DESCRIPTION;GENEX_TARGET;LIBDIR" "")
    foreach(arg IN LISTS ARG_UNPARSE_ARGUMENTS)
        message(FATAL_ERROR "Unknown argument: ${arg}")
    endforeach()
    set(content)
    string(APPEND content
        "# pkg-config .pc file generated by CMake ${CMAKE_VERSION} for ${ARG_NAME}-${ARG_VERSION}. DO NOT EDIT!\n"
        "prefix=${ARG_PREFIX}\n"
        "exec_prefix=\${prefix}\n"
        "libdir=\${exec_prefix}/${ARG_LIBDIR}\n"
        "\n"
        "Name: ${ARG_NAME}\n"
        "Description: ${ARG_DESCRIPTION}\n"
        "Version: ${ARG_VERSION}"
        )
    # Add Requires:
    set(requires_joiner "\nRequires: ")
    set(gx_requires "$<prop:pkg_config_REQUIRES>")
    set(has_requires "$<NOT:$<STREQUAL:,${gx_requires}>>")
    string(APPEND content "$<${has_requires}:${requires_joiner}$<JOIN:${gx_requires},${requires_joiner}>>\n")
    string(APPEND content "\n")
    # Add "Libs:"
    set(libs)
    # Link options:
    set(gx_libs
        [[-L${libdir}]]
        "-l$<prop:OUTPUT_NAME>"
        $<TARGET_GENEX_EVAL:${ARG_GENEX_TARGET},$<prop:pkg_config_LIBS>>
        $<prop:INTERFACE_LINK_OPTIONS>
        )
    string(APPEND libs "$<JOIN:${gx_libs};${gx_linkopts}, >")

    # Cflags:
    set(cflags)
    set(gx_flags
        $<REMOVE_DUPLICATES:$<TARGET_GENEX_EVAL:${ARG_GENEX_TARGET},$<prop:pkg_config_CFLAGS>>>
        $<REMOVE_DUPLICATES:$<prop:INTERFACE_COMPILE_OPTIONS>>
        )
    string(APPEND cflags "$<JOIN:${gx_flags}, >")
    # Definitions:
    set(gx_defs $<REMOVE_DUPLICATES:$<prop:INTERFACE_COMPILE_DEFINITIONS>>)
    set(has_defs $<NOT:$<STREQUAL:,${gx_defs}>>)
    set(def_joiner " -D")
    string(APPEND cflags $<${has_defs}:${def_joiner}$<JOIN:${gx_defs},${def_joiner}>>)
    # Includes:
    set(gx_inc $<prop:pkg_config_INCLUDE_DIRECTORIES>)
    set(gx_inc "$<REMOVE_DUPLICATES:${gx_inc}>")
    set(gx_abs_inc "$<FILTER:${gx_inc},INCLUDE,^/>")
    set(gx_rel_inc "$<FILTER:${gx_inc},EXCLUDE,^/>")
    set(has_abs_inc $<NOT:$<STREQUAL:,${gx_abs_inc}>>)
    set(has_rel_inc $<NOT:$<STREQUAL:,${gx_rel_inc}>>)
    string(APPEND cflags $<${has_rel_inc}: " -I\${prefix}/"
                            $<JOIN:${gx_rel_inc}, " -I\${prefix}/" >>
                            $<${has_abs_inc}: " -I"
                            $<JOIN:${gx_abs_inc}, " -I" >>)
    string(APPEND content "Libs: ${libs}\n")
    string(APPEND content "Cflags: ${cflags}\n")
    string(REPLACE "$<prop:" "$<TARGET_PROPERTY:${ARG_GENEX_TARGET}," content "${content}")

    set("${out}" "${content}" PARENT_SCOPE)
endfunction()

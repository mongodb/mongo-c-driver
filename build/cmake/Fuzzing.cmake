option (ENABLE_FUZZING "Enable fuzzing using LLVM libFuzzer" OFF)

if (ENABLE_FUZZING)
    # This will add another sanitizer when we later include Sanitizers.cmake
    list (APPEND MONGOC_SANITIZE "fuzzer-no-link")
endif ()

include (ProcessorCount)
ProcessorCount (_FUZZER_PARALLELISM)

set (_FUZZERS_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/fuzzers")


#[[
    Generate an executable target that links and runs with LLVM libFuzzer.

    Amongst the given source files there must be one definition of LLVMFuzzerTestOneInput.
    Refer: https://www.llvm.org/docs/LibFuzzer.html#fuzz-target

    This will define an executable with the given name, and all additional
    arguments will be given as source files to that executable. This executable
    will be linked with the '-fsanitize=fuzzer' command-line option.

    This will additionally define a custom target "run-fuzzer-${name}," which,
    when executed, will run the fuzzer executable with a set of pre-defined
    libFuzzer command-line options.

    The following target properties can be used to control the 'run-fuzzer'
    target:

    FUZZER_FORK (integer)
        Set the number of parallel fuzzer tasks to run. The default is the
        parallelism of the host plus four.

    FUZZER_TIMEOUT (integer, seonds)
        Set the maximum amount a single fuzzer task should run before the fuzzer
        consideres it to be "stuck" and to generate a timeout report for the
        given input.

    FUZZER_LEN_CONTROL (integer, 1-100)
        Set the len_control option for the libFuzzer run. Lower values tend to
        generate larger inputs. Default is 50.

    FUZZER_MAX_LEN (integer, bytes)
        Set the maximum input size for a fuzzer input. The default is 4096.

    FUZZER_ONLY_ASCII (boolean)
        If TRUE, only valid ASCII will be given as fuzzer input.
        The default is FALSE.

    FUZZER_DICT (filepath)
        Set to a filepath of a fuzzer dictionary.
        Refer: https://www.llvm.org/docs/LibFuzzer.html#dictionaries
        Default is to have no dictionary.

    Fuzzer executables are written to to the <BUILD_DIR>/fuzzers directory.

    This will unconditionally define the target and the custom target that
    executes it, but it will be EXCLUDE_FROM_ALL=TRUE if the CMake setting
    ENABLE_FUZZING is not true.
]]
function (mongoc_add_fuzzer name)
    add_executable ("${name}" ${ARGN})
    # Run with 4 more jobs than hardware parallelism
    math (EXPR default_fork "${_FUZZER_PARALLELISM} + 4")
    set_target_properties("${name}" PROPERTIES
        # Qualify the filename with the build type:
        DEBUG_POSTFIX ".debug"
        RELEASE_POSTFIX ".opt"
        RELWITHDEBINFO_POSTFIX ".opt-debug"
        # Put them all in the fuzzers/ directory:
        RUNTIME_OUTPUT_DIRECTORY "${_FUZZERS_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${_FUZZERS_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${_FUZZERS_OUT_DIR}"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${_FUZZERS_OUT_DIR}"
        # Target options to control the fuzzer run:
        FUZZER_FORK "${default_fork}"
        FUZZER_TIMEOUT "10"
        FUZZER_LEN_CONTROL "50"
        FUZZER_MAX_LEN "4096"
        FUZZER_ONLY_ASCII "FALSE"
        )
    # Link with the libFuzzer runtime:
    target_link_libraries ("${name}" PRIVATE -fsanitize=fuzzer)

    set (dict "$<TARGET_PROPERTY:${name},FUZZER_DICT>")
    set (art_dir "$<TARGET_FILE_DIR:${name}>/${name}.out/")
    add_custom_target(run-fuzzer-${name}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${art_dir}/corpus"
        # Print some usefile info for the user:
        COMMAND "${CMAKE_COMMAND}" -E echo
        COMMAND "${CMAKE_COMMAND}" -E echo
            "  Running fuzzer program : $<TARGET_FILE:${name}>"
        COMMAND "${CMAKE_COMMAND}" -E echo
            "     Corpus is stored in : ${art_dir}/corpus"
        COMMAND "${CMAKE_COMMAND}" -E echo
            "  Crashes will appear in : ${art_dir}"
        COMMAND "${CMAKE_COMMAND}" -E echo
        # Run the fuzzer:
        COMMAND
            "${CMAKE_COMMAND}" -E chdir "${art_dir}"
            "$<TARGET_FILE:${name}>"
                -create_missing_dirs=1
                -collect_data_flow=1
                -shrink=1  # Try to shrink the test corpus
                -use_value_profile=1
                -ignore_timeouts=0  # Do not ignore timeouts
                -ignore_ooms=0  # Do not ignore OOMs
                -reload=10  # Reload every ten seconds
                "-artifact_prefix=${art_dir}"
                # Target property options:
                "-fork=$<TARGET_PROPERTY:${name},FUZZER_FORK>"
                "-timeout=$<TARGET_PROPERTY:${name},FUZZER_TIMEOUT>"
                "-max_len=$<TARGET_PROPERTY:${name},FUZZER_MAX_LEN>"
                "-len_control=$<TARGET_PROPERTY:${name},FUZZER_LEN_CONTROL>"
                "-only_ascii=$<BOOL:$<TARGET_PROPERTY:${name},FUZZER_ONLY_ASCII>>"
                "-analyze_dict=$<BOOL:${dict}>"
                "$<IF:$<BOOL:${dict}>,-dict=${dict},${art_dir}/corpus>"
                "${art_dir}/corpus"
        WORKING_DIRECTORY "${_FUZZERS_OUT_DIR}"
        DEPENDS "${name}"
        VERBATIM USES_TERMINAL
        )

    # We might not want to build by default:
    if (NOT ENABLE_FUZZING)
        # Fuzzing is not enabled. Exclude the target from being built by default, but still define
        # it so that CMake can verify that it is used correctly.
        set_property (TARGET "${name}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    endif ()
endfunction ()

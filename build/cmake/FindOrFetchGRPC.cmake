#[[
   Ensure a gRPC package is found when ENABLE_GRPC=ON.

   When ENABLE_GRPC=OFF, set MONGOC_ENABLE_GRPC=OFF and do nothing else.

   Otherwise, when ENABLE_GRPC_FIND_PACKAGE=ON, call find_package() for gRPC and
   do nothing else (use a system or custom gRPC package installation).

   Otherwise, use FetchContent to download and install the gRPC package into the
   current binary directory and call find_package() on the installed package.
   Include the installed gRPC package in the installation rules with
   install(DIRECTORY) so it is installed alongside libmongoc.
]]

if (ENABLE_GRPC)
   if (ENABLE_GRPC_FIND_PACKAGE)
      find_package(protobuf CONFIG REQUIRED) # Workaround.
      find_package(gRPC CONFIG REQUIRED)
   else ()
      find_package(Git REQUIRED QUIET)

      # Adapt FetchContent defaults.
      set (grpc-src "${CMAKE_CURRENT_BINARY_DIR}/grpc-src")
      set (grpc-subbuild "${CMAKE_CURRENT_BINARY_DIR}/grpc-subbuild")
      set (grpc-build "${CMAKE_CURRENT_BINARY_DIR}/grpc-build")
      set (grpc-install "${CMAKE_CURRENT_BINARY_DIR}/grpc-install")

      # The gRPC CMake project contains *a lot* of global config variables.
      # Isolate the project by doing an install + find_package() rather than
      # add_subdirectory().
      FetchContent_Declare (
         MONGOC_gRPC

         SOURCE_DIR "${grpc-src}"
         SUBBUILD_DIR "${grpc-subbuild}"
         BINARY_DIR "${grpc-build}"
         INSTALL_DIR "${grpc-install}"

         # ExternalProject recursively initializes all submodules by default,
         # which is *extremely* slow and unnecessary. Manually shallow-clone the
         # gRPC repository *and* submodules to significantly reduce download time.
         # See:
         #  - https://gitlab.kitware.com/cmake/cmake/-/issues/17770
         #  - https://gitlab.kitware.com/cmake/cmake/-/issues/20314
         DOWNLOAD_COMMAND
            COMMAND "${GIT_EXECUTABLE}" init "${grpc-src}" # Avoid non-empty directory errors.
            COMMAND "${GIT_EXECUTABLE}" -C "${grpc-src}" fetch --force --progress --depth 1 "https://github.com/grpc/grpc" "v1.57.0"
            COMMAND "${GIT_EXECUTABLE}" -C "${grpc-src}" checkout --force --progress FETCH_HEAD
            COMMAND "${GIT_EXECUTABLE}" -C "${grpc-src}" submodule update --force --progress --init --depth 1 --jobs 16

         LOG_DOWNLOAD TRUE
         LOG_OUTPUT_ON_FAILURE TRUE
      )
      FetchContent_GetProperties (MONGOC_gRPC)

      # Because NO_DEFAULT_PATH is used to ensure only the locally installed gRPC
      # package is found, all dependencies must be enumerated manually.
      set (GRPC_PACKAGES absl utf8_range protobuf gRPC)

      # Check if the gRPC package install directory is already populated.
      foreach (pkg IN LISTS GRPC_PACKAGES)
         find_package("${pkg}" CONFIG NO_DEFAULT_PATH PATHS "${grpc-install}" QUIET)
      endforeach ()

      # Use both FindPackage and the cache to ensure the package is up-to-date.
      if (gRPC_FOUND AND "$CACHE{INTERNAL_MONGOC_GRPC_FOUND}")
         # The gRPC package install directory is already populated and up-to-date.
      else ()
         if (NOT mongoc_grpc_POPULATED)
            message (STATUS "Downloading gRPC package... (this may take a while!)")
            FetchContent_Populate (MONGOC_gRPC)
            message (STATUS "Downloading gRPC package... done.")
         endif ()

         # Consistency with ENABLE_SSL=GRPC.
         if (APPLE)
            set (grpc_ssl_provider "module")
         else ()
            set (grpc_ssl_provider "package")
         endif ()

         # Consistency with ENABLE_ZLIB=GRPC.
         if (WIN32)
            set (grpc_zlib_provider "module")
         else ()
            set (grpc_zlib_provider "package")
         endif ()

         # Include debug info for development purposes.
         set (grpc_configuration_type "RelWithDebInfo")

         message (STATUS "Configuring gRPC package... (this may take a while!)")
         execute_process (
            COMMAND
               "${CMAKE_COMMAND}"
                  "-S" "${grpc-src}"
                  "-B" "${grpc-build}"
                  "-DCMAKE_BUILD_TYPE=${grpc_configuration_type}"

                  # ABSL forward-compatibility.
                  "-DABSL_PROPAGATE_CXX_STD:BOOL=ON"

                  # Disable unnecessary components and plugins.
                  "-DBUILD_TESTING:BOOL=OFF"
                  "-DCARES_BUILD_TOOLS:BOOL=OFF"
                  "-DRE2_BUILD_TESTING:BOOL=OFF"
                  "-DgRPC_BUILD_CODEGEN:BOOL=ON" # This is required!
                  # "-DgRPC_BUILD_CSHARP_EXT:BOOL=OFF" # Unused?
                  "-DgRPC_BUILD_GRPC_CPP_PLUGIN:BOOL=ON" # This is required!
                  "-DgRPC_BUILD_GRPC_CSHARP_PLUGIN:BOOL=OFF"
                  "-DgRPC_BUILD_GRPC_NODE_PLUGIN:BOOL=OFF"
                  "-DgRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN:BOOL=OFF"
                  "-DgRPC_BUILD_GRPC_PHP_PLUGIN:BOOL=OFF"
                  "-DgRPC_BUILD_GRPC_PYTHON_PLUGIN:BOOL=OFF"
                  "-DgRPC_BUILD_GRPC_RUBY_PLUGIN:BOOL=OFF"

                  # Ensure same libraries are used for both gRPC and libmongoc.
                  "-DgRPC_SSL_PROVIDER:STRING=${grpc_ssl_provider}"
                  "-DgRPC_ZLIB_PROVIDER:STRING=${grpc_zlib_provider}"
            RESULT_VARIABLE retval
            OUTPUT_FILE "${grpc-build}/configure.log"
         )
         if (NOT "${retval}" STREQUAL "0")
            message (FATAL_ERROR "execute_process() fatal error: ${retval}")
         endif ()
         message (STATUS "Configuring gRPC package... done.")

         message (STATUS "Building gRPC package... (this may take a while!)")
         execute_process (
            COMMAND
               "${CMAKE_COMMAND}"
                  --build "${grpc-build}"
                  --config "${grpc_configuration_type}"
            RESULT_VARIABLE retval
            OUTPUT_FILE "${grpc-build}/build.log"
         )
         if (NOT "${retval}" STREQUAL "0")
            message (FATAL_ERROR "execute_process() fatal error: ${retval}")
         endif ()
         message (STATUS "Building gRPC package... done.")

         message (STATUS "Installing gRPC package...")
         execute_process (
            COMMAND
               "${CMAKE_COMMAND}"
                  --install "${grpc-build}"
                  --config "${grpc_configuration_type}"
                  --prefix "${grpc-install}"
            RESULT_VARIABLE retval
            OUTPUT_FILE "${grpc-build}/install.log"
         )
         if (NOT "${retval}" STREQUAL "0")
            message (FATAL_ERROR "execute_process() fatal error: ${retval}")
         endif ()
         message (STATUS "Installing gRPC package... done.")

         foreach (pkg IN LISTS GRPC_PACKAGES)
            find_package ("${pkg}" CONFIG REQUIRED NO_DEFAULT_PATH PATHS "${grpc-install}")
         endforeach ()

         # Ensure the gRPC package is (re)configured if the cache is fresh.
         set (INTERNAL_MONGOC_GRPC_FOUND TRUE CACHE INTERNAL "")
      endif ()

      # Install gRPC libraries alongside libmongoc. The '/' is significant!
      install (DIRECTORY "${grpc-install}/" DESTINATION "${CMAKE_INSTALL_PREFIX}")
   endif ()

   set (MONGOC_ENABLE_GRPC 1)
else ()
   set (MONGOC_ENABLE_GRPC 0)
endif ()

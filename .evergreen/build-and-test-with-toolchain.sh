#!/bin/sh
set -o errexit  # Exit the script with error if any of the commands fail

echo "BUILDING WITH TOOLCHAIN"

# Configure environment with toolchain components
[ -d /opt/mongo-c-toolchain ] && sudo rm -r /opt/mongo-c-toolchain
sudo mkdir /opt/mongo-c-toolchain
TOOLCHAIN_TAR_GZ=$(readlink -f ../mongo-c-toolchain.tar.gz)
sudo tar -xvf "${TOOLCHAIN_TAR_GZ}" -C /opt/mongo-c-toolchain
echo "--- TOOLCHAIN MANIFEST ---"
cat /opt/mongo-c-toolchain/MANIFEST.txt
OLD_PATH=${PATH}
ADDL_PATH=$(readlink -f /opt/mongo-c-toolchain/bin):${PATH}
export CMAKE=$(readlink -f /opt/mongo-c-toolchain/bin/cmake)
[ -x "${CMAKE}" ] || (echo "CMake (${CMAKE}) does not exist or is not executable"; exit 1)
TOOLCHAIN_BASE_DIR=$(readlink -f /opt/mongo-c-toolchain)
TOOLCHAIN_LIB_DIR=${TOOLCHAIN_BASE_DIR}/lib

for ssl_ver in libressl-2.5 libressl-3.0 openssl-0.9.8 openssl-1.0.0 openssl-1.0.1 openssl-1.0.1-fips openssl-1.0.2 openssl-1.1.0; do

   cd ..
   cp -a mongoc mongoc-${ssl_ver}
   cd mongoc-${ssl_ver}
   export PATH=$(readlink -f /opt/mongo-c-toolchain/${ssl_ver}/bin):${ADDL_PATH}:${OLD_PATH}
   ssl_base_dir=$(readlink -f /opt/mongo-c-toolchain/${ssl_ver})
   ssl_lib_dir=${ssl_base_dir}/lib
   export EXTRA_CONFIGURE_FLAGS="-DCMAKE_VERBOSE_MAKEFILE=ON"
   export EXTRA_CMAKE_PREFIX_PATH="${ssl_base_dir};${TOOLCHAIN_BASE_DIR}"

   # Output some information about our build environment
   "${CMAKE}" --version

   # Run the build and tests
   export LD_LIBRARY_PATH="${ssl_lib_dir}:${TOOLCHAIN_LIB_DIR}"
   if [ "${ssl_ver#*libressl}" != "${ssl_ver}" ]; then
      SSL_TYPE=LIBRESSL
   else
      SSL_TYPE=OPENSSL
   fi
   output_file=$(mktemp)
   SSL=${SSL_TYPE} sh ./.evergreen/compile-unix.sh 2>&1 | tee -a "${output_file}"

   # Verify that the toolchain components were used
   if grep -Ec "[-]I/opt/mongo-c-toolchain/include" "${output_file}" >/dev/null \
         && grep -Ec "[-]I/opt/mongo-c-toolchain/${ssl_ver}/include" "${output_file}" >/dev/null \
         && grep -Ec "[-]L/opt/mongo-c-toolchain/lib" "${output_file}" >/dev/null \
         && grep -Ec "/opt/mongo-c-toolchain/${ssl_ver}/lib" "${output_file}" >/dev/null; then
      echo "Toolchain components for ${ssl_ver} were detected in build output...continuing."
   else
      echo "TOOLCHAIN COMPONENTS FOR ${ssl_ver} NOT DETECTED IN BUILD OUTPUT...ABORTING!"
      exit 1
   fi
   rm -f "${output_file}"

   cd ../mongoc
done

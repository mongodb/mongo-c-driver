include(TestProject)

# A bare find_package will succeed
add_test_cmake_project(
   mongoc/CMake/bare-bson-import src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=REQUIRED"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/CMake/bare-mongoc-import src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_MONGOC=1
      "FIND_MONGOC_ARGS=REQUIRED"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
      "EXPECT_MONGOC_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/CMake/bson-import-1.0 src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=1.25;REQUIRED"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

# Try to import a too-new version of 1.x that will never exist
add_test_cmake_project(
   mongoc/CMake/bson-import-too-new-fails src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=1.9999.0"
      EXPECT_FIND_BSON_FAILS=TRUE
)

# Try to import a 2.0 version, which is not installed in this test case
add_test_cmake_project(
   mongoc/CMake/bson-import-2.0-fails src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=2.0"
      EXPECT_FIND_BSON_FAILS=TRUE
)

# Try to import a range of versions
add_test_cmake_project(
   mongoc/CMake/bson-import-range-upper src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=1.0...${PROJECT_VERSION};REQUIRED"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/CMake/bson-import-range-lower src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=${PROJECT_VERSION}...1.9999.0;REQUIRED"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/CMake/bson-import-range-exclusive src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=1.0...<${PROJECT_VERSION}"
      EXPECT_FIND_BSON_FAILS=TRUE
)

add_test_cmake_project(
   mongoc/CMake/bson-import-major-range src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=1.0...2.0;REQUIRED"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/CMake/bson-import-major-range-too-new src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=2.0...<3"
      EXPECT_FIND_BSON_FAILS=TRUE
)

add_test_cmake_project(
   mongoc/CMake/bson-import-bad-components src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=COMPONENTS;foo"
      EXPECT_FIND_BSON_FAILS=TRUE
)

add_test_cmake_project(
   mongoc/CMake/bson-import-opt-components src/libmongoc/tests/cmake-import
   INSTALL_PARENT
   SETTINGS
      FIND_BSON=1
      "FIND_BSON_ARGS=REQUIRED;OPTIONAL_COMPONENTS;foo"
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/pkg-config/bson-import-shared src/libmongoc/tests/pkg-config-import
   INSTALL_PARENT
   SETTINGS
      PKG_CONFIG_SEARCH=bson${PROJECT_VERSION_MAJOR}
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/pkg-config/bson-import-static src/libmongoc/tests/pkg-config-import
   INSTALL_PARENT
   SETTINGS
      PKG_CONFIG_SEARCH=bson${PROJECT_VERSION_MAJOR}-static
      "EXPECT_BSON_VERSION=${mongo-c-driver_VERSION_FULL}"
      EXPECT_BSON_STATIC=1
)

add_test_cmake_project(
   mongoc/pkg-config/mongoc-import-shared src/libmongoc/tests/pkg-config-import
   INSTALL_PARENT
   SETTINGS
      PKG_CONFIG_SEARCH=mongoc1
      "EXPECT_MONGOC_VERSION=${mongo-c-driver_VERSION_FULL}"
)

add_test_cmake_project(
   mongoc/pkg-config/mongoc-import-static src/libmongoc/tests/pkg-config-import
   INSTALL_PARENT
   SETTINGS
      PKG_CONFIG_SEARCH=mongoc1-static
      "EXPECT_MONGOC_VERSION=${mongo-c-driver_VERSION_FULL}"
      EXPECT_BSON_STATIC=1
      EXPECT_MONGOC_STATIC=1
)

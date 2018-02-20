file (GLOB_RECURSE tests_DIST_cs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c)
file (GLOB_RECURSE tests_DIST_hs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.h)
file (GLOB_RECURSE tests_DIST_zeros RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.0)
file (GLOB_RECURSE tests_DIST_pems RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.pem)
file (GLOB_RECURSE tests_DIST_dats RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.dat)
file (GLOB_RECURSE tests_DIST_txts RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.txt)
file (GLOB_RECURSE tests_DIST_jsons RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.json)

set_dist_list (tests_DIST
   CMakeLists.txt
   ${tests_DIST_cs}
   ${tests_DIST_hs}
   ${tests_DIST_zeros}
   ${tests_DIST_pems}
   ${tests_DIST_dats}
   ${tests_DIST_txts}
   ${tests_DIST_jsons}
)

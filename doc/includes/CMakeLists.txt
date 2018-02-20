file (GLOB doc_includes_DIST_txts RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.txt)

set_dist_list (doc_includes_DIST
   CMakeLists.txt
   ${doc_includes_DIST_txts}
)

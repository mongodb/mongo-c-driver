file (GLOB doc_static_DIST_pngs RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.png)

set_dist_list (doc_static_DIST
   CMakeLists.txt
   ${doc_static_DIST_pngs}
)

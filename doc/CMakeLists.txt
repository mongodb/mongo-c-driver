include (SphinxBuild)

if (ENABLE_HTML_DOCS)
   sphinx_build_html (mongoc-html mongo-c-driver)
endif ()

if (ENABLE_MAN_PAGES)
   sphinx_build_man (mongoc-man)
endif()

add_subdirectory (html)
add_subdirectory (includes)
add_subdirectory (man)
add_subdirectory (static)

file (GLOB doc_DIST_rsts RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.rst)

extra_dist_generated (${doc_DIST_htmls} ${doc_DIST_mans})

set_local_dist (doc_DIST_local
   CMakeLists.txt
   ${doc_DIST_rsts}
   conf.py
   libbson-objects.inv
)

set (doc_DIST
   ${doc_DIST_local}
   ${doc_html_DIST}
   ${doc_includes_DIST}
   ${doc_man_DIST}
   ${doc_mongoc_DIST}
   ${doc_mongoc-theme_DIST}
   ${doc_static_DIST}
   PARENT_SCOPE
)

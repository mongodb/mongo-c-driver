include (SphinxBuild)

if (ENABLE_HTML_DOCS)
   sphinx_build_html (mongoac-html mongoac)
endif ()

if (ENABLE_MAN_PAGES)
   sphinx_build_man (mongoac-man)
endif ()

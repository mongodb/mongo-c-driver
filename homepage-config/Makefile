# Minimal makefile for Sphinx documentation
#

# You can set these variables from the command line.
SPHINXOPTS    =
SPHINXBUILD   = sphinx-build
SPHINXPROJ    = mongocorg
SOURCEDIR     = .

# This is eccentric: for mongoc.org we want to build into the root dir.
BUILDDIR      = ..

html: Makefile
	@$(SPHINXBUILD) -nW -b html "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS)

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

.PHONY: help Makefile

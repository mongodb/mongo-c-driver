.SILENT:

.PHONY: poetry-install html serve

default: html

THIS_FILE := $(realpath $(lastword $(MAKEFILE_LIST)))
THIS_DIR := $(shell dirname $(THIS_FILE))
MONGOC_DIR := $(shell dirname $(shell dirname $(THIS_DIR)))
TOOLS_DIR := $(MONGOC_DIR)/tools


POETRY := bash $(TOOLS_DIR)/poetry.sh -C $(MONGOC_DIR)

BUILD_DIR := $(MONGOC_DIR)/_build
_poetry_stamp := $(BUILD_DIR)/.poetry-install.stamp
poetry-install: $(_poetry_stamp)
$(_poetry_stamp): $(MONGOC_DIR)/poetry.lock $(MONGOC_DIR)/pyproject.toml
	$(POETRY) install --with=dev,docs
	mkdir -p $(BUILD_DIR)
	touch $@

SPHINX_JOBS ?= auto
SPHINX_ARGS := -W -n -j "$(SPHINX_JOBS)" -a -b dirhtml

DOCS_SRC := $(THIS_DIR)
DOCS_OUT := $(BUILD_DIR)/docs/dev/html
html: poetry-install
	$(POETRY) run sphinx-build $(SPHINX_ARGS) $(DOCS_SRC) $(DOCS_OUT)

serve: poetry-install
	$(POETRY) run sphinx-autobuild $(SPHINX_ARGS) $(DOCS_SRC) $(DOCS_OUT)

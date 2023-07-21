.SILENT:
.PHONY: poetry-install libbson-docs libmongoc-docs

SPHINX_JOBS ?= 12
SPHINX_ARGS := -n -j "$(SPHINX_JOBS)" -T -b dirhtml
POETRY := bash tools/poetry.sh
SPHINX_BUILD := $(POETRY) run sphinx-build $(SPHINX_ARGS)
SPHINX_AUTOBUILD := $(POETRY) run sphinx-autobuild $(SPHINX_ARGS)

poetry-install: _build/.poetry-install.stamp
_build/.poetry-install.stamp: poetry.lock
	$(POETRY) install --with=dev,docs
	touch $@

libmongoc-docs: poetry-install
	$(SPHINX_BUILD) --keep-going -W src/libmongoc/doc/ _build/libmongoc-html

libbson-docs: poetry-install
	$(SPHINX_BUILD) --keep-going -W src/libbson/doc/ _build/libbson-html

libmongoc-docs-server: poetry-install
	$(SPHINX_AUTOBUILD) src/libmongoc/doc _build/libmongoc-html

libbson-docs-server: poetry-install
	$(SPHINX_AUTOBUILD) src/libbson/doc _build/libbson-html

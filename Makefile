# include env if present
ifneq (,$(wildcard .env))
  include .env
  export
endif
# include local env if present
ifneq (,$(wildcard .env.local))
  include .env.local
  export
endif

all: systemc toolchain

coverage:
	echo "No coverage defined"

git_init:
	git submodule init
	git submodule update
	test -d .venv ||python -m venv .venv
	. .venv/bin/activate ; pip install -r requirements.txt

clean: # cleans local build files
	rm -rf build

build:
	mkdir -p build
	cd build ; cmake -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" ..
	make -C build

.PHONY: snippets docs

# Builds the WAL++ WebAssembly module for the playground. Needs the Emscripten SDK.
wasm:
	bash build-wasm.sh

# Fetches the Surfer viewer bundle for the playground (~15 MB, not committed).
surfer:
	bash tools/fetch-surfer.sh

# Checks that every playground example still evaluates without an error.
check-examples:
	python3 tools/check-examples.py

# Decides which documentation snippets get a "Run in playground" button, by
# running them. Needs build/wal; skipped with a warning if it is not built yet.
snippets:
	@if [ -x build/wal ]; then python3 tools/mark-runnable-snippets.py; \
	else echo "note: build/wal missing, keeping the existing runnable.json"; fi

docs: snippets
	# The viewer bundle is not committed, so fetch it unless it is already here.
	# Without it the playground still ships, with the console working and the
	# viewer pane saying who needs to run what.
	@bash tools/fetch-surfer.sh || echo "warning: no Surfer bundle, the playground ships without the viewer"
	mkdir -p build/docs
	rsync -am \
	      --exclude="build" \
				--exclude="ext" \
				--exclude=".*" \
				--include='*.md' \
				--include='*.wasm' \
				--include='*.vcd' \
				--include='*.json' \
				--include='SURFER_VERSION.txt' \
				--include='/LICENSE' \
				--include='/CITATION.cff' \
				--include='/CITATION.bib' \
				--include='*.png' \
				--include="*.css" \
				--include="*.js" \
				--include="*.mjs" \
				--include="*.svg" \
				--include='*/' \
				--exclude='*' \
				./ build/docs/
	# The site has its own table of contents; drop the README's, which GitHub readers need.
	awk '/^## Table of Contents$$/{skip=1; next} skip && /^## /{skip=0} !skip' \
		build/docs/README.md > build/docs/README.md.tmp && mv build/docs/README.md.tmp build/docs/README.md
	bash build-docs.sh
#	mkdocs build
#	python3 docs/convert_headers.py --force --include-slash-comments

format:
	clang-format -i -- src/**/*.cpp src/**/*.tpp src/**/*.h

tidy:
	clang-tidy -p build --fix -- src/**/*.cpp src/**/*.tpp src/**/*.h

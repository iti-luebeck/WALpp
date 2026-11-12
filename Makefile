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

.PHONY: docs

docs:
	mkdir -p build/docs
	rsync -am \
	      --exclude="build" \
				--exclude="ext" \
				--exclude=".*" \
				--include='*.md' \
				--include='*.png' \
				--include="*.css" \
				--include="*.js" \
				--include="*.mjs" \
				--include="*.svg" \
				--include='*/' \
				--exclude='*' \
				./ build/docs/
	bash build-docs.sh
#	mkdocs build
#	python3 docs/convert_headers.py --force --include-slash-comments

format:
	clang-format -i -- src/**/*.cpp src/**/*.tpp src/**/*.h

tidy:
	clang-tidy -p build --fix -- src/**/*.cpp src/**/*.tpp src/**/*.h

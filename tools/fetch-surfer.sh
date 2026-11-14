#!/usr/bin/env bash
# Fetches the Surfer waveform viewer's WebAssembly build for the documentation playground.
#
# Surfer (https://gitlab.com/surfer-project/surfer, EUPL-1.2) is used unmodified: the
# playground drives it over WCP, the Waveform Control Protocol, which its wasm build
# exposes to JavaScript. Its releases ship native binaries only, so the browser build is
# taken from the project's own hosted demo.
#
# The bundle is ~15 MB and is NOT committed; it is fetched here and ignored by git.
# The checksums of what was fetched land in docs/playground/surfer/SURFER_VERSION.txt
# so a silently changed upstream build is at least visible in the diff of a rebuild.
#
# The playground degrades gracefully: without this bundle the WAL++ console still works
# and the viewer pane explains that it is missing.
set -euo pipefail
cd "$(dirname "$0")/.."

BASE_URL="${SURFER_BASE_URL:-https://app.surfer-project.org}"
DEST="docs/playground/surfer"
FILES="surfer.js surfer_bg.wasm integration.js"

if [ "${1:-}" = "--force" ]; then rm -rf "$DEST"; fi

if [ -f "$DEST/surfer_bg.wasm" ]; then
  echo "surfer bundle already present in $DEST (use --force to refetch)"
  exit 0
fi

mkdir -p "$DEST"
echo "==> fetching the Surfer wasm bundle from $BASE_URL"
for f in $FILES; do
  echo "    $f"
  curl -fsSL -o "$DEST/$f" "$BASE_URL/$f"
done

{
  echo "Surfer WebAssembly bundle for the WAL++ documentation playground."
  echo "Fetched: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "Source:  $BASE_URL"
  echo
  echo "Surfer is Copyright the Surfer contributors and licensed EUPL-1.2."
  echo "Project: https://gitlab.com/surfer-project/surfer"
  echo "It is redistributed here unmodified, alongside the documentation."
  echo
  echo "sha256:"
  for f in $FILES; do
    if command -v shasum > /dev/null; then
      (cd "$DEST" && shasum -a 256 "$f")
    else
      (cd "$DEST" && sha256sum "$f")
    fi
  done
} > "$DEST/SURFER_VERSION.txt"

echo "==> done"
cat "$DEST/SURFER_VERSION.txt"

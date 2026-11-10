#!/usr/bin/env bash
# Builds the documentation into build/site.
# Requires doxide (https://doxide.org) and the Python packages from
# requirements.txt (pip install -r requirements.txt).
set -euo pipefail
cd "$(dirname "$0")"
for tool in doxide mkdocs; do
  command -v "$tool" > /dev/null || { echo "error: $tool not found" >&2; exit 1; }
done
doxide build
mkdocs build

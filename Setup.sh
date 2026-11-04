#!/usr/bin/env bash
set -e

# Directory this script lives in
ROOT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Generate GNU Makefiles from premake.lua
"$ROOT_DIR/vendor/Premake/Linux/premake5" --file="$ROOT_DIR/premake.lua" gmake

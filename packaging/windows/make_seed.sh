#!/usr/bin/env bash
# A release package with a shader seed, in one command (MSYS2 CLANG64, after `bash build.sh`):
#   bash packaging/windows/make_seed.sh [cache folder]
# 1. packages dist/issn-windows (package.sh) with the executable just built;
# 2. runs a VERIFY boot of that executable on the cache folder (default: the one the dist copy
#    plays with, else user/cache/CUSA00223): every cached stage is made again from the game's own
#    files and compared word for word, the stamp written, and the game closes by itself before it
#    starts (SHADPS4_EXIT_AFTER_WARMUP); the launcher's saved game folder is used (--play);
# 3. packages again with the seed (tools/make_shader_seed.py: keys and metas, no shader code).
# The cache folder must sit in dist/issn-windows/user/cache: package.sh checks that the verify ran
# with the packaged issn-probe.exe. Nothing is built here, so the executable cannot change between
# the verify and the package.
set -euo pipefail
cd -- "$(dirname -- "$0")/../.."
cache_root=dist/issn-windows/user/cache
if [[ $# -ge 1 ]]; then
    source=${1%/}
elif [[ -f dist/issn-windows/shader-seed.txt ]] &&
    [[ -d $cache_root/$(tr -d '\r\n' < dist/issn-windows/shader-seed.txt) ]]; then
    source=$cache_root/$(tr -d '\r\n' < dist/issn-windows/shader-seed.txt)
else
    source=$cache_root/CUSA00223
fi
[[ -d $source ]] || { echo "No cache folder $source" >&2; exit 1; }
[[ $(cd -- "$(dirname -- "$source")" && pwd) == $(cd -- "$cache_root" && pwd) ]] ||
    { echo "The cache folder must be in $cache_root" >&2; exit 1; }
name=$(basename -- "$source")
echo "Shader seed from $source"

bash packaging/windows/package.sh

rm -f -- "$source/seed-stamp.bin"
echo 'Verifying the cache with the packaged executable (the game window opens and closes by itself)...'
SHADPS4_CACHE_NAME=$name SHADPS4_CACHE_REBUILD=10 SHADPS4_EXIT_AFTER_WARMUP=1 \
    "dist/issn-windows/inFAMOUS Second Son.exe" --play | grep -E 'CacheRebuild: stage|boot preload done|STOP' || true
[[ -f $source/seed-stamp.bin ]] || {
    echo "The verify wrote no seed-stamp.bin (see dist/issn-windows/user/last_run.log)" >&2
    exit 1
}

ISSN_SHADER_SEED_FROM=$source bash packaging/windows/package.sh
echo 'Done: dist/issn-windows.zip carries the shader seed (the "Shader seed:" line above has the count).'

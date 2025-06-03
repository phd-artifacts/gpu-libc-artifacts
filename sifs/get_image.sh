# this is an apptainer that I froze to build llvm
# for some reason this is not working on sorgan
set -e

CACHE_DIR=$(realpath .cache)
TMP_DIR="$CACHE_DIR/tmp"
mkdir -p "$TMP_DIR"
chmod -R u+rwx "$CACHE_DIR"

APPTAINER_NOHTTPS=1 \
APPTAINER_CACHEDIR="$CACHE_DIR" \
APPTAINER_TMPDIR="$TMP_DIR" \
SINGULARITY_CACHEDIR="$CACHE_DIR" \
apptainer pull --disable-cache --force docker://rodrigoceccato/ompc-base:latest

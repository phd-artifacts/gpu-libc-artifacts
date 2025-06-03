# this is an apptainer that I froze to build llvm
# for some reason this is not working on sorgan
set -e

# allow the caller to override cache directories
CACHE_DIR="${APPTAINER_CACHEDIR:-$(realpath .cache)}"
TMP_DIR="${APPTAINER_TMPDIR:-$CACHE_DIR/tmp}"

mkdir -p "$TMP_DIR"
chmod -R u+rwx "$CACHE_DIR"

APPTAINER_NOHTTPS=1 \
APPTAINER_CACHEDIR="$CACHE_DIR" \
APPTAINER_TMPDIR="$TMP_DIR" \
SINGULARITY_CACHEDIR="$CACHE_DIR" \
apptainer pull --disable-cache --force docker://rodrigoceccato/ompc-base:latest

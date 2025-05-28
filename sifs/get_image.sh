# this is an apptainer that I froze to build llvm
# for some reason this is not working on sorgan
mkdir -p $(realpath .cache)
chmod -R u+rwx $(realpath .cache)
APPTAINER_NOHTTPS=1 APPTAINER_CACHEDIR=$(realpath .cache) apptainer pull docker://rodrigoceccato/ompc-base:latest

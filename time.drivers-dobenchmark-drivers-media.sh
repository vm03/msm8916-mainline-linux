
make clean > /dev/null
sync

perf stat --repeat 3 -e instructions,cycles,cpu-clock --sync --pre 'make clean >/dev/null' make -j96 drivers/media/ >/dev/null


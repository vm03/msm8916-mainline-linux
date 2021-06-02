
make clean > /dev/null
sync

make -j kernel/sched/ >/dev/null
sync

perf stat --repeat 3 -e instructions,cycles,cpu-clock --sync --pre 'rm -f kernel/sched/*.o' make -j96 kernel/sched/ >/dev/null


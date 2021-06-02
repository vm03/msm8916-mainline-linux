
make mrproper >/dev/null
make-distroconfig >/dev/null
mo-enter >/dev/null
sync

perf stat --repeat 3 -e instructions,cycles,cpu-clock --sync --pre 'make clean >/dev/null' make -j96 drivers/ mm/ arch/x86/ net/ kernel/ fs/ ipc/ security/ lib/ virt/ block/ sound/ crypto/ init/ >/dev/null


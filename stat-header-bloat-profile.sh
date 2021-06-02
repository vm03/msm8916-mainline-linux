
#grep 'In file' e | sort | uniq -c | sort -n

grep -w from e | sort --parallel=32 | uniq -c | sort -n --parallel=32

#grep 'warning: #warning profile' e | cut -d: -f1 | sed 's/\.\///g' | sort | uniq -c | sort -n


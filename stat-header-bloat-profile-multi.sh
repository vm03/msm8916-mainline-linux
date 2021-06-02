
#grep 'In file' e | sort | uniq -c | sort -n

#grep -w from e | sort --parallel=32 | uniq -c | sort -n --parallel=32

OPTS="--parallel=16"

echo "# Sorting full header profile ..."

grep 'warning: #warning profile' e | cut -d: -f1 | sed 's/\.\///g' > e2
sort $OPTS e2 > e3
uniq -c e3 > e4
sort -n $OPTS e4 > e5

tail e5

ls -lh e5



FILE=stat-header-bloat-profile.sh.full.v67

# make sure tools are built:
./stat-header-bloat-i-files.sh >/dev/null 2>&1

HEADERS=$(tail -1500 $FILE | tac | cut -c9- )

# echo "headers: $HEADERS"

{
  echo

  echo '                                                               _______________________'
  echo '                                                              | stripped lines of code'
  echo '                                                              |             _____________________________'
  echo '                                                              |            | headers included recursively'
  echo '                                                              |            |                _______________________________'
  echo '                                                              |            |               | usage in a distro kernel build'
  echo ' ____________                                                 |            |               |        _________________________________________'
  echo '| header name                                                 |            |               |       | million lines of comment-stripped C code'
  echo '|                                                             |            |               |       |'

  for N in $HEADERS; do
    NN=$(echo $N | sed 's/arch\/x86\/include\// /' | sed 's/include\// /' | sed 's/ //g' )
    H="#include <$NN>"
    echo "# $H"
    echo "$H" > kernel/header_test_sched.h.c

    NR=$(grep -E " $N$" $FILE | cut -c1-8 | sed 's/ //g')
  #  echo "# N: {$N}, NN: {$NN}, NR: {$NR}"
    ./stat-header-bloat-i-files.sh "$NN" "$NR" 2>&1 | grep -v 'No such file or directory'

  done

} 2>&1 | tee e6

cat header.pretty > e6.pretty
cat e6 | grep -v '^#' | sort -t: -k +4 -n -r >> e6.pretty

ls -lh e6 e6.pretty

#!/bin/bash

seps()
{
  numfmt --grouping $1
}

function print_stats()
{
  local HEADER=$1
  local HEADER_NAME=$2
  local BASE=kernel/header_test_$HEADER


  [ "$HEADER_NAME" = "" ] && {
    HEADER_NAME=$(echo $HEADER | sed 's/sched_$/sched\//g')
    HEADER_NAME="<linux/$HEADER_NAME>"
  }

  rm -f $BASE.i
  make ARCH=$(kconfig-arch) EXTRA_CFLAGS='-P -H' $BASE.i >/dev/null 2>$BASE.i.headers

#  # run it twice, to make sure we only rebuild the .i, not tools:
#
#  rm -f $BASE.i
#  make ARCH=$(kconfig-arch) EXTRA_CFLAGS="-P -H" $BASE.i >/dev/null 2>$BASE.i.headers

  local NR=$(cat $BASE.i | wc -l)
#  local HEADERS_USED=$(cat $BASE.i.headers | grep -vE '^\./|^Multiple' | wc -l)

  cat $BASE.i.headers |
         grep -v 'Multiple include guards' |
	 sed -E 's/ +//g' |
	 sed -E 's/^\.+//g' |
	 sed -E 's/^\/+//g' |
	 sed -E 's/^\.+//g' |
	 sed -E 's/^\/+//g' |
	 sort | uniq > $BASE.i.headers.flat

  local HEADERS_USED=$(cat $BASE.i.headers.flat | wc -l)

  [ "$COUNT" = "" ] && COUNT=1

  [ "$COUNT" = "1" ] && {
    printf " #include %-30s | LOC: %6s | headers: %4s\n" $HEADER_NAME $(seps $NR) $(seps $HEADERS_USED)
  } || {
    MLOC=$(($COUNT*$NR/100000))
    BARS=$(($COUNT*$NR/4000000))

    printf "  #include %-50s | LOC: %6s | headers: %4s | %6s | MLOC: %4s.%s | %s\n" $HEADER_NAME $(seps $NR) $HEADERS_USED $(seps $COUNT) $(seps $(($MLOC/10))) $(($MLOC%10)) $(print-bars $BARS 60)
  }
}

[ ! -z "$*" ] && {

  [ "$#" != "1" -a "$#" != "2" ] && {
    echo '# usage: ./stat-header-bloat-i-files.sh <[include/]header_file_name> [count/weight]'
    exit -1
  }
  N="$1"
  N=$(echo $N | sed 's/^include\///g' | sed 's/^arch\/x86\/include\///g' | sed 's/^arch\/arm64\/include\///g' )
  H="#include <$N>"

  export COUNT="$2"
  #  echo "$H"
  echo "$H" > kernel/header_test_sched.h.c

  # escape the slashes:
  N_PATTERN=$(echo $N | sed 's/\//\\\//g')

  # echo "N_PATTERN: $N_PATTERN"

  print_stats sched.h "<$N>"
  echo "#include <linux/sched.h>" > kernel/header_test_sched.h.c

  exit 0
}

echo
echo '# .i build stats:'
echo '#'

print_stats sched.h

echo

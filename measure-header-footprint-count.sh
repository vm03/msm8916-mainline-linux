
[ "$#" != "1" ] && {
  echo '# usage: ./measure-header-footprint-count.sh measure-header-footprint.sh.vanilla.pretty'
  exit -1
}

echo $(cat $1 | grep MLOC | cut -d: -f4- | cut -d\| -f1)  | sed 's/ /+/g' | bc

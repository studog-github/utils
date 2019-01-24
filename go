#!/bin/sh

#
# History
#
# v1.01 Mar 14 2010 stu
# Added long options
#
# v1.00 Sep 03 2003 stu
# Built the first release from previous similar utilities
#

#
# Tunable parameters
#

# The program name will be prepended later
VERSION="v1.01 Mar 14 2010"

# How many old trees to keep around
PRUNE=20

# This list is searched in order for a target that will successfully clean 
# the source tree.
# mrproper - clean target for linux kernel source trees
# distclean - clean target for many free software packages
# really-clean - clean for the standalone serial driver
# clean - general last-hope target
TRYCLEAN="mrproper distclean really-clean clean"

#
# Functions
#

#
# This uses only shell builtins to set up the utilities for require() to use
# at which point other utilities can be require()ed. This includes setting
# up $basename and $THIS for die().
#
init_require() {
  which which > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "$0: Error: Required utility 'which' not found"
    exit 1
  fi
  which awk > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "$0: Error: Required utility 'awk' not found"
    exit 1
  fi
  which basename > /dev/null 2>&1
  if [ $? -ne 0 ]; then
    echo "$0: Error: Required utility 'basename' not found"
    exit 127
  fi
  which=`which which`
  awk=`which awk`
  basename=`which basename`
  THIS=`$basename $0`
}

die() {
  echo "$THIS: Error: $*"
  exit 1
}

require() {
  local util

  util=`$which $1`
  if [ $? -ne 0 ] || [ "$util" = "" ]; then
    die "Required utility '$1' not found"
  fi
  eval "$1=$util"
}

dstamp() {
  echo "`$date` $*"
}

warn() {
  echo "$THIS: Warning: $*"
}

#
# Main
#

init_require

require date
require pwd
require wc
require expr
require dirname
require ls
require tr
require cut
require sort
require tail
require rm
require cp
require mkdir
require make
require diff

while [ "$1" != "" ]; do
  case $1 in
    -h|--help)
      echo "$VERSION"
      echo
      echo "$THIS -h"
      echo "$THIS [-r] <source-tree> <patch-name>"
      echo
      echo "Specify the path to the source tree, and a name for the patch, and"
      echo "$THIS creates a new numbered tree and generates a patch for it."
      echo
      echo "There must exist a <source-tree>-0-original to start the process."
      echo
      echo "<source-tree>/../pat-<source-tree> will be created if necessary"
      echo "and is where the generated patches are stored."
      echo
      echo "-h, --help	Print this help."
      echo "-r, --redo	Delete the highest numbered patch and redo it."
      exit 0
      ;;
    -r|--redo)
      REDO=yes
      ;;
    *)
      if [ "$TREE" = "" ]; then
        TREE=$1
      elif [ "$NAME" = "" ]; then
        NAME=$1
      else
        die "Too many arguments"
      fi
  esac
  shift
done

[ "$TREE" = "" ] && die "Missing <source-tree>"
[ "$NAME" = "" ] && die "Missing <patch-name>"

[ ! -d "$TREE" ] && die "<source-tree> is not a directory"

cd $TREE
TREE=`$pwd`

[ "$TREE" = "/" ] && die "<source-tree> is /"

cd ..

TREEDIR=`$dirname $TREE`
TREEBASE=`$basename $TREE`
TREEBASELEN=`echo $TREEBASE | $wc -c`
TREEBASELEN=`$expr $TREEBASELEN + 1`
LAST=`$ls -d $TREEBASE-[0-9]* 2> /dev/null` ||
                                            die "No $TREEBASE-0-* in $TREEDIR"
LAST=`echo $LAST | $tr -s [:space:] "\n" | $cut -c$TREEBASELEN- | $sort -n | $tail -1 | $cut -d- -f1`

if [ "$REDO" != "" ]; then
  [ $LAST -eq 0 ] && die "Nothing to redo"
  NEXT=$LAST
  LAST=`expr $LAST - 1`
  dstamp "Removing old files"
  $rm -rf $TREEBASE-$NEXT-*
  $rm -rf pat-$TREEBASE/patch-$NEXT-*
else
  NEXT=`expr $LAST + 1`
fi

dstamp "Copying source tree"
$cp -pr $TREEBASE $TREEBASE-$NEXT-$NAME

dstamp "Cleaning source tree"
cd $TREEBASE-$NEXT-$NAME
for i in $TRYCLEAN; do
  $make -n $i > /dev/null 2>&1
  if [ $? -eq 0 ]; then
    CLEAN=$i
    break
  fi
done
if [ "$CLEAN" = "" ]; then
  warn "Couldn't find a suitable target for cleaning, skipping"
else
  $make $CLEAN > /dev/null 2>&1
fi
cd ..

TREE_LAST=`$ls -d $TREEBASE-$LAST-*`

[ ! -d "pat-$TREEBASE" ] && $mkdir pat-$TREEBASE

dstamp "Diffing"
if [ -f Xd-$TREEBASE ]; then
  $diff -Naurp -X Xd-$TREEBASE $TREE_LAST $TREEBASE-$NEXT-$NAME > pat-$TREEBASE/patch-$NEXT-$NAME
else
  $diff -Naurp $TREE_LAST $TREEBASE-$NEXT-$NAME > pat-$TREEBASE/patch-$NEXT-$NAME
fi

# -gt in this check means that the -0- version of the code will *not* be
# pruned automatically, which is good.
if [ $NEXT -gt $PRUNE ]; then
  OLD=`expr $NEXT - $PRUNE`
  dstamp "Removing ancient tree"
  $rm -rf $TREEBASE-$OLD-*
fi

dstamp "Done"
$ls -alF pat-$TREEBASE/patch-$NEXT-$NAME

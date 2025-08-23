#!/bin/bash

DIR=$1
FILES="bit_flips_search.csv bit_flips_random_analysis.csv bit_flips_combined_analysis.csv main.log stdout.log"

echo "$@"

if [ -z $DIR ]; then
  echo "please specify the directory for storing logs."
  exit 1
fi

if [ ! -d $DIR ]; then
  mkdir $DIR
elif [ ! -z $(ls -A $DIR) ]; then
  echo "$DIR contains files. aborting."
  exit 1
fi

RUNTIME=1800
SEED=1234

function run {
  local IDENT=$(echo "$@" | tr -d "-" | tr " " "-")
  local TARGET="$DIR/$IDENT"
  if [ ! -d $TARGET ]; then
    mkdir $TARGET
  fi

  echo "-f -r $RUNTIME $@" >> "$TARGET/flags.txt"
  
  ./multithread_hammer -s $SEED -fr -fc --fence-type lfence -fl 32 -i -r $RUNTIME $@ | tee "main.log"

  RC=$?
  
  for f in $FILES; do
    if [ ! -f $f ]; then
      continue
    fi

    mv $f $TARGET
  done
  
  if [ $RC -ne 0 ]; then
    echo "something went wrong."
    touch "$TARGET/error.mark"
  fi
}

for p in {1..3}; do
  for d in {1..3}; do
    run -id $d -ic 1 -ip $p -t 1 -l 1
    run -id $d -ic 2 -ip $p -t 1 -l 1
    run -id $d -ic 4 -ip $p -t 1 -l 1
    run -id $d -ic 1 -ip $p -t 1 -l 1 --scheduling default,none
    run -id $d -ic 2 -ip $p -t 1 -l 1 --scheduling default,none
    run -id $d -ic 4 -ip $p -t 1 -l 1 --scheduling default,none
  done;
done;


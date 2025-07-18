#!/bin/bash

DIR=$1
FILES="bit_flips_search.csv bit_flips_analysis.csv main.log stdout.log"

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

RUNTIME=2700

function run {
  local IDENT=$1
  shift
  local TARGET="$DIR/$IDENT"
  if [ ! -d $TARGET ]; then
    mkdir $TARGET
  fi

  echo "-f -r $RUNTIME $@" >> "$TARGET/flags.txt"
  
  ./multithread_hammer -fr -fc -r $RUNTIME $@ | tee "main.log"

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

# 1 thread
run "1-thread" -t 1 -l 1
# 1 thread, omit fencing
run "1-thread-omit" -t 1 -l 1 --fencing-strategy omit
# 4 threads, omit fencing
run "4-thread-omit" -t 4 -l 1 --fencing-strategy omit
# 1 thread, different core
run "1-thread-core-12" -t 1 -l 1 --thread 12
# 2 threads
run "2-threads" -t 2 -l 1
# 4 threads
run "4-threads" -t 4 -l 1
# 1 thread
run "1-thread-random" -t 1 -l 1 --randomize-each
# 4 threads
run "4-thread-random" -t 4 -l 1 --randomize-each
# 4 threads
run "4-thread-core-12" -t 4 -l 1 --thread 12
# 2 threads with simple pattern on support thread
run "2-thread-simple-support" -t 2 -l 1 --simple false,true
# 2 threads, only one fencing
run "2-thread-first-fence" -t 2 -l 1 --scheduling default,none
# 2 threads, no fencing
run "2-thread-no-fence" -t 2 -l 1 --scheduling none
# 1 threads, lfence
run "thread-lfence" -t 1 -l 1 --fence-type lfence
# 1 threads, sfence
run "thread-sfence" -t 1 -l 1 --fence-type sfence
# 1 threads, lfence
run "4-thread-lfence" -t 4 -l 1 --fence-type lfence
# 1 threads, sfence
run "4-thread-sfence" -t 4 -l 1 --fence-type sfence
# 1 threads, simple
run "thread-simple" -t 1 -l 1 --simple true
# 1 threads, simple, lfence
run "thread-simple-lfence" -t 1 -l 1 --simple true --fence-type lfence
# 2 threads, simple
run "2-threads-simple" -t 2 -l 1 --simple true
# 2 threads, simple, lfence
run "2-simple-lfence" -t 2 -l 1 --simple true --fence-type lfence
# 2 threads, simple, sfence
run "2-simple-sfence" -t 2 -l 1 --simple true --fence-type sfence
# 1 pattern on main thread
run "interleaved" -i -t 1 -l 1
# 1 pattern on main thread
run "interleaved-core-12" -i -t 1 -l 1 --thread 12
# 2 interleaved patterns
run "2-interleaved-first-fence" -i -t 2 -l 1 --scheduling default,none
# 2 interleaved patterns with distance of 6
run "2-interleaved-dist-6" -i -t 2 -l 1 --interleave-distance 6 --scheduling default,none
# interleave simple
run "2-interleaved-simple" -i -t 2 -l 1 --scheduling default,none --simple true
# interleave complex with simple
run "2-interleaved-complex-main" -i -t 2 -l 1 --scheduling default,none --simple false,true
# interleave complex 4 times
run "4-interleaved" -i -t 4 -l 1 --scheduling default,none --simple false
# interleave complex 4 times, single pair
run "4-interleaved-single"-i -t 4 -l 1 --scheduling default,none --simple false --interleave-single-pairs
# interleave simple 4 times, single pair
run "4-interleaved-single-simple" -i -t 4 -l 1 --scheduling default,none --simple true --interleave-single-pairs
# interleave simple 4 times, no fence
run "4-interleaved-simple-nofence-single" -i -t 4 -l 1 --scheduling none --simple true --interleave-single-pairs
# interleave simple 4 times, no fence
run "4-interleaved-simple-nofence" -i -t 4 -l 1 --scheduling none --simple true
# interleave simple 4 times, no fence
run "4-interleaved-complex" -i -t 4 -l 1 --scheduling none --simple false
# 1 thread, omit fencing
run "1-interleaved-omit" -i -t 1 -l 1 --fencing-strategy omit
# 4 threads, omit fencing
run "4-interleaved-omit" -i -t 4 -l 1 --fencing-strategy omit
# 1 thread, omit fencing
run "1-interleaved-lfence" -i -t 1 -l 1 --fence-type lfence
# 1 thread, omit fencing
run "1-interleaved-sfence" -i -t 1 -l 1 --fence-type sfence
# 4 threads, omit fencing
run "4-interleaved-lfence" -i -t 4 -l 1 --fence-type lfence
# 4 threads, omit fencing
run "4-interleaved-sfence" -i -t 4 -l 1 --fence-type sfence
# 1 thread, omit fencing
run "1-interleaved-lfence-simple" -i -t 1 -l 1 --fence-type lfence --simple false,true
# 1 thread, omit fencing
run "1-interleaved-sfence-simple" -i -t 1 -l 1 --fence-type sfence --simple false,true
# 4 threads, omit fencing
run "4-interleaved-lfence-simple" -i -t 4 -l 1 --fence-type lfence --simple false,true
# 4 threads, omit fencing
run "4-interleaved-sfence-simple" -i -t 4 -l 1 --fence-type sfence --simple false,true


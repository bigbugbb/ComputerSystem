#!/bin/sh

if [ "$1" = "-v" ] ; then
    verbose=true
fi

# unique disk names
for i in 1 2 3 4 5; do
    disks="$disks /tmp/$USER-disk$i.img"
done

# use 'dd' to create disk images, each with 256 512-byte blocks
for d in $disks; do
    # note that 2<&- suppresses dd info messages
    dd if=/dev/zero bs=512 count=1024 of=$d 2<&- 
done

overall=SUCCESS
test=
first=1

# run tests across different numbers of disks
#
for d in $disks; do
    # run tests across different stripe sizes
    #
    test="$test $d"
    if [ "$first" ] ; then	# need at least two disks in the list
	first=
	continue
    fi

    for s in 8 16 30 64; do
	echo testing $(echo $test| wc -w) disks, stripe size $s blocks
	./stripe-test $s $test
	if [ $? -ne 0 ] ; then overall=FAILED; fi
	if [ $overall = FAILED -a "$verbose" ] ; then
	    echo failed on: 	./stripe-test $s $test
	    exit
	fi
    done
done

# check that blocks beyond end aren't modified
# first add 7 blocks of zeros to the end of each file
#
for d in $disks; do
    dd if=/dev/zero bs=512 count=7 2<&- >> $d
done

# then run a test that writes to the end
#
./stripe-test 64 $disks
if [ $? -ne 0 ] ; then overall=FAILED; fi

# and finally grab the last 7 blocks of each file, delete the zeros
# (with 'tr -d'), and check to see if there are more than 0 bytes
# left.
#
for d in $disks; do
    dd if=$d bs=512 skip=1024 2<&-
done | tr -d '\0' | wc -c | while read bytes; do
    if [ $bytes -gt 0 ] ; then
	echo ERROR: garbage added to end of disks
	overall=FAILED
    fi
done

echo Overall test: $overall
rm -f $disks

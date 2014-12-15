#!/bin/sh

while [ "$1" ] ; do
    case $1 in
    -v) verbose=true;;
    -create) create=true;;
    -cmds) cmds=true;;
    esac
    shift
done

# unique disk names
for i in 1 2 3 4 5; do
    disks="$disks /tmp/$USER-disk$i.img"
done
d4=${disks% *}          # all but the last disk

# use 'dd' to create disk images, each with 256 512-byte blocks
[ "$cmds" ] || for d in $disks; do
    # note that 2<&- suppresses dd info messages
    dd if=/dev/zero bs=512 count=1024 of=$d 2<&- 
done
if [ "$create" ] ; then
    echo disks created:
    echo $disks
    exit 1
fi

overall=SUCCESS
if [ "$cmds" ] ; then
    ECHO=echo
fi

# run tests across different stripe sizes
#
for d in "$d4" "$disks"; do
    for s in 4 16 20; do
    echo testing RAID4 on $(echo $d | wc -w) drives with stripe size $s blocks
    $ECHO ./raid4-test $s $d
    if [ $? -ne 0 ] ; then overall=FAILED; fi
    if [ $overall = FAILED -a "$verbose" ] ; then
        echo failed on:     ./raid4-test $s $d
        exit
    fi
    done
done

# check that blocks beyond end aren't modified
# first add 7 blocks of zeros to the end of each file
#
echo "Testing for writes past end of volume"
for d in $disks; do
    [ "$cmds" ] || dd if=/dev/zero bs=512 count=7 2<&- >> $d
done

# then run a test that writes to the end
#
$ECHO ./raid4-test 64 $disks
if [ $? -ne 0 ] ; then overall=FAILED; fi

# and finally grab the last 7 blocks of each file, delete the zeros
# (with 'tr -d'), and check to see if there are more than 0 bytes
# left.
#
[ "$cmds" ] || for d in $disks; do
    dd if=$d bs=512 skip=1024 2<&-
done | tr -d '\0' | wc -c | while read bytes; do
    if [ $bytes -gt 0 ] ; then
    echo ERROR: garbage added to end of disks
    overall=FAILED
    fi
done

echo Overall test: $overall
$ECHO rm -f $disks

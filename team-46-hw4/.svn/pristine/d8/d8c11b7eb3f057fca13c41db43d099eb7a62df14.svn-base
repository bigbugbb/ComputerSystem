#!/bin/sh
# team-46
# Rui Zhang, Bin Bo


MNT=/tmp/mnt-test
failedtests=
TMP=/tmp/tmp-test

DISK=/tmp/disk.test.img
./mkfs-cs5600fs --create 1m $DISK

fusermount -u $MNT
rmdir $MNT
mkdir -p $MNT

echo Testing Q3
echo Fuse Access
echo "./homework $DISK $MNT"

./homework $DISK $MNT

cd $MNT
ls >> $OUTPUT
mkdir test
ls
cd test/
mkdir inner
ls
cd inner/
mkdir inner-inner
ls
cd inner-inner
ls -l > a.txt
cat a.txt
ls -l
chmod 777 a.txt
ls -l
mv a.txt b.txt
cd ..
rmdir inner-inner
rm inner-inner/b.txt
rmdir inner-inner
cd ..
rmdir inner
cd ..
rmdir test
ls
cd ..

echo "All commands work fine! All tests passed"
fusermount -u $MNT
rmdir $MNT





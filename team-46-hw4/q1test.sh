#!/bin/sh
# team-46
# Rui Zhang, Bin Bo

# compile the homework.c
sh compile.sh

# testing variables
img=disk1.img.gz
cmd=./homework
disk=disk_test1.img
output=./q1-test-output

# delete the leftover from last run
rm -f $disk
rm -f $output

# make a copy of the disk to play with
cp $img $disk.gz
gzip -d $disk.gz

echo Testing Q1
echo read only
echo ./homework --cmdline $disk

# feed the commands to ./homework
$cmd --cmdline $disk > $output <<COMMANDS
ls
ls file.txt
cd home
ls
ls big-1
pwd
ls-l big-1
show big-1
cd ..
ls-l home
statfs
show home/small-2
cd work/dir-2
ls
cd ..
ls
cd dir-1
ls
statfs
cd /work/dir-1
blksiz 17
show small-3
blksiz 1024
show small-3
blksiz 4000
show small-3
COMMANDS
echo >> $output

# diff with the output from running the
# commands against the provided ./q1-soln
diff - $output <<OUTPUT
read/write block size: 1000
cmd> ls
another-file
dir_other
file.txt
home
work
cmd> ls file.txt
error: Not a directory
cmd> cd home
cmd> ls
big-1
small-1
small-2
cmd> ls big-1
error: Not a directory
cmd> pwd
/home
cmd> ls-l big-1
/home/big-1 -rw-r--r-- 1200 2
cmd> show big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
big-1 big-1 big-1 big-1 big-1 big-1 big-1 big-1
cmd> cd ..
cmd> ls-l home
big-1 -rw-r--r-- 1200 2
small-1 -rw-r--r-- 216 1
small-2 -rw-r--r-- 144 1
cmd> statfs
max name length: 43
block size: 1024
cmd> show home/small-2
small-2 small-2 small-2 small-2 small-2 small-2 small-2 small-2 small-2
small-2 small-2 small-2 small-2 small-2 small-2 small-2 small-2 small-2
cmd> cd work/dir-2
cmd> ls
cmd> cd ..
cmd> ls
dir-1
dir-2
cmd> cd dir-1
cmd> ls
small-3
small-4
small-5
cmd> statfs
max name length: 43
block size: 1024
cmd> cd /work/dir-1
cmd> blksiz 17
read/write block size: 17
cmd> show small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3
cmd> blksiz 1024
read/write block size: 1024
cmd> show small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3
cmd> blksiz 4000
read/write block size: 4000
cmd> show small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3 small-3
small-3 small-3
cmd> 
OUTPUT

if [ $? != 0 ] ; then
    echo Something went wrong, tests failed
else
    echo All tests passed!
fi

# clean up after test
rm -f disk_test1.img
rm -f q1-test-output

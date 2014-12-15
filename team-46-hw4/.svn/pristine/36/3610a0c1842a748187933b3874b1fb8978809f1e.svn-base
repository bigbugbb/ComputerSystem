#!/bin/sh
# team-46
# Rui Zhang, Bin Bo

# compile the homework.c
sh compile.sh

# testing variables
img=disk1.img.gz
cmd=./homework
disk=disk_test2.img
output=./q2-test-output
input_small=./q2-input-small
input_big=./q2-input-big

# delete the leftover from last run
rm -f $disk
rm -f $output
rm -f $input_small
rm -f $input_big

# make a copy of the disk to play with
cp $img $disk.gz
gzip -d $disk.gz

# prepare a file with "hello" for write tests
yes 'hello' | head -30 | fmt > $input_small
yes 'world' | head -500 | fmt > $input_big

echo Testing Q2
echo read/write
echo ./homework --cmdline $disk

# feed the commands to ./homework
$cmd --cmdline $disk > $output <<COMMANDS
ls
mkdir test
cd test
put ./q2-input-small q2-small.txt
show
show q2-small.txt
rm q2-small.txt
ls
cd ..
rmdir test
mkdir test
cd test
put ./q2-input-small q2-small.txt
put ./q2-input-big q2-big.txt
ls-l
show q2-big.txt
mkdir inner
cd inner
put ./q2-input-small inner.txt
ls
cd ..
show inner/inner.txt
rm inner/inner.txt
cd /
ls
cd test
ls
rmdir inner
cd ..
ls
ls-l test
rmdir test
blksiz 17
put ./q2-input-small size17.txt
blksiz 1024
put ./q2-input-small size1024.txt
blksiz 4000
put ./q2-input-small size4000.txt
ls-l size17.txt
ls-l size1024.txt
ls-l size4000.txt
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
cmd> mkdir test
cmd> cd test
cmd> put ./q2-input-small q2-small.txt
cmd> show
bad command: show
cmd> show q2-small.txt
hello hello hello hello hello hello hello hello hello hello hello hello
hello hello hello hello hello hello hello hello hello hello hello hello
hello hello hello hello hello hello
cmd> rm q2-small.txt
cmd> ls
cmd> cd ..
cmd> rmdir test
cmd> mkdir test
cmd> cd test
cmd> put ./q2-input-small q2-small.txt
cmd> put ./q2-input-big q2-big.txt
cmd> ls-l
q2-big.txt -rwxrwxrwx 3000 3
q2-small.txt -rwxrwxrwx 180 1
cmd> show q2-big.txt
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world world world world world
world world world world world world world world
cmd> mkdir inner
cmd> cd inner
cmd> put ./q2-input-small inner.txt
cmd> ls
inner.txt
cmd> cd ..
cmd> show inner/inner.txt
hello hello hello hello hello hello hello hello hello hello hello hello
hello hello hello hello hello hello hello hello hello hello hello hello
hello hello hello hello hello hello
cmd> rm inner/inner.txt
cmd> cd /
cmd> ls
another-file
dir_other
file.txt
home
test
work
cmd> cd test
cmd> ls
inner
q2-big.txt
q2-small.txt
cmd> rmdir inner
cmd> cd ..
cmd> ls
another-file
dir_other
file.txt
home
test
work
cmd> ls-l test
q2-big.txt -rwxrwxrwx 3000 3
q2-small.txt -rwxrwxrwx 180 1
cmd> rmdir test
error: Directory not empty
cmd> blksiz 17
read/write block size: 17
cmd> put ./q2-input-small size17.txt
cmd> blksiz 1024
read/write block size: 1024
cmd> put ./q2-input-small size1024.txt
cmd> blksiz 4000
read/write block size: 4000
cmd> put ./q2-input-small size4000.txt
cmd> ls-l size17.txt
/size17.txt -rwxrwxrwx 180 1
cmd> ls-l size1024.txt
/size1024.txt -rwxrwxrwx 180 1
cmd> ls-l size4000.txt
/size4000.txt -rwxrwxrwx 180 1
cmd> 
OUTPUT

if [ $? != 0 ] ; then
    echo 'Something went wrong, tests failed'
else
    echo 'All tests passed!'
fi

# clean up after test
rm -f $disk
rm -f $output
rm -f $input_small
rm -f $input_big
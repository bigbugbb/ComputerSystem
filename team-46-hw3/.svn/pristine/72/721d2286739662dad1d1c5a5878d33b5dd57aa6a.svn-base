#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "blkdev.h"

/*
 * helper function to verify the content is the same as in buffer
 */
int verify_content(struct blkdev *dev, char *buffer){
    int i, result;
    int nblks = dev->ops->num_blocks(dev);
    void *read_buffer;
    read_buffer = malloc(16 * 512);
    for(i = 0; i < nblks; i = i + 16)
    {
        result = dev->ops->read(dev, i, 16, read_buffer);
        assert(SUCCESS == result);

        int buffer_compare = memcmp(buffer, read_buffer, 512*16);
        assert(buffer_compare == 0);
    }
    return 0;
}
/* example main() function that takes several disk image filenames as
 * arguments on the command line.
 * Note that you'll need three images to test recovery after a failure.
 */
int main(int argc, char **argv)
{
    extern int image_devs_open;
    /* create the underlying blkdevs from the images
     */
    struct blkdev *d1 = image_create(argv[1]);
    struct blkdev *d2 = image_create(argv[2]);

    /**********  TEST START ***************/

    /* create image blkdevs from files 1 and 2
     */
    printf("Create Test: Creating mirror device\n");
    struct blkdev *disks[] = {d1, d2};
    struct blkdev *mirror = mirror_create(disks);

    /* create mirror volume from the two blkdevs
     */
    assert(mirror != NULL);
    assert(mirror->ops->num_blocks(mirror) == d1->ops->num_blocks(d1));
    printf("Create Test: Successfully created a mirror device\n");

    /* write all 512 blocks using a 16-block buffer, writing at offsets 0, 16, ...
     * Verify return=success after each write.
     */
    int result;
    printf("Write Test: Writing all 256 blocks with 'A'\n");
    int i=0;
    char *buffer = malloc(16 * BLOCK_SIZE);
    memset(buffer, 'A', 16 * BLOCK_SIZE);

    for(i = 0; i < 256; i = i + 16)
    {
        result = mirror->ops->write(mirror, i, 16, buffer);
        assert(SUCCESS==result);
    }
    printf("Write Test: Successfully write 256 blocks\n");

    /* read all 512 blocks, again 16 at a time.
     * verify result=success and correct data contents on each one.
     */
    printf("Verify Content Test: Verifying the content written to the device\n");
    assert(verify_content(d1, buffer) == 0);
    assert(verify_content(d2, buffer) == 0);
    assert(verify_content(mirror, buffer) == 0);
    printf("Verify Content Test: Content verified\n");

    /* Fail d1 */
    printf("Fail Test: Force failing the first side of mirror\n");
    image_fail(d1);
    void *temp;
    temp = malloc(16 * 512);
    assert(d1->ops->read(d1, 0, 1, temp) == E_UNAVAIL);
    mirror->ops->read(mirror, 0, 1, temp); // this will trigger close operation on failed disk.
    printf("Fail Test: The first side is not available now \n");

    /* read all 512 blocks, again 16 at a time.
     * verify result=success and correct data contents on each one.
     */
    printf("Read Other Side Test: Read data from the other side\n");
    assert(verify_content(d2, buffer) == 0);
    assert(verify_content(mirror, buffer) == 0);
    printf("Read Other Side Test: Content from other side of mirror verified\n");

    /* create image(disk3), replace(0, image3) */
    printf("Mirror Replace Test: Replace the failed side of mirror with a new disk \n");
    struct blkdev *d3 = image_create(argv[3]);
    result = mirror_replace(mirror, 0, d3);
    assert(result==SUCCESS);
    printf("Mirror Replace Test: Replace successfully \n");

    /* read all 512 blocks, verify result=success and correct contents */
    printf("Verify Replace Test: Verify the disk is correctly replaced\n");
    assert(verify_content(d2, buffer) == 0);
    assert(verify_content(d3, buffer) == 0);
    assert(verify_content(mirror, buffer) == 0);
    printf("Verify Replace Test: The disk is correctly replaced\n");

    /* Fail disk 2 */
    printf("Fail Test: Force failing the other side of mirror\n");
    image_fail(d2);
    assert(d2->ops->read(d2, 0, 1, temp) == E_UNAVAIL);
    mirror->ops->read(mirror, 0, 1, temp); // this will trigger close operation on failed disk.
    printf("Fail Test: The other side is not available now \n");

    /* write all 512 blocks with different data */
    printf("Write Test: Writing all 256 blocks with 'B' with the other side failed\n");
    memset(buffer, 'B', 16 * BLOCK_SIZE);
    for(i = 0; i < 256; i = i + 16)
    {
        result = mirror->ops->write(mirror, i, 16, buffer);
        assert(result==SUCCESS);
    }
    assert(verify_content(mirror, buffer) == 0);
    printf("Write Test: Successfully write 256 blocks\n");

    /*  create image(disk4), replace(1, image4) */
    printf("Mirror Replace Test: Replace the failed side of mirror with a new disk \n");
    struct blkdev *d4 = image_create(argv[4]);
    result = mirror_replace(mirror, 1, d4);
    assert(result==SUCCESS);
    printf("Mirror Replace Test: Replace successfully \n");

    /*  read all 512 blocks, verify result=success and correct contents */
    printf("Verify Replace Test: Verify the disk is correctly replaced\n");
    assert(verify_content(d3, buffer) == 0);
    assert(verify_content(d4, buffer) == 0);
    assert(verify_content(mirror, buffer) == 0);
    printf("Verify Replace Test: The disk is correctly replaced\n");

    /*  fail disk3  */
    printf("Fail Test: Force failing the first side of mirror\n");
    image_fail(d3);
    assert(d3->ops->read(d3, 0, 1, temp) == E_UNAVAIL);
    mirror->ops->read(mirror, 0, 1, temp); // this will trigger close operation on failed disk.
    printf("Fail Test: The first side is not available now \n");

    /*  read all 512 blocks, verify result=success and correct contents */
    printf("Read & Verify Test: Read and verify the mirror with first side failed \n");
    assert(verify_content(d4, buffer) == 0);
    assert(verify_content(mirror, buffer) == 0);
    printf("Read & Verify Test: Successfully read from the other side\n");

    /*  check that image1, image2, and image3 were closed properly: */
    printf("Number of open disk: %d\n", image_devs_open);
    assert(image_devs_open == 1);

    printf("Mirror Test: SUCCESS\n");
    return 0;
}

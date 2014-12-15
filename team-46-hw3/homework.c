/*
 * file:        homework.c
 * description: skeleton code for CS 5600 Homework 3
 * mirror and stripe parts of code is not written by bigbug
 * 
 * bigbug, Northeastern Computer Science, 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "blkdev.h"

/********** MIRRORING ***************/

/* example state for mirror device. See mirror_create for how to
 * initialize a struct blkdev with this.
 */
struct mirror_dev {
    struct blkdev *disks[2];    /* flag bad disk by setting to NULL */
    int nblks;
};

static int mirror_num_blocks(struct blkdev *dev)
{
    // extract the mirror_dev from the blkdev first
    struct mirror_dev *mdev = dev->private;
    return mdev->nblks;
}

/* read from one of the sides of the mirror. (if one side has failed,
 * it had better be the other one...) If both sides have failed,
 * return an error.
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should close the
 * device and flag it (e.g. as a null pointer) so you won't try to use
 * it again. 
 */
static int mirror_read(struct blkdev * dev, int first_blk,
        int num_blks, void *buf)
{
    // extract the mirror_dev from the blkdev first
    struct mirror_dev *mdev = dev->private;
    // two sides of the mirror
    struct blkdev *disk_0 = mdev->disks[0];
    struct blkdev *disk_1 = mdev->disks[1];

    int result_first = E_UNAVAIL, result_other = E_UNAVAIL;
    //try to read the disk_0 first
    if(disk_0 != NULL){
        result_first = disk_0->ops->read(disk_0, first_blk, num_blks, buf);
        if(result_first == E_BADADDR){
            return result_first;
        }
        if(result_first == E_UNAVAIL){
            // close disk_0 and flag it NULL
            printf("closing disk_0 \n");
            disk_0->ops->close(disk_0);
            mdev->disks[0] = NULL;
        }
    }

    // disk_0 failed, now we try the disk_1
    if(disk_1 != NULL){
        result_other = disk_1->ops->read(disk_1, first_blk, num_blks, buf);
        if(result_other == E_UNAVAIL){
            // close disk_1 and flag it NULL
            printf("closing disk_1 \n");
            disk_1->ops->close(disk_1);
            mdev->disks[1] = NULL;
        }
    }

    if(result_first == SUCCESS || result_other == SUCCESS){
        return SUCCESS;
    }
    // both disk_0 and disk_1 failed
    return E_UNAVAIL;
}

/* write to both sides of the mirror, or the remaining side if one has
 * failed. If both sides have failed, return an error.
 * Note that a write operation may indicate that the underlying device
 * has failed, in which case you should close the device and flag it
 * (e.g. as a null pointer) so you won't try to use it again.
 */
static int mirror_write(struct blkdev * dev, int first_blk,
        int num_blks, void *buf)
{
    // extract the mirror_dev from the blkdev first
    struct mirror_dev *mdev = dev->private;
    // two sides of the mirror
    struct blkdev *disk_0 = mdev->disks[0];
    struct blkdev *disk_1 = mdev->disks[1];

    int result_first = E_UNAVAIL, result_other = E_UNAVAIL;
    //try to write the disk_0 first
    if(disk_0 != NULL){
        result_first = disk_0->ops->write(disk_0, first_blk, num_blks, buf);
        // if E_BADADDR returned, no need to try on the other disk
        if(result_first == E_BADADDR){
            return result_first;
        }
        if(result_first == E_UNAVAIL){
            // close disk_0 and flag it NULL
            disk_0->ops->close(disk_0);
            mdev->disks[0] = NULL;
        }
    }

    // either disk_0 failed or wirte to disk_0 succeeded, we now write to disk_1
    if(disk_1 != NULL){
        result_other = disk_1->ops->write(disk_1, first_blk, num_blks, buf);
        if(result_other == E_UNAVAIL){
            // close disk_1 and flag it NULL
            disk_1->ops->close(disk_1);
            mdev->disks[1] = NULL;
        }
    }

    if(result_first == SUCCESS || result_other == SUCCESS){
        return SUCCESS;
    }

    return E_UNAVAIL;
}

/* clean up, including: close any open (i.e. non-failed) devices, and
 * free any data structures you allocated in mirror_create.
 */
static void mirror_close(struct blkdev *dev)
{
    struct mirror_dev *mdev = dev->private;
    struct blkdev *disk_0 = mdev->disks[0];
    struct blkdev *disk_1 = mdev->disks[1];

    // close all disks
    disk_0->ops->close(disk_0);
    disk_1->ops->close(disk_1);

    // free all memory allocated
    free(mdev);
    free(dev);
}

struct blkdev_ops mirror_ops = {
        .num_blocks = mirror_num_blocks,
        .read = mirror_read,
        .write = mirror_write,
        .close = mirror_close
};

/* create a mirrored volume from two disks. Do not write to the disks
 * in this function - you should assume that they contain identical
 * contents. 
 */
struct blkdev *mirror_create(struct blkdev *disks[2])
{
    // check the size of given two disks, if not the same size, return NULL
    int disk_size = disks[0]->ops->num_blocks(disks[0]);
    if(disks[1]->ops->num_blocks(disks[1]) != disk_size){
        printf("error: given two disk have different sizes");
        return NULL;
    }

    struct blkdev *dev = malloc(sizeof(*dev));
    struct mirror_dev *mdev = malloc(sizeof(*mdev));

    // copy the given disks to mdev's disks,
    // and set the number of blocks according to any of the disks.
    mdev->disks[0] = disks[0];
    mdev->disks[1] = disks[1];
    mdev->nblks = disk_size;

    dev->private = mdev;
    dev->ops = &mirror_ops;

    return dev;
}

/* replace failed device 'i' (0 or 1) in a mirror. Note that we assume
 * the upper layer knows which device failed. You will need to
 * replicate content from the other underlying device before returning
 * from this call.
 */
int mirror_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    struct mirror_dev *mdev = volume->private;
    struct blkdev *good_disk = mdev->disks[1-i]; // get the non-failed disk
    int disk_size = good_disk->ops->num_blocks(good_disk);

    if(newdisk->ops->num_blocks(newdisk) != disk_size){
        return E_SIZE;
    }

    // create a buffer to store the data from the good disk
    char *buf;
    buf = malloc((size_t) (BLOCK_SIZE * disk_size));
    // read the data to buffer from the good disk
    good_disk->ops->read(good_disk, 0, disk_size, buf);
    // store the data to the newdisk from the buffer
    newdisk->ops->write(newdisk, 0, disk_size, buf);
    // replace the failed disk with new disk
    mdev->disks[i] = newdisk;

    return SUCCESS;
}

/**********  STRIPING ***************/
/* example state for stripe device.
 */
struct stripe_dev {
    struct blkdev **disks;    /* flag bad disk by setting to NULL */
    int nblks;                  /* number of blocks stripe device holds */
    int ndisks;                 /* number of disks in stripe device */
    int stripe_size;               /* number of blocks in a stripe */
};

int stripe_num_blocks(struct blkdev *dev)
{
    struct stripe_dev *sdev = dev->private;
    return sdev->nblks;
}

/* read blocks from a striped volume. 
 * Note that a read operation may return an error to indicate that the
 * underlying device has failed, in which case you should (a) close the
 * device and (b) return an error on this and all subsequent read or
 * write operations. 
 */
static int stripe_read(struct blkdev * dev, int first_blk,
        int num_blks, void *buf)
{
    struct stripe_dev *sdev = dev->private;

    // check if the address is out of bound or less than ZERO
    if(first_blk + num_blks > sdev->nblks || first_blk < 0){
        return E_BADADDR;
    }

    // keep reading from stripe device to buffer if number of blocks to
    // be read are greater than ZERO.
    int read_pos = first_blk;
    int blocks_left = num_blks;
    while(blocks_left > 0){
        // caculate disk_index and disk_address in that disk of given read_pos
        int stripe_number = read_pos / sdev->stripe_size;
        int stripe_offset = read_pos % sdev->stripe_size;
        int disk_index = stripe_number % sdev->ndisks;
        int stripe_set_number = stripe_number / sdev->ndisks;
        int disk_address = stripe_set_number * sdev->stripe_size + stripe_offset;

        if(sdev->disks[disk_index] != NULL){
            int result = sdev->disks[disk_index]->ops->read(sdev->disks[disk_index], disk_address, 1, buf);
            if(result == SUCCESS){
                buf += BLOCK_SIZE;
                read_pos++;
                blocks_left--;
            } else {
                sdev->disks[disk_index]->ops->close(sdev->disks[disk_index]);
                return result;
            }
        } else {
            sdev->disks[disk_index]->ops->close(sdev->disks[disk_index]);
            return E_UNAVAIL;
        }
    }
    return SUCCESS;
}

/* write blocks to a striped volume.
 * Again if an underlying device fails you should close it and return
 * an error for this and all subsequent read or write operations.
 */
static int stripe_write(struct blkdev * dev, int first_blk,
        int num_blks, void *buf)
{
    struct stripe_dev *sdev = dev->private;

    // check if the address is out of bound or less than ZERO
    if(first_blk + num_blks > sdev->nblks || first_blk < 0){
        return E_BADADDR;
    }

    // keep reading from stripe device to buffer if number of blocks to
    // be read are greater than ZERO.
    int write_pos = first_blk;
    int blocks_left = num_blks;
    while(blocks_left > 0){
        // caculate disk_index and disk_address in that disk of given read_pos
        int stripe_number = write_pos / sdev->stripe_size;
        int stripe_offset = write_pos % sdev->stripe_size;
        int disk_index = stripe_number % sdev->ndisks;
        int stripe_set_number = stripe_number / sdev->ndisks;
        int disk_address = stripe_set_number * sdev->stripe_size + stripe_offset;

        if(sdev->disks[disk_index] != NULL){
            int result = sdev->disks[disk_index]->ops->write(sdev->disks[disk_index], disk_address, 1, buf);
            if(result == SUCCESS){
                buf +=  BLOCK_SIZE;
                write_pos ++;
                blocks_left --;
            } else {
                sdev->disks[disk_index]->ops->close(sdev->disks[disk_index]);
                return result;
            }
        } else {
            sdev->disks[disk_index]->ops->close(sdev->disks[disk_index]);
            return E_UNAVAIL;
        }
    }
    return SUCCESS;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in stripe_create. 
 */
static void stripe_close(struct blkdev *dev)
{
    struct stripe_dev *sdev = dev->private;
    // close all disks
    int i = 0;
    for(i = 0; i < sdev->ndisks; i++){
        sdev->disks[i]->ops->close(sdev->disks[i]);
    }
    //free the mem
    free(sdev);
    free(dev);
}

struct blkdev_ops stripe_ops = {
        .num_blocks = stripe_num_blocks,
        .read = stripe_read,
        .write = stripe_write,
        .close = stripe_close
};


/* create a striped volume across N disks, with a stripe size of
 * 'unit'. (i.e. if 'unit' is 4, then blocks 0..3 will be on disks[0],
 * 4..7 on disks[1], etc.)
 * Check the size of the disks to compute the final volume size, and
 * fail (return NULL) if they aren't all the same.
 * Do not write to the disks in this function.
 */
struct blkdev *striped_create(int N, struct blkdev *disks[], int unit)
{

    int disk_size = disks[0]->ops->num_blocks(disks[0]);
    assert(unit < disk_size);

    int i = 0;
    for (i = 0; i < N; i++) {
        int temp_size = disks[i]->ops->num_blocks(disks[i]);
        if(temp_size != disk_size){
            printf("error: given disks have differnet sizes");
            return NULL;
        }
    }

    // allocate mem for the stripe_dev
    struct blkdev *dev = malloc(sizeof(*dev));
    struct stripe_dev *sdev = malloc(sizeof(*sdev));
    sdev->disks = malloc(N * sizeof(disk_size));

    for (i = 0; i < N; i++) {
        sdev->disks[i] = disks[i];
    }
    sdev->nblks = unit * (disk_size/unit) * N;
    sdev->ndisks = N;
    sdev->stripe_size = unit;

    dev->private = sdev;
    dev->ops = &stripe_ops;
    return dev;
}


/**********   RAID 4  ***************/

struct raid4_dev {
    struct blkdev** disks;    /* flag bad disk by setting to NULL */
    int unit;  // unit size initialized in the raid4 creation
    int count; // disk size including the failed or degraded one
};

/* helper function - compute parity function across two blocks of
 * 'len' bytes and put it in a third block. Note that 'dst' can be the
 * same as either 'src1' or 'src2', so to compute parity across N
 * blocks you can do:
 *
 *     void **block[i] - array of pointers to blocks
 *     dst = <zeros[len]>
 *     for (i = 0; i < N; i++)
 *        parity(block[i], dst, dst);
 *
 * Yes, it's slow.
 */
void parity(int len, void *src1, void *src2, void *dst)
{
    unsigned char *s1 = src1, *s2 = src2, *d = dst;
    int i;
    for (i = 0; i < len; ++i)
        d[i] = s1[i] ^ s2[i];
}

static int raid4_num_blocks(struct blkdev *dev)
{
    // for each disk and get the num of blocks from a single available disk
    int i = 0, single_disk_blks = 0;
    struct raid4_dev* r4dev = (struct raid4_dev *) dev->private;
    for (i = 0; i < r4dev->count; ++i) {
        struct blkdev *disk = r4dev->disks[i];
        if (disk) { // at least one disk will never be NULL
            single_disk_blks = disk->ops->num_blocks(disk) / r4dev->unit * r4dev->unit;        
            break;
        }
    }

    // return the total num of blocks of this raid4 device
    return single_disk_blks * (r4dev->count - 1);
}

static int raid4_volume_degraded(struct blkdev *dev) 
{
    int i = 0;
    struct raid4_dev* r4dev = (struct raid4_dev *) dev->private;    
    for (i = 0; i < r4dev->count; ++i) {
        if (!r4dev->disks[i])
            return i;        
    }
    return -1;
}

/* read blocks from a RAID 4 volume.
 * If the volume is in a degraded state you may need to reconstruct
 * data from the other stripes of the stripe set plus parity.
 * If a drive fails during a read and all other drives are
 * operational, close that drive and continue in degraded state.
 * If a drive fails and the volume is already in a degraded state,
 * close the drive and return an error.
 */
static int raid4_read(struct blkdev * dev, int first_blk,
                      int num_blks, void *buf)
{    
    int i = 0, j = 0, k = 0, ret = SUCCESS;
    int total_blks = raid4_num_blocks(dev);
    struct raid4_dev *r4dev = (struct raid4_dev *) dev->private;

    assert(num_blks > 0);

    // check if any address in the requested range is illegal - i.e.
    // less than zero or greater than blkdev->ops->size(blkdev)-1.
    if (first_blk < 0 || first_blk + num_blks > total_blks)
        return E_BADADDR;    

    // check whether the volumn is failed
    int failed_count = 0;        
    for (i = 0; i < r4dev->count; ++i) {       
        failed_count += !r4dev->disks[i];
        if (failed_count > 1)
            return E_UNAVAIL;
    }

    // create buffers for parity operation
    unsigned char** bufs = (unsigned char **) calloc(r4dev->count - 1, sizeof(unsigned char *));
    for (i = 0; i < r4dev->count - 1; ++i)
        bufs[i] = (unsigned char *) calloc(BLOCK_SIZE, sizeof(unsigned char));    

    // read each block
    for (i = first_blk; i < first_blk + num_blks; ++i) {
        int degraded   = raid4_volume_degraded(dev);           // get the degraded disk index
        int device_idx = i / r4dev->unit % (r4dev->count - 1); // device index for the first block
        int unit_idx   = i / r4dev->unit / (r4dev->count - 1); // unit index of this device
        int block_idx  = unit_idx * r4dev->unit + i % r4dev->unit;

__retry:               
        if (degraded == device_idx) { // degraded on regular disk
            // read data and parity to each buffer
            for (k = 0, j = 0; j < r4dev->count; ++j) {
                if (j == degraded) // skip the degraded drive
                    continue;                                
                struct blkdev *disk = r4dev->disks[j];
                ret = disk->ops->read(disk, block_idx, 1, bufs[k++]);
                assert(ret != E_BADADDR);
                if (ret == E_UNAVAIL) { // fails and the volume is already in a degraded state
                    // close the drive and return an error
                    disk->ops->close(disk);
                    r4dev->disks[j] = NULL;
                    goto __error; // a drive fails and the volume is already in a degraded state
                }
            }

            // try to reconstruct data from the other stripes of the stripe set plus parity        
            for (j = 0; j < r4dev->count - 2; ++j)
                parity(BLOCK_SIZE, bufs[j], bufs[j + 1], bufs[j + 1]);    

            // copy the reconstructed block into the buffer        
            memcpy(buf, bufs[r4dev->count - 2], BLOCK_SIZE);

        } else if (degraded == r4dev->count - 1) { // the parity disk is degraded
            struct blkdev *disk = r4dev->disks[device_idx];
            ret = disk->ops->read(disk, block_idx, 1, buf);
            assert(ret != E_BADADDR);
            if (ret == E_UNAVAIL) {
                disk->ops->close(disk); 
                r4dev->disks[device_idx] = NULL;                
                goto __error; // a drive fails and the volume is already in a degraded state
            }
        } else { // all drives are operational
            struct blkdev *disk = r4dev->disks[device_idx];
            ret = disk->ops->read(disk, block_idx, 1, buf);
            assert(ret != E_BADADDR);
            if (ret == E_UNAVAIL) {
                disk->ops->close(disk); 
                r4dev->disks[device_idx] = NULL;   
                degraded = device_idx; // update degraded disk index       
                goto __retry; // continue in degraded state, use "goto" to reduce indents
            }
        }

        buf += BLOCK_SIZE; // read one block size each time
    }

__error:
    for (i = 0; i < r4dev->count - 1; ++i)
        free(bufs[i]);
    free(bufs);

    return ret;
}

/* write blocks to a RAID 4 volume.
 * Note that you must handle short writes - i.e. less than a full
 * stripe set. You may either use the optimized algorithm (for N>3
 * read old data, parity, write new data, new parity) or you can read
 * the entire stripe set, modify it, and re-write it. Your code will
 * be graded on correctness, not speed.
 * If an underlying device fails you should close it and complete the
 * write in the degraded state. If a drive fails in the degraded
 * state, close it and return an error.
 * In the degraded state perform all writes to non-failed drives, and
 * forget about the failed one. (parity will handle it)
 */
static int raid4_write(struct blkdev * dev, int first_blk,
                       int num_blks, void *buf)
{
    int i = 0, j = 0, k = 0, ret = SUCCESS;
    int total_blks = raid4_num_blocks(dev);
    struct raid4_dev *r4dev = (struct raid4_dev *) dev->private;

    assert(num_blks > 0);

    // check if any address in the requested range is illegal - i.e.
    // less than zero or greater than blkdev->ops->size(blkdev)-1.
    if (first_blk < 0 || first_blk + num_blks > total_blks)
        return E_BADADDR;    

    // check whether the volumn is failed
    int failed_count = 0;        
    for (i = 0; i < r4dev->count; ++i) {       
        failed_count += !r4dev->disks[i];
        if (failed_count > 1)
            return E_UNAVAIL;
    }

    // create buffers for parity operation
    unsigned char** bufs = (unsigned char **) calloc(r4dev->count - 1, sizeof(unsigned char *));
    for (i = 0; i < r4dev->count - 1; ++i)
        bufs[i] = (unsigned char *) calloc(BLOCK_SIZE, sizeof(unsigned char));    

    // read each block
    for (i = first_blk; i < first_blk + num_blks; ++i) {
        int degraded   = raid4_volume_degraded(dev);           // get the degraded disk index
        int device_idx = i / r4dev->unit % (r4dev->count - 1); // device index for the first block
        int unit_idx   = i / r4dev->unit / (r4dev->count - 1); // unit index of this device
        int block_idx  = unit_idx * r4dev->unit + i % r4dev->unit;

__retry:        
        /* read old data, parity if not in degraded mode, otherwise reconstruct the old data or parity */
        /* save the old data in bufs[0], and old parity bufs[last index] */
        if (degraded == device_idx) {
            // read data and parity to each buffer
            for (k = 0, j = 0; j < r4dev->count; ++j) {
                if (j == degraded) // skip the degraded drive
                    continue;                                
                struct blkdev *disk = r4dev->disks[j];
                ret = disk->ops->read(disk, block_idx, 1, bufs[k++]);
                assert(ret != E_BADADDR);
                if (ret == E_UNAVAIL) { // fails and the volume is already in a degraded state
                    // close the drive and return an error
                    disk->ops->close(disk);
                    r4dev->disks[j] = NULL;
                    goto __error; // a drive fails and the volume is already in a degraded state
                }
            }

            // reconstruct the old data  
            for (j = r4dev->count - 2; j > 0; --j) {                           
                parity(BLOCK_SIZE, bufs[j], bufs[j - 1], bufs[j - 1]);  
            }

            // compute the new parity
            parity(BLOCK_SIZE, bufs[0], bufs[r4dev->count - 2], bufs[0]);
            parity(BLOCK_SIZE, buf, bufs[0], bufs[0]);

            // write new parity to raid4
            struct blkdev *pdisk = r4dev->disks[r4dev->count - 1];  
            ret = pdisk->ops->write(pdisk, block_idx, 1, bufs[0]);
            assert(ret != E_BADADDR);
            if (ret == E_UNAVAIL) {
                pdisk->ops->close(pdisk); 
                r4dev->disks[r4dev->count - 1] = NULL;         
                goto __error; // a drive fails and the volume is already in a degraded state
            }
        } else if (degraded == r4dev->count - 1) { // the parity disk is degraded
            // write the data directly                    
            struct blkdev *disk = r4dev->disks[device_idx];  
            ret = disk->ops->write(disk, block_idx, 1, buf);
            if (ret == E_UNAVAIL) {
                disk->ops->close(disk); 
                r4dev->disks[device_idx] = NULL;             
                goto __error; // a drive fails and the volume is already in a degraded state
            }
        } else { // all drives are operational
            // read old data
            struct blkdev *disk = r4dev->disks[device_idx];            
            ret = disk->ops->read(disk, block_idx, 1, bufs[0]);
            assert(ret != E_BADADDR);
            if (ret == E_UNAVAIL) {
                disk->ops->close(disk); 
                r4dev->disks[device_idx] = NULL; 
                degraded = device_idx;             
                goto __retry; // continue in degraded state, use "goto" to reduce indents
            }

            // read parity
            struct blkdev *pdisk = r4dev->disks[r4dev->count - 1];            
            ret = pdisk->ops->read(pdisk, block_idx, 1, bufs[r4dev->count - 2]);
            assert(ret != E_BADADDR);
            if (ret == E_UNAVAIL) {
                pdisk->ops->close(pdisk); 
                r4dev->disks[r4dev->count - 1] = NULL;   
                degraded = r4dev->count - 1;           
                goto __retry; // continue in degraded state, use "goto" to reduce indents
            }

            // compute the new parity
            parity(BLOCK_SIZE, bufs[0], bufs[r4dev->count - 2], bufs[0]);
            parity(BLOCK_SIZE, buf, bufs[0], bufs[0]);

            // now bufs[0] has new parity, write new data and new parity to raid4
            ret = disk->ops->write(disk, block_idx, 1, buf);
            if (ret == E_UNAVAIL) {
                disk->ops->close(disk); 
                r4dev->disks[device_idx] = NULL; 
                degraded = device_idx;             
                goto __retry; // continue in degraded state, use "goto" to reduce indents
            }
            ret = pdisk->ops->write(pdisk, block_idx, 1, bufs[0]);
            assert(ret != E_BADADDR);
            if (ret == E_UNAVAIL) {
                pdisk->ops->close(pdisk); 
                r4dev->disks[r4dev->count - 1] = NULL;   
                degraded = r4dev->count - 1;           
                goto __retry; // continue in degraded state, use "goto" to reduce indents
            }
        }

        buf += BLOCK_SIZE; // read one block size each time
    }

__error:
    for (i = 0; i < r4dev->count - 1; ++i)
        free(bufs[i]);
    free(bufs);

    return ret;
}

/* clean up, including: close all devices and free any data structures
 * you allocated in raid4_create. 
 */
static void raid4_close(struct blkdev *dev)
{   
    // assume the input blkdev is valid and correctly initialized
    int i = 0;
    struct raid4_dev *r4dev = (struct raid4_dev *) dev->private;

    for (i = 0; i < r4dev->count; ++i) {
        struct blkdev *disk = r4dev->disks[i]; 
        if (disk) {  // the dev is closed already               
            disk->ops->close(disk); 
            r4dev->disks[i] = NULL;
        }                   
    }

    dev->private = NULL;    /* crash any attempts to access */
    free(r4dev->disks);
    free(r4dev);    
    free(dev->ops);
    free(dev);
}

void update_parity(struct blkdev *dev)
{
    struct raid4_dev *r4dev = (struct raid4_dev *) dev->private;
    int i, j, num_blocks = r4dev->disks[0]->ops->num_blocks(r4dev->disks[0]);

    // adjust the num block based on the unit size
    int num_units = num_blocks / r4dev->unit;
    int unit_size = r4dev->unit * BLOCK_SIZE;

    // create buffers for parity operation, assume no error here
    unsigned char** bufs = (unsigned char **) calloc(r4dev->count - 1, sizeof(unsigned char *));
    for (i = 0; i < r4dev->count - 1; ++i)
        bufs[i] = (unsigned char *) calloc(unit_size, sizeof(unsigned char)); 

    /* for each unit on every disk to reconstruct the parity    
     */
    for (i = 0; i < num_units; ++i) {
        // read each unit from every disk except the last one
        for (j = 0; j < r4dev->count - 1; ++j) {
            struct blkdev *disk = r4dev->disks[j];
            disk->ops->read(disk, r4dev->unit * i, r4dev->unit, bufs[j]);
        }
        // try to reconstruct data from the other stripes of the stripe set plus parity
        for (j = 0; j < r4dev->count - 2; ++j)                           
            parity(unit_size, bufs[j], bufs[j + 1], bufs[j + 1]); 

        // write the new parity to the last disk
        struct blkdev *pdisk = r4dev->disks[r4dev->count - 1];
        pdisk->ops->write(pdisk, r4dev->unit * i, r4dev->unit, bufs[r4dev->count - 2]);
    }

    // release all buffers for parity
    for (i = 0; i < r4dev->count - 1; ++i)
        free(bufs[i]);    
    free(bufs);
}

/* Initialize a RAID 4 volume with stripe size 'unit', using
 * disks[N-1] as the parity drive. Do not write to the disks - assume
 * that they are properly initialized with correct parity. (warning -
 * some of the grading scripts may fail if you modify data on the
 * drives in this function)
 */
struct blkdev *raid4_create(int N, struct blkdev *disks[], int unit)
{
    assert(N >= 3 && disks && unit > 0);

    // create a blkdev structure for this raid4 device
    struct blkdev *dev = (struct blkdev *) calloc(1, sizeof(struct blkdev));    
    if (!dev)
        goto __error;

    // create memory for blkdev_ops
    dev->ops = (struct blkdev_ops *) calloc(1, sizeof(struct blkdev_ops));
    if (!dev->ops)
        goto __error;

    // create the raid4_dev as the "private" field of the blkdev created above
    struct raid4_dev *r4dev = (struct raid4_dev *) calloc(1, sizeof(struct raid4_dev));
    if (!r4dev)
        goto __error;

    r4dev->disks = (struct blkdev **) calloc(N, sizeof(struct blkdev *));
    if (!r4dev->disks) 
        goto __error;    

    // check the size of each disk, return NULL for a size mis-match
    int i, size = disks[0]->ops->num_blocks(disks[0]);
    for (i = 1; i < N; ++i) {
        if (disks[i] && size != disks[i]->ops->num_blocks(disks[i])) {
            fprintf(stderr, "disk size mis-match\n");
            goto __error;
        }
    }
    assert(size >= unit);

    // populate the raid4_dev
    r4dev->unit  = unit;
    r4dev->count = N;
    memcpy(r4dev->disks, disks, sizeof(struct blkdev *) * N);

    // populate the blkdev before returning.    
    dev->ops->num_blocks = raid4_num_blocks;
    dev->ops->read       = raid4_read;
    dev->ops->write      = raid4_write;
    dev->ops->close      = raid4_close;    
    dev->private = r4dev;

    // update the parity in case some disk is repaired but still has the old data
    update_parity(dev); // assume it always works

    return dev;

__error:   
    // release the allocated memory
    if (dev) {
        if (dev->ops)
            free(dev->ops);
        if (r4dev) {
            if (r4dev->disks)
                free(r4dev->disks);
            free(r4dev);        
        }
        free(dev);
    }

    return NULL;
}

/* replace failed device 'i' in a RAID 4. Note that we assume
 * the upper layer knows which device failed. You will need to
 * reconstruct content from data and parity before returning
 * from this call.
 */
int raid4_replace(struct blkdev *volume, int i, struct blkdev *newdisk)
{
    int j = 0, k = 0, n = 0;
    int ret = SUCCESS;
    struct raid4_dev *r4dev = (struct raid4_dev *) volume->private;    

    // if the new disk is not the same size as the old one, return E_SIZE    
    int num_blocks = newdisk->ops->num_blocks(newdisk);
    for (j = 0; j < r4dev->count; ++j) {
        struct blkdev *disk = (struct blkdev *) r4dev->disks[j];
        if (disk && num_blocks != disk->ops->num_blocks(disk)) {
            fprintf(stderr, "disk size mis-match\n");
            return E_SIZE;
        }        
    }

    // adjust the num block based on the unit size
    int num_units = num_blocks / r4dev->unit;
    int unit_size = r4dev->unit * BLOCK_SIZE;

    // create buffers for parity operation, assume no error here
    unsigned char** bufs = (unsigned char **) calloc(r4dev->count - 1, sizeof(unsigned char *));
    for (j = 0; j < r4dev->count - 1; ++j)
        bufs[j] = (unsigned char *) calloc(unit_size, sizeof(unsigned char)); 

    // for each unit on every disk to reconstruct content from data and parity    
    for (n = 0; n < num_units; ++n) {
        // read data and parity to each buffer
        for (k = 0, j = 0; j < r4dev->count; ++j) {
            if (j != i) {// skip the drive 'i'
                struct blkdev *disk = r4dev->disks[j];
                ret = disk->ops->read(disk, r4dev->unit * n, r4dev->unit, bufs[k++]);
                assert(ret == SUCCESS);               
            }
        }
        // try to reconstruct data from the other stripes of the stripe set plus parity
        for (j = 0; j < r4dev->count - 2; ++j)                           
            parity(unit_size, bufs[j], bufs[j + 1], bufs[j + 1]); 

        // write the reconstruct content to the new disk
        ret = newdisk->ops->write(newdisk, r4dev->unit * n, r4dev->unit, bufs[r4dev->count - 2]);
        assert(ret == SUCCESS);
    }

    // update the raid4 by adding the new disk
    if (r4dev->disks[i])
        r4dev->disks[i]->ops->close(r4dev->disks[i]);        
    r4dev->disks[i] = newdisk;

    // release all buffers for parity
    for (i = 0; i < r4dev->count - 1; ++i)
        free(bufs[i]);    
    free(bufs);

    return SUCCESS;
}

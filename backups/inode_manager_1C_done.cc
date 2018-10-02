#include "inode_manager.h"
#include <stdio.h>
#include <ctime>
#include <cstring>

/*
 * FUNCTION NOTES:
 * 		void *memcpy(void *dest, const void *src, size_t n);
 *		void bzero(void *s, int n); // put n byte 0 
 *
 * DECLARATION NOTES:
 * 		disk: blocks[BLOCK_NUM][BLOCK_SIZE]
 *
 */	

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void 
disk::read_block(blockid_t id, char *buf)
{
	if ((id < 0) || (id >= BLOCK_NUM) || (!buf)){
		printf("\t invalid blockid %d\n", id);
		return;		
	}
	memcpy(buf, blocks[id], BLOCK_SIZE); // read buf
}

void
disk::write_block(blockid_t id, const char *buf)
{
	if ((id < 0) || (id >= BLOCK_NUM) || (!buf)){
		printf("\t invalid blockid %d\n", id);
		return;    
	}   
	memcpy(blocks[id], buf, BLOCK_SIZE); // write buf
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.
   */
	blockid_t cur_block = 0;
	char buf[BLOCK_SIZE];

	while (cur_block < sb.nblocks)	//struct superblock sb
	{  
		read_block(BBLOCK(cur_block), buf);
		for (int i = 0; i < BLOCK_SIZE && cur_block < sb.nblocks; i++)
		{
			unsigned char mask = 0x80; //10000000
			while (mask > 0 && cur_block < sb.nblocks)
			{
				if ((buf[i] & mask) == 0)
				{
					buf[i] |= mask;
					int pos_bblock = BBLOCK(cur_block); 
						//position of bitmap block(stored this block id)
					write_block(pos_bblock, buf);

					std::cout << "\talloc_block  " <<cur_block <<"\n";
					return cur_block;
				}
				mask = mask >> 1;
				cur_block++;
			}
		}
	}
	
	printf("\tbm: no available block\n");
	exit(-1);
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
	if ((id < 0) || (id >= BLOCK_NUM)){
		return;
	}
	
	char buf[BLOCK_SIZE];
	int pos_bblock = BBLOCK(id); //position of bitmap block(stored this block id)

	read_block(pos_bblock, buf);
	int index = (id % BPB) >> 3;
	unsigned char mask = 0xFF ^ (1 << (7 - ((id % BPB) & 0x7)));
	buf[index] &= mask;
	write_block(pos_bblock, buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

	//mark bootblock, superblock, bitmap, inode table 
	char buf[BLOCK_SIZE];
	blockid_t cur_block = 0;
	blockid_t reserve_block = RESERVED_BLOCK(sb.nblocks, sb.ninodes);
	
	while (cur_block < reserve_block){
		read_block(BBLOCK(cur_block), buf);
		for (int i = 0; i < BLOCK_SIZE && cur_block < reserve_block; i++){
			unsigned char mask = 0x80;
			while (mask > 0 && cur_block < reserve_block){
				buf[i] |= mask;
				mask = mask >> 1;
				cur_block++;
			}
		}
		write_block(BBLOCK(cur_block - 1), buf); 
	}

	bzero(buf, sizeof(buf));
	memcpy(buf, &sb, sizeof(sb));
	write_block(1, buf);
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf); //disk *d;
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
	std::cout<<"  write block: buf "<< buf <<"\n";
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
   */

	// type=0:free ; type=1 2

	uint32_t cur_inode = 1;

    while (cur_inode < INODE_NUM && cur_inode <= bm->sb.ninodes)  //struct superblock sb, block_manager* bm
    {
		inode_t *inode = get_inode(cur_inode);
        if (inode == NULL || inode->type == 0)
		{
			inode = (inode_t *)malloc(sizeof(inode_t));
			inode->type = type;
			inode->size = 0;
			inode->atime = (unsigned int)time(NULL);
			inode->mtime = (unsigned int)time(NULL);
			inode->ctime = (unsigned int)time(NULL);
			
			put_inode(cur_inode, inode);
			free(inode);
			return cur_inode;
		}
		cur_inode++;
    }

	printf("\tim: no available inodes\n");
	exit(-1);
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   */
	if ((inum < 1) || (inum > INODE_NUM)){
		return;
	}

	inode_t *inode = get_inode(inum);
	if (inum == NULL || inode->type == 0){
		printf("\tim: bad inodes\n");
	} else {
		inode->type = 0;
		put_inode(inum, inode);
		return;
	}

	printf("\tim: bad inodes\n");
    exit(-1);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;
	
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_Out
   */
	char block[BLOCK_SIZE];

	for(int index=2;index<52;index++){
		inode_t *inod = get_inode(index);
		char bb[BLOCK_SIZE];
		bm->read_block(inod->blocks[index], bb);
		std::cout<<"\t read_file: block index: "<< index<<"  bb: "<<bb<<"\n";
	
		if (inod == NULL){
			printf("\terror in read_file\n");
			return;
		}
	}

	inode_t *inode = get_inode(inum);
	if (inode == NULL){
                printf("\terror in read_file\n");
                return;
	}

	char *buf = (char *)malloc(inode->size);
	unsigned int pos = 0; // for buf

	for (unsigned int i = 0; i < NDIRECT && pos < inode->size; i++)
	{
		// read blocksize
		if ((inode->size - pos) > BLOCK_SIZE){
			bm->read_block(inode->blocks[i], buf+pos); 
				//read block and store in buf
			pos += BLOCK_SIZE;
//			std::cout<<"\tread_file a " << buf <<"\n";
		} else {
			int left_len = inode->size - pos;
			bm->read_block(inode->blocks[i], block);
			memcpy(buf+pos, block, left_len); //block to buf
			pos += left_len;
			std::cout<<"\tread_file b " << buf <<" i: "<<i<<"\n";
		}
	}

	//if quit for due to i<NDIRECT, indirect blocks
	if (pos < inode->size)
	{
		char indirect[BLOCK_SIZE];
		bm->read_block(inode->blocks[NDIRECT], indirect); //last block
		for (unsigned int i = 0; i < NINDIRECT && pos < inode->size; i++)
		{
			blockid_t blockid = *((blockid_t *)indirect + i); //point ALU
			if (inode->size - pos > BLOCK_SIZE){
            	bm->read_block(blockid, buf+pos); 
                	//read block and store in buf
	            pos += BLOCK_SIZE;
//				std::cout<<"\tread_file c " << buf <<"\n";
    	    } else {
        	    int left_len = inode->size - pos;
            	bm->read_block(blockid, block);
	            memcpy(buf+pos, block, left_len); //block to buf
    	        pos += left_len;
//				std::cout<<"\tread_file d " << buf <<"\n";
       		}
		}
	}

	*buf_out = buf;

	*size = inode->size;

	std::cout<<"\tread_file " << buf << " size "<< size<<"\n";
	inode->atime = (unsigned int)time(NULL);
	inode->ctime = (unsigned int)time(NULL);
	put_inode(inum, inode);
  	free(inode);
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode
   */
	char block[BLOCK_SIZE];
	char indirect[BLOCK_SIZE];
	inode_t *inode = get_inode(inum); // init

	// get old and new block num
	unsigned int old_blocknum = (inode->size + BLOCK_SIZE -1) / BLOCK_SIZE;
	unsigned int new_blocknum = (size + BLOCK_SIZE -1 ) / BLOCK_SIZE;

	std::cout<<"write file: old "<< old_blocknum<<" new: "<<new_blocknum<<"\n";

	// --- change the block (free or alloc) ---//
	// case 1: old blocknum > new blocknum, need free  
	if (old_blocknum > new_blocknum)
	{
		// case 1.1: old,new > NDIRECT
		if (new_blocknum > NDIRECT)
		{	
			bm->read_block(inode->blocks[NDIRECT], indirect); //use indirect blocks
			for (unsigned int i = new_blocknum; i < old_blocknum; i++){
				bm->free_block(*((blockid_t *)indirect + (i - NDIRECT))); //free unused blocks
			}
		}
		// case 1.2: new < NDIRECT
		else {
			// case 1.2.1: new<NDIRECT and old>NDIRECT
			if (old_blocknum > NDIRECT)
			{
				bm->read_block(inode->blocks[NDIRECT], indirect);
				for (unsigned int i = NDIRECT; i < old_blocknum; i++){
					bm->free_block(*((blockid_t *)indirect + (i - NDIRECT)));
				}
				bm->free_block(inode->blocks[NDIRECT]);
				for (unsigned int i = new_blocknum; i < NDIRECT; i++){
					bm->free_block(inode->blocks[i]);
				}
			}
			// case 1.2.2: new,old < NDIRECT
			else {
				for (unsigned int i = new_blocknum; i < old_blocknum; i++){
					bm->free_block(inode->blocks[i]);
				}
			}
		}
	} 
	
	// case 2: old blocknum <= new blocknum, need alloc
	if (old_blocknum <= new_blocknum) 
	{ 
		// case 2.1: new,old <= NDIRECT
		if (new_blocknum <= NDIRECT){
			for (unsigned int i = old_blocknum; i < new_blocknum; i++){
				inode->blocks[i] = bm->alloc_block();
			}
		}
		// case 2.2: new > NDIRECT
		else {
			// case 2.2.1: new>NDIRECT, old<NDIRECT
			if (old_blocknum <= NDIRECT){
				for (unsigned int i = old_blocknum; i < NDIRECT; i++){
					inode->blocks[i] = bm->alloc_block();
				}
				inode->blocks[NDIRECT] = bm->alloc_block();
				
				bzero(indirect, BLOCK_SIZE);
				for (unsigned int i = NDIRECT; i < new_blocknum; i++){
					*((blockid_t *)indirect + (i - NDIRECT)) = bm->alloc_block();
				}
				bm->write_block(inode->blocks[NDIRECT], indirect);
					// write indirect into NDIRECT
			}	
			// case 2.2.2: new,old > NDIRECT
			else {
				bm->read_block(inode->blocks[NDIRECT], indirect);
				for (unsigned int i = old_blocknum; i < new_blocknum; i++){
					*((blockid_t *)indirect + (i - NDIRECT)) = bm->alloc_block();
				}
				bm->write_block(inode->blocks[NDIRECT], indirect);
			}
		}
	}	

	// --- write buf to blocks ---//
	int pos = 0; // for buf, code is like read_file
	
	for (int i = 0; i < NDIRECT && pos < size; i++)
    {   
        // write blocksize
        if (size - pos > BLOCK_SIZE){
            bm->write_block(inode->blocks[i], buf+pos); 
                //write buf into block
            pos += BLOCK_SIZE;
        } else {
            int left_len = size - pos;
            memcpy(block, buf+pos, left_len); //buf to block
			bm->write_block(inode->blocks[i], block);
            pos += left_len;
        }   
    }   
	
	//if quit for due to i<NDIRECT, indirect blocks
	if (pos < size)
	{
        bm->read_block(inode->blocks[NDIRECT], indirect); //last block
        for (unsigned int i = 0; i < NINDIRECT && pos < size; i++)
		{
            blockid_t blockid = *((blockid_t *)indirect + i); //point ALU
            if (size - pos > BLOCK_SIZE){
                bm->write_block(blockid, buf+pos);
                    //read block and store in buf
                pos += BLOCK_SIZE;
            } else {
                int left_len = size - pos;
                memcpy(block, buf+pos, left_len); //buf to block
				bm->write_block(blockid, block);
                pos += left_len;
            }
        }
    }

//	std::cout<<"\t inum: "<<inum<<" buf: "<<buf<<"\t"; 	

	// --- change inode attr ---//
	inode->size = size;
	inode->mtime = (unsigned int)time(NULL); // modify
	inode->ctime = (unsigned int)time(NULL); // change mode

	std::cout<<"write file: inum "<< inum<<" buf: "<<buf<<"\n";

	put_inode(inum, inode);
	free(inode);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
 	inode_t* inode = get_inode(inum);
	if (!inode){
		printf("\tim: bad inode num\n");
		return;
	} 

	a.type = inode->type;
	a.size = inode->size;
	a.atime = inode->atime;
	a.mtime = inode->mtime;
	a.ctime = inode->ctime;

	free(inode);
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your code goes here
   * note: you need to consider about both the data block and inode of the file
   */
	std::cout<<"remove file: inum "<< inum <<"\n";
  
	inode_t *inode = get_inode(inum);
	unsigned int blocknum = (inode->size + BLOCK_SIZE - 1 ) / BLOCK_SIZE;

	// case 1
	if (blocknum <= NDIRECT){
		for (unsigned int i = 0; i < blocknum; i++){
			bm->free_block(inode->blocks[i]);
		}
	}
	// case 2
	else {
		for (unsigned int i = 0; i < NDIRECT; i++){
			bm->free_block(inode->blocks[i]);
		}
	
		char indirect[BLOCK_SIZE];
		bm->read_block(inode->blocks[NDIRECT], indirect);
		for (unsigned int i = NDIRECT; i < blocknum; i++){
			bm->free_block(*((blockid_t *)indirect + (i - NDIRECT)));
		}
		bm->free_block(inode->blocks[NDIRECT]);
	}

	free_inode(inum);
	free(inode);
}

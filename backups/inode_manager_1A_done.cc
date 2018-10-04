#include "inode_manager.h"

/*
 * FUNCTION NOTES:
 * 		void *memcpy(void *dest, const void *src, size_t n);
 * 
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
		return;		
	}
	memcpy(buf, blocks[id], BLOCK_SIZE); // read buf
}

void
disk::write_block(blockid_t id, const char *buf)
{
	if ((id < 0) || (id >= BLOCK_NUM) || (!buf)){
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
		read_block(cur_block, buf);
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
	int pos_id = id % BPB; //bit position in bitmap block
	
	unsigned char mask = ~(1 << (pos_id % 8)); //pos_id=9 mask=11111101(small endian)
	int pos_byte = pos_id / 8;

	read_block(pos_bblock, buf);
	buf[pos_byte] &= mask;
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

    while (cur_inode < INODE_NUM)  //struct superblock sb, block_manager* bm
    {
		inode_t *inode = get_inode(cur_inode);
        if (inode == NULL)
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

  return;
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
  
  return;
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
  
  return;
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
  
  return;
}

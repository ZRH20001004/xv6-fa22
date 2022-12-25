// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct buf buf[NBUF];
} bcache;

struct {
  struct spinlock lock;
  struct buf *head;
} bucket[13];

struct spinlock steal_lock;


uint
hash(uint blockno)
{
  return blockno % 13; 
}

struct buf*
search(uint dev, uint blockno)
{
  int i;
  struct buf *b;

  i = hash(blockno);
  acquire(&bucket[i].lock);
  b = bucket[i].head;
  while(b)
  {   
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bucket[i].lock);
      acquiresleep(&b->lock);
      return b;
    }
    b = b->next;
  }
  //to prevent deadlock
  release(&bucket[i].lock);
  return 0;
}

void
binit(void)
{
  struct buf *b;

  for(int i = 0; i < 13; i++)
  {
    initlock(&bucket[i].lock, "bcache.bucket");
    if(i == 0)
    {
      for(b = bcache.buf; b < bcache.buf+NBUF; b++){
        b->next = bucket[i].head;
        bucket[i].head = b;
      }
    }
  }

  initlock(&steal_lock, "bcache.steal");

}

static struct buf*
bget(uint dev, uint blockno)
{
  struct buf* b;
  
  if((b=search(dev, blockno))){
    return b;
  }

  acquire(&steal_lock);

  if((b=search(dev, blockno))){
    return b;
  }

  for(int i = 0; i < 13; i++){
    acquire(&bucket[i].lock);
    b = bucket[i].head;
    while(b){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        if(b == bucket[i].head){
          bucket[i].head = b->next;
        }else{
          struct buf* prev;
          prev = bucket[i].head;
          while(prev->next != b){
            prev = prev->next;
          }
          prev->next = prev->next->next;
        }
        release(&bucket[i].lock);
        break;
      }
      b = b->next;
    }
    if(b)
    {
      int idx;
      idx = hash(blockno);
      acquire(&bucket[idx].lock);
      b->next = bucket[idx].head;
      bucket[idx].head = b;
      release(&bucket[idx].lock);
      release(&steal_lock);
      acquiresleep(&b->lock);
      return b;
    }
    release(&bucket[i].lock);
  }

  release(&steal_lock);

  panic("bget");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int i;

  i = hash(b->blockno);
  acquire(&bucket[i].lock);
  b->refcnt--;
  release(&bucket[i].lock);
}

void
bpin(struct buf *b) {
  int i;

  i = hash(b->blockno);
  acquire(&bucket[i].lock);
  b->refcnt++;
  release(&bucket[i].lock);
}

void
bunpin(struct buf *b) {
  int i;

  i = hash(b->blockno);
  acquire(&bucket[i].lock);
  b->refcnt--;
  release(&bucket[i].lock);
}



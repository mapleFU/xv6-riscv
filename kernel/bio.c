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

// 全局的 buffer cache 入口, 会有一个物理的 NBUF * BLOCK_SIZE 的缓存,
// 和逻辑的 lru list.
//
// 相对于 kalloc/kfree, 这个资源是单 CPU 的, kalloc 和 kfree 是不会共享的, 因为
// 一个 CPU 除了 Steal 不会拿别的核心的资源. 但是 bcache 的 b系列接口 可能只获得
// 相同的 block, 所以这里不能 per-cpu.
//
// 所以如果要提升并发, 这里需要采用 partition 的形式. 每个会 shard 到某个 bucket 里面, 然后按照 bucket 来分配.
// 这里应该没有一个全局的锁, 因为 sharding 是硬性的, 不会有 re-hash.
//
// cache 归还这里, 需要参考原本的 LRU 逻辑:
// 1. 一开始, 所有的 `struct buf` 组成了一个双向链表.
// 2. `get` 的时候, 会倾向于从尾部获取, 当然获取之后篮子都没改变.
// 3. `release` 的时候, 会从原来的位置摘掉, 放到链表的头部. 下一次会倾向于拿到这个.
struct {
  // 全局的 lock, 只有 steal 的时候会使用
  struct spinlock lock;
  // 每一个 buf 会有一个 sleeplock.
  // 它们物理上会 align 在一起.
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  //
  // 一个傻逼的 LRU.
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;

  // 双向链表, 每次插入 bcache.head.next
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
//
// 遍历, blockno 是具体的序列号, 先尝试看看有没有已经缓存了这个块, 有的话添加
// refcnt 然后占锁返回 (refcnt 似乎是由 bcache.lock 保护的).
// 没有的话, LRU 摘掉 free(refcnt == 0) 的块, 然后这个会给到用户.
//
// bget 不会调整 LRU 的内容.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      // 找到这个块之后, 要保证对数据的访问是串行的.
      // 因为它没有从 bcache 链表摘出去, 会有并发线程访问这个块.
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  // 只有 bread 之后, 内存才是 invalid 的.
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

  // 只有这个地方会占用.
  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  // 摘掉, 放到头部
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}



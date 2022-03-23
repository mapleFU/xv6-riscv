// buffer cache 层.
struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  // 挂着 sleeplock, 防止出现并发问题
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;

  // 具体存放的 block.
  uchar data[BSIZE];
};


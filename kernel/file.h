// File 层其实就是在上面多包了一层, 然后从 inode 接口到实现了一层 file 接口.
struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE, FD_DEVICE } type;
  // 文件本身的表.
  int ref; // reference count

  // 文件描述符的内容.

  char readable;
  char writable;

  // 下面抽了好几层, 基本上可以当成一个 tagged enum 对待.

  struct pipe *pipe; // FD_PIPE
  // 实际上用的具体内容
  struct inode *ip;  // FD_INODE and FD_DEVICE
  uint off;          // FD_INODE
  short major;       // FD_DEVICE
};

#define major(dev)  ((dev) >> 16 & 0xFFFF)
#define minor(dev)  ((dev) & 0xFFFF)
#define	mkdev(m,n)  ((uint)((m)<<16| (n)))

// in-memory copy of an inode (这个就单纯是文件一层的了)
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count (内存 rc)
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?

  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// map major device number to device functions.
struct devsw {
  int (*read)(int, uint64, int);
  int (*write)(int, uint64, int);
};

extern struct devsw devsw[];

#define CONSOLE 1

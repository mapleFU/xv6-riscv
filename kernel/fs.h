// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// superblock 应该用 static_assert 来严格小于 BSIZE. 这里记录了非常多分配的元信息.
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

// On-disk inode structure
// sizeof(dinode) == 2 * 4 + 4 + 4 * 13 == 64
struct dinode {
  // 区分 file, directory, device, 这些内容定义在 `stats.h`
  // 此外, type == 0 表示 free.
  short type;           // File type
  short major;          // Major device number (T_DEVICE only)
  short minor;          // Minor device number (T_DEVICE only)
  // 引用这个 inode 的 directory 对象.
  // (话说, 每个目录都会引用自己和父节点吧).
  short nlink;          // Number of links to inode in file system
  uint size;            // Size of file (bytes)
  // 具体持有这些内容的 Disk 块的地址.
  // TODO(mwish): 如果要多级的话, 这里怎么表示? 这个 dinode 怎么这么大.
  uint addrs[NDIRECT+1];   // Data block addresses
};

// Inodes per block.
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

// 目录项的内容, 16B.
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};


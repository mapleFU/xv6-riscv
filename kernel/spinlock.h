// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  // 查问题的时候, 需要知道详细一点的锁的信息之类的.
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.
};


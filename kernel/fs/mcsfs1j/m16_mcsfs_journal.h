#ifndef M16_MCSFS_JOURNAL_H
#define M16_MCSFS_JOURNAL_H
/*
 * MCSOS M16 - MCSFS1J public interface (extracted for kernel integration)
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define M16_BLOCK_SIZE 512u
#define M16_MAX_BLOCKS 128u
#define M16_MAX_INODES 16u
#define M16_DIRECT_BLOCKS 4u
#define M16_MAX_NAME 32u
#define M16_MAGIC 0x4d43534631564a31ULL
#define M16_JMAGIC 0x4d43534a524e4c31ULL
#define M16_VERSION 1u
#define M16_J_EMPTY 0u
#define M16_J_COMMITTED 2u
#define M16_JOURNAL_MAX_RECORDS 8u
#define M16_JOURNAL_START 1u
#define M16_JOURNAL_BLOCKS (1u + (2u * M16_JOURNAL_MAX_RECORDS))
#define M16_INODE_BITMAP_LBA (M16_JOURNAL_START + M16_JOURNAL_BLOCKS)
#define M16_BLOCK_BITMAP_LBA (M16_INODE_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_LBA (M16_BLOCK_BITMAP_LBA + 1u)
#define M16_INODE_TABLE_BLOCKS 4u
#define M16_ROOT_DIR_LBA (M16_INODE_TABLE_LBA + M16_INODE_TABLE_BLOCKS)
#define M16_DATA_START_LBA (M16_ROOT_DIR_LBA + 1u)

#define M16_E_OK 0
#define M16_E_INVAL -1
#define M16_E_IO -2
#define M16_E_NOSPC -3
#define M16_E_EXISTS -4
#define M16_E_NOENT -5
#define M16_E_CORRUPT -6
#define M16_E_TOOLONG -7

struct m16_blockdev {
    uint8_t blocks[M16_MAX_BLOCKS][M16_BLOCK_SIZE];
    uint32_t total_blocks;
    uint64_t writes;
    int fail_after;
};

struct m16_super {
    uint64_t magic;
    uint32_t version;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t journal_start;
    uint32_t journal_blocks;
    uint32_t inode_bitmap_lba;
    uint32_t block_bitmap_lba;
    uint32_t inode_table_lba;
    uint32_t inode_table_blocks;
    uint32_t root_dir_lba;
    uint32_t data_start_lba;
    uint32_t clean_generation;
    uint32_t reserved[114];
};

struct m16_inode {
    uint32_t used;
    uint32_t kind;
    uint32_t size;
    uint32_t direct[M16_DIRECT_BLOCKS];
    uint32_t reserved[25];
};

struct m16_dirent {
    uint32_t used;
    uint32_t ino;
    char name[M16_MAX_NAME];
};

struct m16_journal_header {
    uint64_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t seq;
    uint32_t count;
    uint32_t header_checksum;
    uint32_t reserved[121];
};

struct m16_journal_desc {
    uint64_t magic;
    uint32_t target_lba;
    uint32_t payload_checksum;
    uint32_t reserved[124];
};

struct m16_jrec {
    uint32_t target_lba;
    uint8_t payload[M16_BLOCK_SIZE];
};

struct m16_tx {
    uint32_t count;
    struct m16_jrec rec[M16_JOURNAL_MAX_RECORDS];
};

void m16_dev_init(struct m16_blockdev *dev);
int m16_journal_recover(struct m16_blockdev *dev);
int m16_format(struct m16_blockdev *dev);
int m16_mount(struct m16_blockdev *dev, struct m16_super *sb);
int m16_write_file_ex(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size, int stop_after_commit_record);
int m16_write_file(struct m16_blockdev *dev, const char *name, const uint8_t *data, uint32_t size);
int m16_read_file(struct m16_blockdev *dev, const char *name, uint8_t *out, uint32_t out_cap, uint32_t *out_size);
int m16_fsck(struct m16_blockdev *dev);

#endif

#ifndef __HDD_H
#define __HDD_H

//taken from open ps2 loader but adopted for ps2doom 
#include <libhdd.h>
#include <tamtypes.h>

//hdd and pfs whole structure
#define PFS_INODE_MAX_BLOCKS 114
#define HDD_MODE_UPDATE_DELAY MENU_UPD_DELAY_NOUPDATE

// APA Partition
#define APA_IOCTL2_GETHEADER 0x6836

typedef struct
{
    u32 start;  // Sector address
    u32 length; // Sector count
} apa_sub_t;

typedef struct
{
    u8 unused;
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} ps2time_t;

typedef struct
{
    u32 checksum;
    u32 magic; // APA_MAGIC
    u32 next;
    u32 prev;
    char id[APA_IDMAX];
    char rpwd[APA_PASSMAX];
    char fpwd[APA_PASSMAX];
    u32 start;
    u32 length;
    u16 type;
    u16 flags;
    u32 nsub;
    ps2time_t created;
    u32 main;
    u32 number;
    u32 modver;
    u32 pading1[7];
    char pading2[128];
    struct
    {
        char magic[32];
        u32 version;
        u32 nsector;
        ps2time_t created;
        u32 osdStart;
        u32 osdSize;
        char pading3[200];
    } mbr;
    apa_sub_t subs[APA_MAXSUB];
} apa_header_t;

typedef struct
{
    u32 number;
    u16 subpart;
    u16 count;
} pfs_blockinfo_t;

typedef struct
{
    u32 checksum;
    u32 magic;
    pfs_blockinfo_t inode_block;
    pfs_blockinfo_t next_segment;
    pfs_blockinfo_t last_segment;
    pfs_blockinfo_t unused;
    pfs_blockinfo_t data[PFS_INODE_MAX_BLOCKS];
    u16 mode;
    u16 attr;
    u16 uid;
    u16 gid;
    ps2time_t atime;
    ps2time_t ctime;
    ps2time_t mtime;
    u64 size;
    u32 number_blocks;
    u32 number_data;
    u32 number_segdesg;
    u32 subpart;
    u32 reserved[4];
} pfs_inode_t;
//end

/*
typedef struct
{
    char partition_name[APA_IDMAX + 1];
    char name[HDL_GAME_NAME_MAX + 1];
    char startup[8 + 1 + 3 + 1];
    u8 hdl_compat_flags;
    u8 ops2l_compat_flags;
    u8 dma_type;
    u8 dma_mode;
    u8 disctype;
    u32 layer_break;
    u32 start_sector;
    u32 total_size_in_kb;
} hdl_game_info_t;

typedef struct
{
    u32 count;
    hdl_game_info_t *games;
} hdl_games_list_t;

typedef struct
{
    u32 start;
    u32 length;
} apa_subs;

int hddReadSectors(u32 lba, u32 nsectors, void *buf);

// Array should be APA_MAXSUB+1 entries.
int hddGetPartitionInfo(const char *name, apa_sub_t *parts);

// Array should be max entries.
int hddGetFileBlockInfo(const char *name, const apa_sub_t *subs, pfs_blockinfo_t *blocks, int max);

int hddCheck(void);
u32 hddGetTotalSectors(void);
int hddIs48bit(void);
int hddSetTransferMode(int type, int mode);
void hddSetIdleTimeout(int timeout);
void hddSetIdleImmediate(void);

void hddInit();

void hddLoadModules(void);
*/

#endif
#ifndef FAT32_H
#define FAT32_H

#pragma pack(push,1)

// FAT32引导记录结构
struct dos_boot_recorder {
    unsigned char  jump_instr[3];
    unsigned char  OEM_name[8];
    unsigned short bytes_per_sector;
    unsigned char  sectors_per_cluster;
    unsigned short reserved_sector_nbr;
    unsigned char  fat_table_nbr;
    unsigned short _root_dir_entry_nbr;
    unsigned short _total_sector_nbr_16;
    unsigned char  media_type;
    unsigned short _fat_table_sector_nbr_16;
    unsigned short sectors_per_track;
    unsigned short disk_head_nbr;
    unsigned int   sectors_before_partition;
    unsigned int   total_sector_nbr_32;
    unsigned int   fat_table_sector_nbr_32;
    unsigned short fs_flag;
    unsigned short fs_version;
    unsigned int   root_dir_cluster;
    unsigned short fsinfo;
    unsigned short backup_dbr_sector_nbr;
    unsigned char  _reserved[12];
    unsigned char  bios_drive_number;
    unsigned char  __reserved;
    unsigned char  extended_boot_signature;
    unsigned int   volume_serial_number;
    unsigned char  volume_label[11];
    unsigned char  fs_type[8];
};

// 短文件名目录项结构
struct short_name_dir_entry {
    unsigned char  file_name_in_ascii[11];
    unsigned char  entry_attribute;
    unsigned char  _reserved;
    unsigned char  create_time_1;
    unsigned short create_time_2;
    unsigned short create_date;
    unsigned short last_access_date;
    unsigned short cluster_nbr_high;
    unsigned short last_write_time;
    unsigned short last_write_date;
    unsigned short cluster_nbr_low;
    unsigned int   file_size;
};

// 长文件名目录项结构
struct long_name_dir_entry {
    // TODO:
};

#pragma pack(pop)


// 目录项的分类
#define NEVER_USED_ENTRY_FLAG    ((unsigned char)0x00)
#define DELETED_ENTRY_FLAG       ((unsigned char)0xe5)
#define LONG_NAME_ENTRY_FLAG     ((unsigned char)0x0f)
#define VOLUME_LABEL_ENTRY_FLAG  ((unsigned char)0x08)
#define DIRECTORY_ENTRY_FLAG     ((unsigned char)0x10)
#define ARCHIVE_FILE_ENTRY_FLAG  ((unsigned char)0x20)

#define LONG_NAME_ENTRY_SEQ_MASK 0x40

enum dir_entry_type {
    NEVER_USED,
    DELETED_ENTRY,
    ARCHIVE,
    DIRECTORY,
    VOLUME_LABEL,
    LONG_NAME_ENTRY,
    SHORT_NAME_ENTRY,
    CUT_OFF_SHORT_NAME_ENTRY,
    OTHERS
};

#endif // FAT32_H
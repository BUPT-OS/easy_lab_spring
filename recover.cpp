#include <stdio.h>
#include <string>
#include <vector>
#include <optional>
#include <ext2fs/ext2fs.h>
#include <arpa/inet.h>
#include "journal.h"

using namespace std;

ext2_filsys fs = NULL;
char *file_to_write = NULL;

void init_output_file()
{
    FILE *fp = fopen(file_to_write, "w");
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    fclose(fp);
}

void append_to_file(const char *content)
{
    FILE *fp = fopen(file_to_write, "a");
    if (fp == NULL) {
        perror("fopen");
        return;
    }
    fputs(content, fp);
    fclose(fp);
}

blk64_t inode_to_blknbr(ext2_ino_t ino)
{
    // TODO:
}

void byte_order_conversion(journal_header_t *ptr)
{
    // TODO:
}

void byte_order_conversion(journal_block_tag_t *ptr)
{
    // TODO:
}

char *find_origin_blk_in_journal(blk64_t blk_nbr)
{
    // TODO:
}

optional<ext2_ino_t> find_deleted_inode()
{
    // TODO:
    return nullopt;
}

struct ext2_inode *get_inode_ptr_from_block(char *block, ext2_ino_t ino)
{
    // TODO:
}

void recover_deleted_file()
{    
    // 1. find deleted inodes
    auto option = find_deleted_inode();
    if (!option) {
        return;
    }
    ext2_ino_t ino = *option;
    blk64_t blk_nbr = inode_to_blknbr(ino);
    // 2. traverse the log to find historical versions of data blocks
    char *block = find_origin_blk_in_journal(blk_nbr);
    if (block == NULL) {
        printf("can't find deleted file\n");
        return;
    }
    // 3. extract original inode from block
    struct ext2_inode *inode_ptr = get_inode_ptr_from_block(block, ino);
    // 4. read data and write to file
    // TODO:

    free(block);
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        // example: recover ext3.disk recovered_file.txt
        printf("Usage: %s <ext3 image file> <file to store deleted content>\n", argv[0]);
        return 1;
    }
    
    const char *ext3_image_file = argv[1];
    file_to_write = argv[2];
    init_output_file();
    errcode_t errcode = ext2fs_open(ext3_image_file, 0, 0, 0, unix_io_manager, &fs);
    if (errcode) {
        printf("Failed to open fs image: %s\n", error_message(errcode));
        return 2;
    }
    recover_deleted_file();
    ext2fs_close(fs);
    return 0;
}
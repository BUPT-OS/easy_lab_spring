#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "FAT32.h"

struct dos_boot_recorder *dbr = NULL;
char *file_to_write = NULL;

unsigned int *first_fat_table;
unsigned int cluster_size;

void write_to_file(char *content)
{
    FILE *fp = fopen(file_to_write, "w");
    if (fp == NULL) {
        perror("fopen failed");
        return;
    }
    if (fputs(content, fp) == EOF) {
        perror("fputs failed");
        fclose(fp);
        return;
    }
    if (fclose(fp) != 0) {
        perror("fclose failed");
    }
}

void *get_cluster_ptr(unsigned int cluster_nbr)
{
    // TODO:
}

void *get_first_fat_table_ptr()
{
    // TODO:
}

char *extract_file_name(struct long_name_dir_entry *l_entry)
{
    // TODO:
}

int search_to_recover(unsigned int cluster_nbr, char *deleted_file_name)
{
    // iterate over the file system: DFS or BFS
    // TODO:
}

void recover_deleted_file(unsigned char *img_data, char *deleted_file_name)
{
    dbr = (struct dos_boot_recorder *)img_data;
    unsigned int cluster_nbr = 2;
    cluster_size = dbr->sectors_per_cluster * dbr->bytes_per_sector;
    first_fat_table = (unsigned int *)get_first_fat_table_ptr();
    if (search_to_recover(cluster_nbr, deleted_file_name)) {
        printf("successfully recover deleted file\n");
    } else {
        printf("failed to recover deleted file\n");
    }
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        printf("Usage: %s <FAT32 image file> <deleted file name> <file to store deleted content>\n", argv[0]);
        return 1;
    }
    
    const char *fat32_img_path = argv[1];
    file_to_write = argv[3];
    int fd = open(fat32_img_path, O_RDWR);
    if (fd < 0) {
        perror("Failed to open FAT32 image file");
        return 2;
    }
    struct stat fs;
    if (fstat(fd, &fs) < 0) {
        perror("Failed to get file status");
        close(fd);
        return 3;
    }
    unsigned char *img_data = mmap(NULL, fs.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (img_data == MAP_FAILED) {
        perror("Failed to map file to memory");
        close(fd);
        return 4;
    }
    recover_deleted_file(img_data, argv[2]);
    munmap(img_data, fs.st_size);
    close(fd);
    return 0;
}
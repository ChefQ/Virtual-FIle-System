#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

#include "ext2_utils.c"
#include <time.h>
#include <errno.h>
#include <string.h>
#include <libgen.h>
#include <math.h>

unsigned char* disk;

int main(int argc, char **argv){
    
    if(argc != 3) {
        fprintf(stderr, "Usage: ext2_restore <image file name> <path of file to be removed>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *super_block = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *group_desc = (struct ext2_group_desc *)(disk + 1024 + 1024);
    char *path = argv[2];

    struct ext2_inode *inode_table = (struct ext2_inode *)(disk + group_desc->bg_inode_table * 1024);
    
    // Get basename of path
    char base_name [256];
    memset(base_name, '\0', 255);
    strcpy(base_name, basename(path));

    // Check to see path exists
    int inode_number;
    // inode_number = findPath(path, (unsigned char *)inode_table);
    // if(inode_number > 0){
        // return error
    //}
    
    char *dir_name = dirname(path); // path of directory name of file path provided
    int dir_inode_number = findPath(dir_name, (unsigned char *)inode_table); // inode of dir path

    if(dir_inode_number < 0){ // check whether dir path exists
        // return error
        exit(ENOENT);
    }
    
    struct ext2_inode dir_inode = inode_table[dir_inode_number - 1]; 
    if (!(S_ISDIR(dir_inode.i_mode))){ // check type
        // return error
    }

    // standard of size of struct in bytes minus name_len and padding
    int standard = sizeof(int) + sizeof(short) + sizeof(char) + sizeof(char);

    // expected size of entry we are looking for
    int expected_size = standard + strlen(base_name);

    struct ext2_dir_entry *prev_dir_entry;
    struct ext2_dir_entry *current_dir_entry;

    int data_index = 0;
    int new_rec;
    
    while((dir_inode.i_block[data_index] != 0) && (data_index != 12)){

        unsigned char *data_pointer = BLOCK_OFFSET(dir_inode.i_block[data_index], disk + 1024);;
        struct ext2_dir_entry *prev = (struct ext2_dir_entry *)data_pointer;
        struct ext2_dir_entry *prev_initial;

        int data_tracker = 0;
        int count = 0; 
        int flag = 0;
        while (data_tracker < EXT2_BLOCK_SIZE){ // counter

            struct ext2_dir_entry *dir_entry =  (struct ext2_dir_entry *)data_pointer;

            // check rec_len
            if(dir_entry->rec_len == 0 || dir_entry->name_len == 0){
                break;
            }

            // check name of entry
            int ret = strncmp(dir_entry->name, base_name, dir_entry->name_len);
            if(ret == 0){
                inode_number = dir_entry->inode;

                prev_dir_entry = prev_initial;
                current_dir_entry = dir_entry;
                new_rec = count;

                break;
            }
            
            // actual size of entry
            int actual_length = standard + dir_entry->name_len;
            int padding = dir_entry->rec_len - actual_length;
            
            int num = actual_length % 4;
            int another_num = actual_length / 4;

            int const_padding = -1;
            if(const_padding <= count){
                count = 0;
                flag = 0;
            }
            
            if((dir_entry->rec_len > actual_length) && (padding >= expected_size)){
                int use_length = (num == 0) ? (4 * another_num) : (4 * (another_num + 1));

                if(flag == 0){
                    int const_padding = padding;
                    prev_initial = (struct ext2_dir_entry *)data_pointer;
                    flag = 1;
                }

                data_tracker += use_length;
                data_pointer += use_length;

                count += use_length;
                continue;
            }
            
            prev = (struct ext2_dir_entry *)data_pointer;
            data_tracker += dir_entry->rec_len;
            data_pointer += dir_entry->rec_len;

            if(flag == 1){
                count += dir_entry->rec_len;
            }
        }
        data_index++;
    }

    if(inode_number < 0){
        // return error
    }
    
    struct ext2_inode inode = inode_table[inode_number - 1];

    // check inode in inode_bitmap
    unsigned char *inode_bitmap = disk + (1024 * group_desc->bg_inode_bitmap);
    int inode_byte = (inode_number - 1) / 8;
    int inode_position = (inode_number - 1) % 8;
    if((inode_bitmap[inode_byte] >> inode_position) & 1 == 1){
        // return error
    }

    // check i_mode of inode
    if (S_ISDIR(inode.i_mode)){
        // return error
        return EISDIR;
    }
    // it is a file
    else if(S_ISREG(inode.i_mode)){
        // works
    }
    else{
        // if it is a symbolic link not sure
        // unknown file type return error
    }

    // get data blocks need to use
    int used_index = 0;

    unsigned char *data_bitmap = disk + (1024 * group_desc->bg_block_bitmap);

    while((inode.i_block[used_index] != 0) && (used_index != 12)){
        int current_index = inode.i_block[used_index] - 1;

        int data_byte = current_index / 8;
        int data_position = current_index % 8;

        if((data_bitmap[data_byte] >> data_position) & 1 == 1){
            // return error
        }
        used_index++;
    }

    // recover dir_entry
    current_dir_entry->rec_len = prev_dir_entry->rec_len - new_rec;
    prev_dir_entry->rec_len = new_rec;
    
    // Go to inode bimap, make inode entry 1
    inode_bitmap[inode_byte] ^= (1 << (inode_position));
    inode.i_dtime = 0; // reset deletion time
    

    // change data_bitmap back
    // Go to data bitmap, make entries from list 1
    int data_count = 0;

    used_index = 0;
    while((inode.i_block[used_index] != 0) && (used_index != 12)){
        int current_index = inode.i_block[used_index] - 1;

        int data_byte = current_index / 8;
        int data_position = current_index % 8;

        data_bitmap[data_byte] ^= (1 << (data_position));
        used_index++;
    }
    return 0;  
}
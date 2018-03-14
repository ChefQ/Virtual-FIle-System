#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "ext2_utils.c"
#include <time.h>
#include <math.h>
#include <libgen.h>
int main(int argc, char **argv){
  struct stat sb; // information about the file
  char * t_file; //target file
  int split;
  char path1[PATH_MAX];
  if(argc != 4){
    fprintf(stderr, "Usage: readimg <image file name file name>\n");
    exit(1);
  }

  int fd = open(argv[1], O_RDWR); // opens the img file

  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  //arr_counter = usedInodes(disk);
  if(disk == MAP_FAILED) {
     perror("mmap");
      exit(1);
  }

  char actualpath [PATH_MAX+1];
  char *ptr;

  /*Check 1: if path in the operating system is a file*/
  ptr = realpath(argv[2], actualpath);
  if(ptr == NULL){
    fprintf(stderr,"ERROR: %s\n", strerror(ENOENT) );

    exit(ENOENT);
  }

  lstat(ptr, &sb);

  int blocks = ceil((float)sb.st_size/EXT2_BLOCK_SIZE);
  // checks if path in the operating system is a file
  if( !(S_ISREG(sb.st_mode)) ){
    fprintf(stderr,"ERROR: %s\n", strerror(EISDIR) );
    exit(EISDIR);
  }
  /* Check 1 ends*/

  /*Check 2 if path in img exists and if the target file doesn't exist*/


  for(split = strlen(argv[3])-1; split >= 0; split --){

    if (argv[3][split] == '/'){
      strncpy(path1,argv[3],split+1);
      //path[split+1] = '\0';
      break;
    }

    t_file = argv[3];
    t_file = t_file + split;

  }


         char *c_path = argv[3];
         char cpy_path[strlen(c_path) + 1];

         strcpy(cpy_path, c_path);
         cpy_path[strlen(c_path) + 1] = '\0';

         char * path = dirname(cpy_path);


  unsigned char *IN_Table = disk + (5 * EXT2_BLOCK_SIZE); // going straight to inode table

  int node = findPath(path, IN_Table);
  if(node == -1){
    fprintf(stderr,"ERROR: %s\n", strerror(ENOENT) );
    exit(ENOENT);
  }

  if (validDirectory( node, t_file, IN_Table ) == EEXIST){
    fprintf(stderr,"ERROR: %s\n", strerror(EEXIST) );
    exit(EEXIST);
  }
  /* Check 2 ends*/

  /* Check 3: if there is space in the img to copy the file*/
  if(sb.st_size > freeBlocks(disk)*EXT2_BLOCK_SIZE){
    fprintf(stderr,"ERROR: %s\n", strerror(ENOSPC) );
    exit(ENOSPC);
  }


  /* Check 3 ends*/
  int free_inode = freeInode(disk);
  /*Step 1: create the targer file in the img*/
  struct ext2_inode * inode =  (struct ext2_inode *)
                          ( IN_Table + ( sizeof(struct ext2_inode)* ( node-1 ) ) );
  int dest_size = sizeof(Entry.inode) + sizeof(Entry.rec_len)
                + sizeof(Entry.name_len) + sizeof(Entry.file_type)
                + (int) strlen(t_file);
  int flag = 0;
  inode->i_links_count += 1;
  for(int direct_p = 0;
    (inode->i_block[direct_p] != 0) && (direct_p != 12); direct_p++){

    unsigned char * pointer =  BLOCK_OFFSET(inode->i_block[direct_p], disk + 1024);

    int counter = 0;// keeping track block size
    while (counter < EXT2_BLOCK_SIZE ){ // counter
      unsigned char * dest_pointer = pointer;
      struct ext2_dir_entry * dir_entry =  (struct ext2_dir_entry * )pointer;

      int std_size = sizeof(dir_entry->inode) + sizeof(dir_entry->rec_len)
                    + sizeof(dir_entry->name_len) + sizeof(dir_entry->file_type)
                    + (int) dir_entry->name_len;
      int padding = dir_entry->rec_len - std_size;


      if( std_size%4 != 0){ // standard size isn't a multiple of 4
        padding -= 4 - std_size%4;
        std_size += 4 - std_size%4; // adjusted standard size
      }
      if( (padding > 0) && (padding%4 == 0) && (dest_size <= padding)){

        dir_entry->rec_len =  std_size;
        dest_pointer +=  std_size ; //stopped here

        /*Step 1a: Constructing directory entry for targer file*/
        struct ext2_dir_entry * dest_folder =  (struct ext2_dir_entry * )dest_pointer;
        dest_folder->inode = free_inode;
        dest_folder->rec_len = padding;
        dest_folder->name_len = strlen(t_file);
        dest_folder->file_type = EXT2_FT_REG_FILE ;
        strncpy(dest_folder->name,t_file,dest_folder->name_len);
        /*Step 1a ends*/

        /*Step 1b: Constructing Inode for targer file*/
        struct ext2_inode * dest_inode =  (struct ext2_inode *)
                                  ( IN_Table + ( sizeof(struct ext2_inode)* ( dest_folder->inode-1 ) ) );
            //Review this
        dest_inode->i_mode = 0x8000;
      //  dest_inode->i_uid = 0;
        dest_inode->i_size = sb.st_size;
      //  dest_inode->i_gid = 0;
        dest_inode->i_links_count = 1;
        dest_inode->i_blocks=blocks*2;
        dest_inode->i_ctime = time(NULL); // not sure how to represent time
        //dest_inode->i_flags;
      //  dest_inode->osd1 = 0;

        for(int i = 0; (i < blocks) && ( i < 12 ); i++){

            dest_inode->i_block[i] = freeBlock(disk);

        }
        // do somethinge here
        if(blocks > 12){

          dest_inode->i_block[12] = indirectBlock(disk,blocks);
        }

        dest_inode->i_generation=0;
        dest_inode->i_file_acl=0;
        dest_inode->i_dir_acl=0;
        dest_inode->i_faddr=0;
        /*Step 1b ends*/
        flag = 1;
        break;
      }
      if (flag ==1){
        break;
      }
      pointer +=  dir_entry->rec_len;
      counter+=dir_entry->rec_len;
    }

  }


  /*Step 1 ends*/
  /*Step 2: reading from file  to target file*/

  int fd1 = open(actualpath,O_RDONLY);
  struct ext2_inode * tf_inode =  (struct ext2_inode *)
                            ( IN_Table + ( sizeof(struct ext2_inode)* ( free_inode-1 ) ) );
  unsigned char * offset;
  unsigned char  buf[EXT2_BLOCK_SIZE];
  int count;

  for(int i = 0; (i < blocks) && ( i < 12 ); i++){
    offset = BLOCK_OFFSET(tf_inode->i_block[i], disk + 1024);
    count = read (fd1, buf, EXT2_BLOCK_SIZE);
    if(count == 0){
      break;
    }
    for(int p = 0; p < EXT2_BLOCK_SIZE; p++ ){
      *(offset+ p) = buf[p];

    }
  }

  if(blocks > 12){
      unsigned char * indirection = BLOCK_OFFSET(tf_inode->i_block[12], disk + 1024);
      int * block_number = (int *) indirection;
      for(int i = 0; (i < (blocks-12)); i++){

        offset = BLOCK_OFFSET(*block_number, disk + 1024);
        count = read (fd1, buf, EXT2_BLOCK_SIZE);
        if(count == 0){
          break;
        }
        for(int p = 0; p < EXT2_BLOCK_SIZE; p++ ){
          *(offset + p) = buf[p];

        }
        block_number+=1;
      }
  }
  close(fd1);


}

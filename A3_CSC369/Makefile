FLAGS = -Wall -std=gnu99
DEPENDENCIES = ext2.h

all: ext2_ls ext2_rm ext2_restore ext2_mkdir ext2_cp

ext2_utils: ext2_utils.o
	gcc ${FLAGS} -o $@ $^

ext2_rm: ext2_rm.o
	gcc ${FLAGS} -o $@ $^

ext2_ls: ext2_ls.o
	gcc ${FLAGS} -o $@ $^

ext2_cp: ext2_cp.o -lm
	gcc ${FLAGS} -o $@ $^

ext2_mkdir: ext2_mkdir.o
	gcc ${FLAGS} -o $@ $^

ext2_restore: ext2_restore.o
	gcc ${FLAGS} -o $@ $^

%.o: %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o ext2_ls ext2_rm ext2_restore ext2_cp ext2_mkdir ext2_utils


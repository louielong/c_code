
#include ../../make.config

#KBUILD_EXTRA_SYMBOLS+=$(DRIVER_DIR)/cpld/Module.symvers

obj-m := async_noti.o

#CC=$(CROSS_COMPILE)gcc -DPICA
CC=$(CROSS_COMPILE)gcc
LD=$(CROSS_COMPILE)ld
#CFLAGS     = -std=gnu99 -g -Wall -Werror --pedantic
INC := -I{KERNDIR}/include

#KERNERDIR=../../build_image/linux-source-3.16
KERNERDIR=/lib/modules/`uname -r`/build


PWD := $(shell pwd)

all:async fasync

async:
	make -C $(KERNERDIR) M=$(PWD) modules

fasync:
	#(cd $(KERNDIR) && $(CC) $(INC) -o $(PWD)/fasync -c $(PWD)/fasync.c)
	$(CC) $(INC) -o fasync  fasync.c

all_clean: clean
	rm -fr  *.symvers  *.ko *.order
clean:
	rm -fr *.o *~.core .*.cmd *.mod.c .tmp_versions \
			*.ko *.order  *.symvers fasync


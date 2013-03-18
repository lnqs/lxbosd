obj-m := lxbsod.o PANIC.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD)

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -rf Module.symvers

modules_install:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules_install


ELEVATOR_MODULE = /usr/src/linux-4.19.98/SyscallModule

.PHONY: compile insert remove start issue stop watch_proc clean

compile: producer.c consumer.c wrappers.h
	gcc -o producer.x producer.c
	gcc -o consumer.x consumer.c

insert:
	sudo make -C $(ELEVATOR_MODULE) && sudo insmod $(ELEVATOR_MODULE)/elevator.ko
remove:
	sudo rmmod elevator

start:	compile
	./consumer.x --start
issue:	compile
	./producer.x
stop:	compile
	./consumer.x --stop

cat_proc:
	while [ 1 ]; do \
		clear; clear; \
		sudo cat /proc/elevator; \
		sleep 1; \
	done
watch_proc:
	sudo watch tail -n 200 /proc/elevator
clean:
	rm producer.x consumer.x

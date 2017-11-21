CC := gcc
LIB_DIRECTORY := -L/usr/lib/arm-linux-gnueabihf
INC_DIRECTORY := -I/usr/include/bluetooth
LIB := /usr/lib/arm-linux-gnueabihf/libbluetooth.a
CFLAGS := -Wall -fPIC

.c.o:
	$(CC) $(INC_DIRECTORY) $(CFLAGS) -c $<

all:
	make tool
	make lib

tool:   bletool.o
	gcc -o bletool $(INC_DIRECTORY) $(LIB_DIRECTORY) $< -lbluetooth

lib:    bletool.o
	gcc $(CFLAGS) -shared -o libbletool.so $< $(LIB)
	install -m 644 libbletool.so ../

clean:
	rm -f bletool bletool.o libbletool.o libbletool.so

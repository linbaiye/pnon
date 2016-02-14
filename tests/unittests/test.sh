gcc test_ringbuffer.c -I ../../src/  -std=c99 -lcmocka -Wl,--wrap=recvfrom
./a.out

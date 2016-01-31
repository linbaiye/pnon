gcc test_protocol.c -I ../../src/  -std=c99 -lcmocka -Wl,--wrap=recvfrom

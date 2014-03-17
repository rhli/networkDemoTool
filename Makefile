CC=gcc
CLIB=pthread

compile:
	${CC} test_server.c -o test_server -l ${CLIB}
#	${CC} error_test_server.c -o error_test_server -l ${CLIB}
#	${CC} fake_client.c -o fake_client -l ${CLIB}
	${CC} test_client1.c -o test_client1 -l ${CLIB}
	${CC} test_client2.c -o test_client2 -l ${CLIB}
	${CC} test_client3.c -o test_client3 -l ${CLIB}

clean:
	rm test_server test_client1 test_client2 test_client3 *~

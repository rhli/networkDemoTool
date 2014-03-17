# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <pthread.h>

# define MAX_CLIENT_NUM 10
# define MAX_USERNAME 50
# define MAX_MSG_LENGTH 300
# define MAX_UNREAD_MSG 10

//both IP address and Port number are stored in network byte ordering
typedef struct userinfo{
	unsigned int IP_addr;
	unsigned short Port_num;
	char username[MAX_USERNAME];
	int connected;
}userinfo;




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

userinfo ui[MAX_CLIENT_NUM];
int listen_sd;
int listen_ready=0;
unsigned short listening_port_num;
pthread_t listen_thread;
pthread_t func_thread;

int main(int argc, char** argv){
	if(argc!=3){
		printf("Usage: %s (server IP)(server Port_num)\n",argv[0]);
		exit(0);
	}
	int i;
	int choice;
	int sd[11];
	struct in_addr container;
	struct sockaddr_in server_addr[11];
	for(i=0;i<5;i++){
		//Connect to the server
		sd[i]=socket(AF_INET,SOCK_STREAM,0);
		memset(&server_addr[i],0,sizeof(server_addr[i]));
		server_addr[i].sin_family=AF_INET;
		server_addr[i].sin_addr.s_addr=inet_addr(argv[1]);
		server_addr[i].sin_port=htons(atoi(argv[2]));
		if(connect(sd[i],(struct sockaddr *)(server_addr+i),sizeof(server_addr[i]))<0){
			perror("connect");
			exit(0);
		}
		unsigned char buff[200];
		memset(buff,0,100);
		buff[0]=0x01;
		char username[2];
		username[0]='a'+i;
		username[1]='\0';
		int namelen=1;
		namelen=htonl(namelen);
		int port_len=sizeof(unsigned short);
		port_len=htonl(port_len);
		listening_port_num=10000+i;
		if(i==2){
			listening_port_num--;
		}
		if(i==3){
			username[0]='a';
		}
		listening_port_num=htons(listening_port_num);
		memcpy(buff+sizeof(char),&namelen,sizeof(int));
		memcpy(buff+sizeof(char)+sizeof(int),username,ntohl(namelen));
		memcpy(buff+sizeof(char)+sizeof(int)+ntohl(namelen),&port_len,sizeof(int));
		memcpy(buff+sizeof(char)+sizeof(int)+ntohl(namelen)+sizeof(int),&listening_port_num,sizeof(unsigned short));
		int len;
		printf("\n");
		int already_sent=0;
		int msg_len=sizeof(char)+sizeof(int)+ntohl(namelen)+sizeof(int)+sizeof(unsigned short);
		while(1){
			if((len=send(sd[i],buff+already_sent,msg_len-already_sent,0))<0){
				printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
				exit(0);
			}
			already_sent+=len;
			if(already_sent>=msg_len){
				break;
			}
		}
		int received_len=0;
		int success=0;
		while(1){
			if((len=recv(sd[i],buff+received_len,100,0))<0){
				printf("Recv Error: %s (Errno:%d)\n",strerror(errno),errno);
				exit(0);
			}
			received_len+=len;

			if(buff[0]==0xff){
				if(received_len<6){
					continue;
				}
				if(buff[1+sizeof(int)]==0x01){
					printf("Username been registered\n");
					break;
				}
				if(buff[1+sizeof(int)]==0x02){
					printf("Same IP address and port num have been used\n");
					break;
				}
				if(buff[1+sizeof(int)]==0x03){
					printf("User number overflow. Try again later\n");
					break;
				}
			}else if(buff[0]==0x02){
				printf("Registration completed:%s\n",username);
				success=1;
				break;
			}
		}
		if(success==0){
			continue;
		}
		buff[0]=0x03;
		if((len=send(sd[i],buff,1,0))<0){
			printf("Send Error: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		received_len=0;
		while(1){
			if((len=recv(sd[i],buff+received_len,200-received_len,0))<0){
				printf("Recv Error: %s (Errno:%d)\n",strerror(errno),errno);
				exit(0);
			}
			received_len+=len;
			int total_len;

			//reconstruct client list
			if(received_len>=5){
				memcpy(&total_len,buff+1,sizeof(int));
				total_len=ntohl(total_len);
			}else{
				continue;
			}
			int back_i=i;
			if(received_len==total_len+1+sizeof(int)){
				int current_len=1+sizeof(int);
				for(i=0;i<MAX_CLIENT_NUM-1;i++){
					int namelen;
					memcpy(&namelen,buff+current_len,sizeof(int));
					namelen=ntohl(namelen);
					current_len+=sizeof(int);
					memcpy(ui[i].username,buff+current_len,namelen);
					current_len+=namelen;
					memcpy(&ui[i].IP_addr,buff+current_len,sizeof(int));
					current_len+=sizeof(int);
					memcpy(&ui[i].Port_num,buff+current_len,sizeof(unsigned short));
					current_len+=sizeof(unsigned short);
					container.s_addr=ui[i].IP_addr;
					printf("No. %d: name %s IP %s Port %d\n",i,ui[i].username,inet_ntoa(container),ntohs(ui[i].Port_num));
					if(current_len==(total_len+1+sizeof(int))){
						break;
					}
				}
				i=back_i;
				break;
			}else{
				continue;
			}
		}
	}
	
	for(i=0;i<5;i++){
		close(sd[i]);
	}
	return 0;
}

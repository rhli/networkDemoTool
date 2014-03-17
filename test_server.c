# include <stdio.h>
# include <stdlib.h>
# include <unistd.h>
# include <string.h>
# include <errno.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <netinet/in.h>
# include <pthread.h>

# define PORT 12300
# define MAX_CLIENT_NUM 10
# define MAX_USERNAME 50

//both IP address and Port number are stored in network byte ordering
typedef struct userinfo{
	unsigned int IP_addr;
	unsigned short Port_num;
	char username[MAX_USERNAME];
}userinfo;

int client_number=0;
pthread_t threads[MAX_CLIENT_NUM];
int cs_sd[MAX_CLIENT_NUM];
//whether a slot is available
int availability[MAX_CLIENT_NUM];
//whether a slot finishes registration
int completement[MAX_CLIENT_NUM];
userinfo ui[MAX_CLIENT_NUM];
struct sockaddr_in client_addr[MAX_CLIENT_NUM];
pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;

void* cs_connection(void* args){
	//initialization
	int i;
	struct in_addr container;
	int* ava_p=(int*) args;
	int ava=*ava_p;
	int sd=cs_sd[ava];
	unsigned char buff[1000];
	int len;
	int received_len=0;
	while(1){
	//receive port num and screen name
		len=recv(sd,buff+received_len,sizeof(buff),0);
		if(len==0){
			printf("UNREGISTERED USER DEPARTURE:\n");
			container.s_addr=ui[ava].IP_addr;
			printf("IP %s Port %d\n",inet_ntoa(container),ntohs(ui[ava].Port_num));
			pthread_mutex_lock(&mutex);
			availability[ava]=1;
			completement[ava]=0;
			client_number--;
			pthread_mutex_unlock(&mutex);
			pthread_exit(NULL);
		}
		int usernamelen;
		int useripaddr;
		unsigned short userportnum;
		received_len+=len;
		if(received_len<=5){
			continue;
		}else{
			memcpy(&usernamelen,buff+1,sizeof(int));
			usernamelen=ntohl(usernamelen);
			printf("USERNAME LENGTH:%d\n",usernamelen);
			if(received_len<usernamelen+11){
				continue;
			}
		}
		memcpy(ui[ava].username,buff+1+sizeof(int),usernamelen);
		memcpy(&ui[ava].Port_num,buff+1+sizeof(int)+usernamelen+sizeof(int),sizeof(unsigned short));
		ui[ava].username[usernamelen]='\0';

		//Below is the registration process
		pthread_mutex_lock(&mutex);
		for(i=0;i<MAX_CLIENT_NUM;i++){
			if((ava!=i)&&(completement[i]==1)){
				if((ui[i].IP_addr==ui[ava].IP_addr)&&(ui[i].Port_num==ui[ava].Port_num)){
					//Send error message 0x02
					char error_msg[7];
					memset(error_msg,0,7);
					int temp=1;
					temp=htonl(temp);
					error_msg[0]=0xff;
					memcpy(error_msg+1,&temp,sizeof(int));
					error_msg[1+sizeof(int)]=0x02;
					error_msg[2+sizeof(int)]=0x00;
					int msg_len=6;
					int already_sent=0;
					while(1){
						int len=send(sd,error_msg+already_sent,msg_len-already_sent,0);
						if(len<=0){
							perror("send()");
						}else{
							already_sent+=len;
						}
						if(already_sent>=msg_len){
							break;
						}
					}
					printf("ERROR: Same IP address:%s and port number have been registered %d\n",inet_ntoa(ui[i].IP_addr),ntohs(ui[i].Port_num));
					pthread_mutex_unlock(&mutex);
					break;
				}
				if(strcmp(ui[i].username,ui[ava].username)==0){
					//Send error message 0x01
					char error_msg[7];
					memset(error_msg,0,7);
					int temp=1;
					temp=htonl(temp);
					error_msg[0]=0xff;
					memcpy(error_msg+1,&temp,sizeof(int));
					error_msg[1+sizeof(int)]=0x01;
					error_msg[2+sizeof(int)]=0x00;
					int msg_len=6;
					int already_sent=0;
					while(1){
						int len=send(sd,error_msg+already_sent,msg_len-already_sent,0);
						if(len<=0){
							perror("send()");
						}else{
							already_sent+=len;
						}
						if(already_sent>=msg_len){
							break;
						}
					}
					printf("ERROR: Duplicate Username\n");
					pthread_mutex_unlock(&mutex);
					break;
				}
			}
		}
		if(i!=MAX_CLIENT_NUM){
			received_len=0;
			continue;
		}
		completement[ava]=1;
		pthread_mutex_unlock(&mutex);
		printf("REGISTRATION SUCCEED:\n");
		printf("***Username      :%s\n",ui[ava].username);
		container.s_addr=ui[ava].IP_addr;
		printf("***IP address    :%s\n",inet_ntoa(container));
		printf("***Listening Port:%d\n",ntohs(ui[ava].Port_num));
		char reg_success[2];
		reg_success[0]=0x02;
		reg_success[1]=0x00;
		len=send(sd,reg_success,strlen(reg_success),0);
		if(len<=0){
			perror("send()");
		}
		break;
	}
	
	while(1){
		//waiting for request for client list
		len=recv(sd,buff,sizeof(buff),0);
		if(len==0){
			printf("USER DEPARTURE:\n");
			container.s_addr=ui[ava].IP_addr;
			printf("No. %d: name %s IP %s Port %d\n",ava,ui[ava].username,inet_ntoa(container),ntohs(ui[ava].Port_num));
			close(sd);
			pthread_mutex_lock(&mutex);
			availability[ava]=1;
			completement[ava]=0;
			client_number--;
			pthread_mutex_unlock(&mutex);
			pthread_exit(NULL);
		}else{
			//TODO: Organizing and sending list
			char list_buff[1000];
			int current_len=0;
			char com_c[2];
			com_c[0]=0x04;
			com_c[1]=0x00;
			memcpy(list_buff,com_c,1);
			current_len=5;
			pthread_mutex_lock(&mutex);
			for(i=0;i<MAX_CLIENT_NUM;i++){
				if(completement[i]==1){
					int namelen=strlen(ui[i].username);
					int bnamelen=htonl(namelen);
					memcpy(list_buff+current_len,&bnamelen,sizeof(int));
					current_len+=sizeof(int);
					memcpy(list_buff+current_len,ui[i].username,namelen);
					current_len+=namelen;
					memcpy(list_buff+current_len,&ui[i].IP_addr,sizeof(int));
					current_len+=sizeof(int);
					memcpy(list_buff+current_len,&ui[i].Port_num,sizeof(unsigned short));
					current_len+=sizeof(unsigned short);
				}
			}
			current_len-=5;
			current_len=htonl(current_len);
			memcpy(list_buff+1,&current_len,sizeof(int));
			pthread_mutex_unlock(&mutex);
			int msg_len=ntohl(current_len)+5;
			int already_sent=0;
			while(1){
				int len=send(sd,list_buff+already_sent,msg_len-already_sent,0);
				if(len<=0){
					perror("send()");
				}else{
					already_sent+=len;
				}
				if(already_sent>=msg_len){
					break;
				}
			}
		}
	}
}

int main(int argc, char** argv){
	int i;
	int lis_port;
	if(argc==2){
		lis_port=atoi(argv[1]);
	}else{
		lis_port=PORT;
	}
	//Initialization
	for(i=0;i<MAX_CLIENT_NUM;i++){
		availability[i]=1;
		completement[i]=0;
	}

	int sd=socket(AF_INET,SOCK_STREAM,0);
	long val=1;
	if(setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(long))==-1){
		perror("setsockopt");
		exit(1);
	}
	int client_sd;
	struct sockaddr_in server_addr;
	struct sockaddr_in c_addr;
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family=AF_INET;
	server_addr.sin_addr.s_addr=htonl(INADDR_ANY);
	server_addr.sin_port=htons(lis_port);
	if(bind(sd,(struct sockaddr *) &server_addr,sizeof(server_addr))<0){
		printf("bind error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	if(listen(sd,MAX_CLIENT_NUM+1)<0){
		printf("listen error: %s (Errno:%d)\n",strerror(errno),errno);
		exit(0);
	}
	while(1){
		int addr_len=sizeof(c_addr);
		if(client_number==MAX_CLIENT_NUM){
			continue;
		}
		if((client_sd=accept(sd,(struct sockaddr *) &c_addr,&addr_len))<0){
			printf("accept erro: %s (Errno:%d)\n",strerror(errno),errno);
			exit(0);
		}
		printf("GETTING CONNECTION REQUEST FROM IP: %s Port: %d\n",inet_ntoa(c_addr.sin_addr),ntohs(c_addr.sin_port));
		//find an available slot
		pthread_mutex_lock(&mutex);
		for(i=0;i<MAX_CLIENT_NUM;i++){
			if(availability[i]==1){
				break;
			}
		}
		if(i==MAX_CLIENT_NUM){
			//sending error msg
			printf("ERROR: Client number overflow\n");
			char error_msg[3];
			error_msg[0]=0xff;
			error_msg[1]=0x03;
			error_msg[2]=0x00;
			int msg_len=strlen(error_msg);
			int already_sent=0;
			while(1){
				int len=send(client_sd,error_msg+already_sent,strlen(error_msg)-already_sent,0);
				if(len<=0){
					perror("send()");
				}else{
					already_sent+=len;
				}
				if(already_sent>=msg_len){
					break;
				}
			}
			close(client_sd);
			pthread_mutex_unlock(&mutex);
			continue;
		}
		availability[i]=0;
		client_number++;
		pthread_mutex_unlock(&mutex);
		int index=i;
		cs_sd[index]=client_sd;
		memcpy(client_addr+index,&c_addr,sizeof(c_addr));
		ui[index].IP_addr=c_addr.sin_addr.s_addr;
		ui[index].Port_num=c_addr.sin_port;
		//create new thread
		int return_value=pthread_create(&threads[index],NULL,cs_connection,&index);
	}
	return 0;
}

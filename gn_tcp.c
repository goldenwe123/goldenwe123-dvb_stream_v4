#include "gn_tcp.h"
#include <pthread.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define SERV_PORT 8753
#define MAXNAME 1024

 int socket_fd;      /* file description into transport */
 int recfd;     /* file descriptor to accept        */
 int length;     /* length of address structure      */
 int nbytes;     /* the number of read **/
 struct sockaddr_in myaddr; /* address of this service */
 struct sockaddr_in client_addr; /* address of client    */
 
char tcp_buffer[TCP_BUFFER_NUMS][188];
char tcp_audio_buffer[TCP_BUFFER_NUMS][188];
pthread_mutex_t mutex_video = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_audio = PTHREAD_MUTEX_INITIALIZER;
sem_t semaphore_video; //flag for video
sem_t semaphore_audio; //flag for audio

uint64_t video_pts;
uint64_t audio_pts;

uint64_t tcp_get_pts(char* pts)
{
	uint64_t pts_0 = (pts[0] >> 1) & 0x07;
	uint64_t pts_1 = pts[1];
	uint64_t pts_2 = pts[2] >> 1;
	uint64_t pts_3 = pts[3];
	uint64_t pts_4 = pts[4] >> 1;
	
	return (pts_0 << 30) + (pts_1 << 22) + (pts_2 << 15) + (pts_3 << 7) + (pts_4 );
	
}

uint64_t gn_tcp_get_video_pts()
{
		return video_pts;
}

uint64_t gn_tcp_get_audio_pts()
{
		return audio_pts;
}

		

int isVideo()
{
	
	if(tcp_buffer[tcp_head][4] == 0x00 && tcp_buffer[tcp_head][5] == 0x00 && tcp_buffer[tcp_head][6] == 0x01 && tcp_buffer[tcp_head][7] == 0xE0) {
		
		return 1;
	}

		return 0;
	
}

int isAudio()
{
	
	if(tcp_audio_buffer[tcp_audio_head][4] == 0x00 && tcp_audio_buffer[tcp_audio_head][5] == 0x00 && tcp_audio_buffer[tcp_audio_head][6] == 0x01 && tcp_audio_buffer[tcp_audio_head][7] == 0xC0) {
		
		return 1;
	}

		return 0;
	
}

int isVideoEmpty()
{
	
		
	if(  tcp_head == tcp_tail ) {
		//printf("RTP no data\n");
		return 1;
	}	

		return 0;
	
}

int isAudioEmpty()
{
	
		
	if(  tcp_audio_head == tcp_audio_tail ) {
		//printf("RTP no data\n");
		return 1;
	}	

		return 0;
	
}			
			


void gn_tcp_init()
{
	
	tcp_head = 0;
	tcp_tail = 0;
	tcp_audio_head =0;
    tcp_audio_tail =0;
	video_pts = 0;
	audio_pts = 0;
	sem_init(&semaphore_video, 1, 0);
	sem_init(&semaphore_audio, 1, 0);
	
/*                             
 *      Get a socket into TCP/IP
 */
 if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
  perror ("socket failed");
  exit(1);
 }
 
 // int flags = fcntl(socket_fd, F_GETFL, 0);
 // fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);

 
 /*
 *    Set up our address
 */
 bzero ((char *)&myaddr, sizeof(myaddr));
 myaddr.sin_family = AF_INET;
 myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
 myaddr.sin_port = htons(SERV_PORT);
 
 /*
 *     Bind to the address to which the service will be offered
 */
 if (bind(socket_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) <0) {
  perror ("bind failed");
  exit(1);
 }

}

void gn_tcp_listener()
{
	
	if (listen(socket_fd, 20) <0) {
	  perror ("listen failed");
	  exit(1);
	}
	memset(&client_addr, '\0', sizeof(client_addr)); // zero structure out 
	length = sizeof(client_addr);
    perror("Server is ready to receive !!\n");
    perror("Can strike Cntrl-c to stop Server >>\n");
	
	while (1) {
		
		recfd = accept(socket_fd, NULL, NULL);

		if (recfd == -1 && errno == EAGAIN) {

           // fprintf(stderr, "no client connections yet\n");
            continue;
        } else if (recfd == -1) {

            perror("accept failed");

            return;
        }
			int flags = fcntl(recfd, F_GETFL, 0);
			fcntl(recfd, F_SETFL, flags | O_NONBLOCK);
			break;
	}
	
	
//	char *s = inet_ntoa(client_addr.sin_addr);
//	printf("IP address: %s\n", s);
	printf("The Server is connected\n");
	// struct timeval timeout={3,0};//3s
	// setsockopt(recfd,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof(timeout));
	// int opt=SO_REUSEADDR;
        // setsockopt(recfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	// opt = 1000000;
	// int optlen = sizeof(int);
	// setsockopt(recfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
	// getsockopt(recfd,SOL_SOCKET,SO_REUSEADDR,&opt,&optlen);
	// printf("!!!!!!SOCKET SO_REUSEADDR %d\n", opt);
	// opt = 100000000;
	// setsockopt(recfd,SOL_SOCKET,SO_SNDBUF,&opt,sizeof(int));
	// getsockopt(recfd,SOL_SOCKET,SO_SNDBUF,&opt,&optlen);
	// printf("!!!!!!SOCKET SNTBUF %d\n", opt);
}

void gn_tcp_put(char* data)
{
	
	pthread_mutex_lock(&mutex_video); 
	if( tcp_head == ( (tcp_tail+1) % TCP_BUFFER_NUMS ) ){
		
		//while(1) {
			//printf("****************TCP video buffer full*********** %d %d \n",tcp_head,tcp_tail);
		//}
		pthread_mutex_unlock(&mutex_video);
		return ;
	}
	
	memcpy(tcp_buffer[tcp_tail] , data , 188);
	
	tcp_tail++;
	tcp_tail %= TCP_BUFFER_NUMS;
	pthread_mutex_unlock(&mutex_video);
	sem_post(&semaphore_video);
	//printf("TCP video buffer  %d %d \n",tcp_head,tcp_tail);
	return;
		
}

/*
ssize_t  gn_tcp_send(void *buf)
{
	

	
	length = send(recfd,buf, 188, 0  );
	if(length == -1) {
	   perror ("write to client error");
	   exit(1);
	}
	return length;
	
}		
*/
ssize_t  gn_tcp_send()
{	
	//printf("tcp wait\n");
	sem_wait(&semaphore_video);       
	pthread_mutex_lock(&mutex_video); 
	//printf("tcp send\n");
	
	 //for(int i=0 ;i<4 ;i++) {
			// tcp_buffer[tcp_head][i]=i;
			//printf("%x ",tcp_buffer[tcp_head][i]);
	// }	
	//printf("\n");
	int count_temp = 0;
	// clock_t start, finish;
	// double duration;
	// start = clock();
	while(count_temp != 188 && !IsSocketClosed()) {
		length = send(recfd, &tcp_buffer[tcp_head][count_temp], 188-count_temp, MSG_NOSIGNAL  );
		//printf("send %d\n",length);
		if(length <0)
			continue;
		count_temp+=length;
	}
	// finish = clock();
	// duration = (double)(finish - start) / CLOCKS_PER_SEC;
	// printf( "%f seconds\n", 188/ duration );
	if(length == -1) {
	   tcp_head++;
       tcp_head %= TCP_BUFFER_NUMS;
	   perror ("Video write to client error");
	   usleep(50000);
	   pthread_mutex_unlock(&mutex_video);
	   return length;
	}
	
	
	tcp_head++;
    tcp_head %= TCP_BUFFER_NUMS;
	pthread_mutex_unlock(&mutex_video);
	
	return length;
	
}

ssize_t  gn_tcp_read( char* buff)
{
	ssize_t len = read(recfd, buff, sizeof(buff));
	
	return len;
}	

void gn_tcp_audio_put(char* data)
{
	pthread_mutex_lock(&mutex_video); 
	//printf("RTP video buffer %d %d \n",rtp_head,rtp_tail);
	if( tcp_audio_head == ( (tcp_audio_tail+1) % TCP_BUFFER_NUMS ) ){
		
		pthread_mutex_unlock(&mutex_video);
		return ;
	}
	

	memcpy(tcp_audio_buffer[tcp_audio_tail] , data , 188);
	
	tcp_audio_tail++;
	tcp_audio_tail %= TCP_BUFFER_NUMS;
	pthread_mutex_unlock(&mutex_video);
	sem_post(&semaphore_audio);
	//printf("TCP audio buffer  %d %d \n",tcp_head,tcp_tail);
	return;
		
}

ssize_t  gn_tcp_audio_send()
{
	//printf("audio wait\n");
	sem_wait(&semaphore_audio);
	pthread_mutex_lock(&mutex_video); 
	length = send(recfd, tcp_audio_buffer[tcp_audio_head], 188, MSG_NOSIGNAL  );
	//printf("audio send\n");
	if(length == -1) {
	   perror ("Audio write to client error");
	   usleep(10000);
	   pthread_mutex_unlock(&mutex_video);
	   return length;
	}
	
	tcp_audio_head++;
    tcp_audio_head %= TCP_BUFFER_NUMS;
	pthread_mutex_unlock(&mutex_video);
		
	return length;
	
}

int IsSocketClosed()  
{  
 char buff[32];  
 int recvBytes = recv(recfd, buff, sizeof(buff), MSG_PEEK);  
   
 int sockErr = errno;  
   
 //cout << "In close function, recv " << recvBytes << " bytes, err " << sockErr << endl;  
   
 if( recvBytes > 0) //Get data  
  return 0;  
   
 if( (recvBytes == -1) && (sockErr == EWOULDBLOCK) ) //No receive data  
  return 0;  
     
 return 1;  
}  

void gn_tcp_clear()
{
	tcp_head=0;
	tcp_tail=0;
	sem_init(&semaphore_video, 1, 0);
}	

	

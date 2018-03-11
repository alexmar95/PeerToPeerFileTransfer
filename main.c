#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string.h>

#include"helper.h"
#define SERVER_PORT 5678
#define RECEIVER_PORT 8765
#define PL 1024

char peers[10][20];
int peer_count;
pthread_mutex_t mutex;
char *dir_path;

void parseFolder(char *fpath){

}

void receive_file(){
  int sockfd,connfd;
  char buf[1024];
  struct sockaddr_in local_addr, rmt_addr;
  socklen_t rlen;
  char filePath[511];
  
  sockfd = socket(PF_INET,SOCK_STREAM, 0);
  set_addr(&local_addr, NULL, INADDR_ANY, RECEIVER_PORT);
  bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
  listen(sockfd,5);
  while(1){
    rlen = sizeof(rmt_addr);
    connfd = accept(sockfd, (struct sockaddr *)&rmt_addr, &rlen);

    stream_read(connfd,(void *)buf,1);
    int fnameLength = *buf;
    if(stream_read(connfd,(void *)buf,fnameLength)!=fnameLength){
      printf("Couldn't read entrire file name on receive\n");
    }
    buf[fnameLength]='\0';
    snprintf(filePath,510,"%s%s",dir_path,buf);

    stream_read(connfd, (void *)buf, sizeof(int));
    fnameLength = *(int *)buf;
    if(stream_read(connfd, (void *)buf, fnameLength)!=fnameLength){
      printf("Couldn't read entire file <%s>!\n", filePath);
    }
    FILE *f=fopen(filePath, "wb");
    if(!f)
      printf("Couldn't open %s\n", filePath);
    fwrite(buf, sizeof(char), fnameLength, f);
  }
}

void *upload(void *arg){
  int sockfd,connfd;
  char buf[1024];
  struct sockaddr_in local_addr, rmt_addr;
  socklen_t rlen;
  char filePath[511];
  
  sockfd = socket(PF_INET,SOCK_STREAM, 0);
  set_addr(&local_addr, NULL, INADDR_ANY, SERVER_PORT);
  bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
  listen(sockfd,5);
  while(1){
    rlen = sizeof(rmt_addr);
    connfd = accept(sockfd, (struct sockaddr *)&rmt_addr, &rlen);
    while(0 < stream_read(connfd,(void *)buf,1)){
      int fnameLength = *buf;
      if(stream_read(connfd,(void *)buf,fnameLength)!=fnameLength){
	printf("Couldn't read entrire file name");
      }
      buf[fnameLength]='\0';
      snprintf(filePath,510,"%s%s",dir_path,buf);
      printf("Requested file:%s\n",filePath);
      //send_file(filePath);
    }
  }
  
  return NULL;
}
void *download(void *arg){
  char *file = (char *)arg;
  char flength = strlen(file);
  for(int i=0;i<peer_count;i++){
    int sockfd;
    struct sockaddr_in local_addr, remote_addr;
    sockfd = socket(PF_INET,SOCK_STREAM,0);
    set_addr(&local_addr,NULL,INADDR_ANY,0);
    bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
    set_addr(&remote_addr,peers[i],0,SERVER_PORT);
    connect(sockfd, (struct sockaddr *)&remote_addr,sizeof(remote_addr));
    stream_write(sockfd,&flength,1);
    stream_write(sockfd,file,flength);
  }
  
  return NULL;
}

void readIPAddresses(){
  FILE *f;
  f = fopen("config.txt","r");
  int i=0;
  while(fscanf(f,"%19s",peers[i++])>0);
  peer_count = i;
  fclose(f);
}

int main(int argc, char *argv[]){
  if(argc!=2){
    printf("Bad arguments");
    return (int)'@';
  }
  dir_path = argv[1];
  readIPAddresses();
  
  pthread_t thread;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,1);
  pthread_mutex_init(&mutex, NULL);

  if(0 != (pthread_create(&thread, &attr, upload,NULL))){
    printf("Upload thread failed to start\n");
    return (int)'@';
  }
  char file_name[256];
  int count;
  while((count = scanf("%255s",file_name))){
    if(!strcmp(file_name,"stop"))
      break;
    if(0 != (pthread_create(&thread, &attr, download,file_name))){
      printf("Download thread failed to start\n");
      return (int)'@';
    }
  }
  return 0;
}

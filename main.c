#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>

#include"helper.h"
#define SERVER_PORT 5678
#define RECEIVER_PORT 8765
#define PL 8096
#define DOWNLOADS "./downloads/"
#define TEMPS "./temps/"

char peers[10][20];
int peer_count;
pthread_mutex_t mutex;
char *dir_path;

int getFileSize(char *file){
  FILE *f = fopen(file,"rb");
  if(f){
    fseek(f,0,SEEK_END);
    int ret = ftell(f);
    fclose(f);
    return ret;
  }
  return -1;
}

void writeMetadataToFile(char *fname,int fsize){
  char path[512];
  sprintf(path,"%s%s.metadata",TEMPS,fname);
  FILE *f = fopen(path,"w");
  fprintf(f,"%s\n%d",fname,fsize);
  fclose(f);
}

int readFileMetadata(int connfd,char *file){
  char buf[256];
  stream_read(connfd,(void *)buf,1);
  int fsize,fnameLength = buf[0];
  if(stream_read(connfd,(void *)buf,fnameLength)!=fnameLength){
    printf("Couldn't read entire file name on receive\n");
  }
  stream_read(connfd,(void *)&fsize,sizeof(int));
  buf[fnameLength]='\0';
  if(strcmp(file,buf)){
    printf("Did not ask for this file!\n");
    return -1;
  }
  writeMetadataToFile(buf,fsize);
  printf("Metadata read:%s %d\n",file,fsize);
  return fsize;
}

void readFileAndWriteToDisk(int connfd,char *path, int fsize){
  char *buf = malloc(PL);
  int r=0;
  int orig = fsize;
  FILE *f=fopen(path, "wb");
  if(!f)
    printf("Couldn't open %s\n", path);
  else
    {
      printf("fisze:%d\n",fsize);
      while(fsize>0 && ((r=stream_read(connfd,(void*)buf,PL>fsize ? fsize : PL)))!=-1){
	fwrite(buf, sizeof(char), r, f);
	fsize-=r;
      }
      printf("File %s received with size %d\n",path,orig);
      fclose(f);
    }
  free(buf);
  if(r==-1)
    unlink(path);
}

void receiveFile(char *file){
  int sockfd,connfd;
  struct sockaddr_in local_addr, rmt_addr;
  socklen_t rlen;
  
  sockfd = socket(PF_INET,SOCK_STREAM, 0);
  set_addr(&local_addr, NULL, INADDR_ANY, RECEIVER_PORT);
  bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
  listen(sockfd,5);
  
  rlen = sizeof(rmt_addr);
  connfd = accept(sockfd, (struct sockaddr *)&rmt_addr, &rlen);
  char action;
  stream_read(connfd,(void *)&action,1);
  char path[512];
  sprintf(path,"%s%s",DOWNLOADS,file);
  int fsize;
  switch(action){
  case 1:
    fsize = readFileMetadata(connfd,file);
    if(fsize>=0){
      readFileAndWriteToDisk(connfd,path,fsize);
    }
    break;
  case 2:
    //readFile(connfd);
    break;
  default:
    printf("Malformed data!\n");
    break;
  } 
}

int readFileFromDiskAndSend(char *path,int fsize, int sockfd){
  char *buff=malloc(PL);
  FILE *f = fopen(path,"rb");
  if(!f)
    return -1;
  int x;
  while(((x = fread(buff, sizeof(char),PL>fsize ? fsize : PL, f))>=0)&&fsize>0){
    stream_write(sockfd,buff,x);
    fsize-=x;
  }
  free(buff);
  return fsize;
}

void send_file(char *file, char *address){
  int sockfd;
  struct sockaddr_in local_addr, remote_addr;
  char path[512];
  sprintf(path,"%s%s",DOWNLOADS,file);
  sockfd = socket(PF_INET,SOCK_STREAM,0);
  set_addr(&local_addr,NULL,INADDR_ANY,0);
  bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
  set_addr(&remote_addr,address,0,RECEIVER_PORT);
  connect(sockfd, (struct sockaddr *)&remote_addr,sizeof(remote_addr));
  char action = 1;
  int fsize;
  char fnameLength=strlen(file);
  fsize = getFileSize(path);
  stream_write(sockfd,&action,1);
  stream_write(sockfd,(void*)&fnameLength,1);
  stream_write(sockfd,file,fnameLength);
  printf("Sending file %s with size %d\n",file,fsize);
  stream_write(sockfd,(void*)&fsize,sizeof(int));
  printf("Sent %d bytes\n",fsize-readFileFromDiskAndSend(path,fsize,sockfd));
  printf("File %s with size %d sent\n",file,fsize);
}

void *upload(void *arg){
  int sockfd,connfd;
  char buf[1024];
  
  struct sockaddr_in local_addr, rmt_addr;
  socklen_t rlen;
  char filePath[512];
  while(1){
    sockfd = socket(PF_INET,SOCK_STREAM, 0);
    set_addr(&local_addr, NULL, INADDR_ANY, SERVER_PORT);
    bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
    listen(sockfd,5);
    rlen = sizeof(rmt_addr);
    connfd = accept(sockfd, (struct sockaddr *)&rmt_addr, &rlen);
    stream_read(connfd,(void *)buf,1);
    int fnameLength = *buf;
    if(stream_read(connfd,(void *)buf,fnameLength)!=fnameLength){
      printf("Couldn't read entrire file name");
    }
    buf[fnameLength]='\0';
    snprintf(filePath,510,"%s%s",dir_path,buf);
    printf("Requested file:%s\n",filePath);
    if(fileExists(filePath)){
      char address[20];
      strncpy(address,inet_ntoa(rmt_addr.sin_addr),19);
      printf("Preparing to send file to:%s\n",address);
      send_file(buf,address);
    }
    close(connfd);
    close(sockfd);
  }
  
  return NULL;
}

void *download(void *arg){
  char *file = (char *)arg;
  char flength = strlen(file);
  printf("PeerCount=%d\n",peer_count);
  int sockfd;
  for(int i=0;i<peer_count;i++){
    struct sockaddr_in local_addr, remote_addr;
    sockfd = socket(PF_INET,SOCK_STREAM,0);
    set_addr(&local_addr,NULL,INADDR_ANY,0);
    bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr));
    set_addr(&remote_addr,peers[i],0,SERVER_PORT);
    connect(sockfd, (struct sockaddr *)&remote_addr,sizeof(remote_addr));
    stream_write(sockfd,&flength,1);
    stream_write(sockfd,file,flength);
  }
  printf("Preparing to receive file\n");
  receiveFile(file);
  close(sockfd);
  return NULL;
}

void readIPAddresses(){
  FILE *f;
  f = fopen("config.txt","r");
  int i=0;
  while(fscanf(f,"%19s",peers[i++])>0);
  peer_count = i-1;
  fclose(f);
  printf("PeerAddresses:\n");
  for(int i=0;i<peer_count;i++){
    printf("%s\n",peers[i]);
  }
  printf("\n");
}

int main(void){
  dir_path = DOWNLOADS;
  if(!fileExists(dir_path)){
    printf("Creating \"downloads\" folder\n");
    mkdir(dir_path, 0777);
  }
  if(!fileExists(TEMPS)){
    printf("Creating \"temps\" folder\n");
    mkdir(TEMPS, 0777);
  }
  
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
  char file_name[256],fpath[512];
  int count;
  while((count = scanf("%255s",file_name))){
    if(!strcmp(file_name,"stop"))
      break;
    snprintf(fpath,510,"%s%s",dir_path,file_name);
    if(fileExists(fpath)){
      printf("The file already exists\n");
      continue;
    }
    if(0 != (pthread_create(&thread, &attr, download,file_name))){
      printf("Download thread failed to start\n");
      return (int)'@';
    }
  }
  return 0;
}

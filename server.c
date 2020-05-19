#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>

#define PORT "3491"  // the port users will be connecting to
#define MAXDATASIZE 100 
#define BACKLOG 10   // how many pending connections queue will hold

int total_conns = 0;

pthread_mutex_t fastmutex = PTHREAD_MUTEX_INITIALIZER;

void sigchld_handler(int s)
{
  // waitpid() might overwrite errno, so we save and restore it:
  int saved_errno = errno;

  while(waitpid(-1, NULL, WNOHANG) > 0);
  errno = saved_errno;
}


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
unsigned long hash(unsigned char *str)
{
  unsigned long hash = 5381;
  int c;

  while (c = *str++)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

// must be called from handle_client_connection
void send_client_project(int client_fd, char* project_name){
  // tar the project. output is result.tar.gz
  char buffer[32];
  sprintf(buffer, "tar -zcvf result.tar.gz %s", project_name);
  system(buffer);

  // now lets read result.tar.gz so we can send it to client
  int project_descriptor = open("result.tar.gz", O_RDONLY);

  if(project_descriptor < 0){
    perror("couldnt open tar file");
  }

  // get length of file
  off_t fsize;
  fsize = lseek(project_descriptor,0,SEEK_END);
  lseek(project_descriptor,0,0);

  // printf("file len of tar: %ld\n", fsize);

  // send client length of file
  char msg_len[10];
  sprintf(msg_len,"tarlen:%ld",fsize);
  send(client_fd,msg_len,10,0);

  // byte representation of the file
  char *projbuffer = (char*) malloc(sizeof(char) * fsize);
  int bytes_read = read(project_descriptor,projbuffer, fsize);
  if(bytes_read < 0){
    perror(strerror(errno));
  }
  printf("server: fsize: %ld bytes read into projbuffer: %d\n", fsize, bytes_read);
  int bytes_written = send(client_fd,projbuffer,fsize,0); 
  if (bytes_written <= 0) {
    perror(strerror(errno));
  } else {
    printf("server: sent client %d bytes\n", bytes_written);
    system("rm -rf result.tar.gz");
  }
}

int exist(char *name){
  if(access(name, F_OK ) != -1 ) {
    return 1;
  } else {
    return 0;
  }
}

void send_client_manifest(int client_fd, char* filePath){
  // get manifest ( projname/.Manifest)
  int manifest_fd = open(filePath, O_RDONLY);
  if(manifest_fd < 0){
    perror("couldnt open tar file");
  }
// get length of file off_t fsize;
  off_t fsize = lseek(manifest_fd,0,SEEK_END);
  lseek(manifest_fd,0,0);

  // printf("file len of tar: %ld\n", fsize);

  // send client length of file
  char msg_len[10];
  sprintf(msg_len,"tarlen:%ld",fsize);
  send(client_fd,msg_len,10,0);

  // byte representation of the file
  char *projbuffer = (char*) malloc(sizeof(char) * fsize);
  int bytes_read = read(manifest_fd,projbuffer, fsize);

  if(bytes_read < 0){
    perror(strerror(errno));
  }

  printf("server: %ld bytes read into manifest, now sending it to client: %d\n", fsize, bytes_read);
  int bytes_written = send(client_fd,projbuffer,fsize,0); 
  if (bytes_written <= 0) {
    perror(strerror(errno));
  } else {
    printf("server: sent client manifest in %d bytes\n", bytes_written);
  }
}

void* handle_client_connection(void* client_fd)  // client file descriptor
{

  int client_descriptor = *(int*) client_fd;

  // keep handling the client in this thread
  int BUFFER_LEN = 100;
  int num_bytes;
  // handle incoming client requests
  while(1){
    // buffer for data sent to us from client
    char buffer[BUFFER_LEN];
    int num_bytes;
    size_t len;

    //recieve commands from client from here
    if((num_bytes = recv(client_descriptor, buffer, BUFFER_LEN, 0) != -1)){
      // the client will send something like checkout:projectname so we will seperate it.
      // copy the data recieved from client into a new array to tokenize 
      char *buffercpy= malloc(sizeof(char) * BUFFER_LEN);
      strcpy(buffercpy,buffer);
      char *tokenptr;
      tokenptr = strtok(buffercpy, ":");
      printf("server: cmd from client %s with %d bytes\n",tokenptr, num_bytes);
      while(tokenptr != NULL){
	if(strcmp(tokenptr,"checkout") == 0){
	  // clients to checkout. lets do it
	  tokenptr = strtok(NULL, ":");
	  if(!exist(tokenptr)){
	     printf("%s not found on server\n",tokenptr);
	     char dummybuffer[1];
	     send(client_descriptor,dummybuffer,1,0);
	     // return NULL;
	  }
	  printf("server: sending client the project -> %s \n", tokenptr);
	  send_client_project(client_descriptor, tokenptr);
	  free(buffercpy);
	} else if(strcmp(tokenptr,"update") == 0){
	  // check if project name exists and .Manifest exists
	  tokenptr = strtok(NULL, ":");
	  char manifestPath[strlen(tokenptr) + 1 + strlen(".Manifest")];
	  sprintf(manifestPath,"%s/%s",tokenptr,".Manifest");
	  if(!exist(tokenptr) || !exist(manifestPath)){
	    printf("could not update %s. Are you sure the project and its .Manifest exists?  \n",tokenptr);
	    char dummybuffer[1];
	    send(client_descriptor,dummybuffer,1,0);
	  } else { 
	  printf("server: sending client the project manifest\n");
	  send_client_manifest(client_descriptor, manifestPath);
	  free(buffercpy);
          }
	} else if(strcmp(tokenptr,"create") == 0){
	  // check if project name exists 
	  tokenptr = strtok(NULL, ":");
	  printf("tokenptr: %s\n",tokenptr);
	  if(exist(tokenptr)){
	    printf("server: project already exists!\n");
	    char dummybuffer[1];
	    send(client_descriptor,dummybuffer,1,0);
          } else {
             // create the project
             char *mkdir = "mkdir";
             char mkprojcmd[strlen(mkdir) + strlen(tokenptr)]; 
             sprintf(mkprojcmd,"%s %s",mkdir,tokenptr);
             printf("server: creating... %s\n",mkprojcmd);
             system(mkprojcmd);
             // create a manifest file
             char makemanifest[strlen(tokenptr) + 1 + strlen(tokenptr)]; 
             sprintf(makemanifest,"%s/.Manifest",tokenptr);
             int manifest_fd = open(makemanifest, O_CREAT | O_RDWR ,0666);
             write(manifest_fd,"1\n",2);
             // send client the project
             send_client_project(client_descriptor,tokenptr);
             free(buffercpy);
	  } 
	} else if(strcmp(tokenptr,"destroy") == 0){
	  // check if project name exists 
	  tokenptr = strtok(NULL, ":");
	  printf("tokenptr: %s\n",tokenptr);
	  if(!exist(tokenptr)){
	    printf("server: project does not exist!\n");
	    char *response = "project does not exist\n";
	    send(client_descriptor,response,strlen(response),0);
	  } else {
	    // destroy the project
	    printf("destroying %s\n",tokenptr);
	    char buffer[strlen(tokenptr) + 7];
	    sprintf(buffer, "rm -rf %s", tokenptr);
	    system(buffer);
	    char dummybuffer[1];
	    send(client_descriptor,dummybuffer,1,0);
	  }
	} else if(strcmp(tokenptr,"upgrade") == 0){
	  // check if project name exists 
	  tokenptr = strtok(NULL, ":");
	  printf("tokenptr: %s\n",tokenptr);
	  if(!exist(tokenptr)){
	    printf("server: project does not exist!\n");
	    char *response = "project does not exist\n";
	    send(client_descriptor,response,strlen(response),0);
	  } else {
	    // just send the entire project back lmao
	    printf("updating %s\n",tokenptr);
	    send_client_project(client_descriptor,tokenptr);
	  }
	} else if(strcmp(tokenptr,"manifest") == 0){
	  // return manifest for the project manifest:projectname
	  tokenptr = strtok(NULL, ":");
	  char manifestPath[strlen(tokenptr) + 1 + strlen(".Manifest")];
	  sprintf(manifestPath,"%s/%s",tokenptr,".Manifest");
	  if(!exist(tokenptr) || !exist(manifestPath)){
	    printf("could not get manifest%s. Are you sure the project and its .Manifest exists?  \n",tokenptr);
	  } else {
	    printf("manifest path %s\n", manifestPath);
	    send_client_manifest(client_descriptor,manifestPath);
	  }
	} else if (strcmp(tokenptr,"commit") == 0){
	  // commit:projectname:length
	  // client sending us a file
	  char *buffercpy = malloc(sizeof(char) * MAXDATASIZE);
	  strcpy(buffercpy,buffer);
	  char *tokenptr;
	  tokenptr = strtok(buffercpy, ":");
	  tokenptr = strtok(NULL, ":");

	  // get projectname
	  char *projectname = malloc(sizeof(char) * strlen(tokenptr));
	  memcpy(projectname,tokenptr,strlen(tokenptr));
	  printf("server: preparing commit for %s\n", projectname);
	  // hash the projectname so we can identify it when comparing commits
 	  unsigned long *project_name_hash = hash(projectname);

	  // convert to project_name_hash to a string
	  char hashbuffer[32];
	  sprintf(hashbuffer,"%ln",project_name_hash);
	  printf("conversion of hash to string %s\n", hashbuffer);
 	 
	  // create final filename for .Commit (.Commit%hash)
	  char commitfilename[strlen(hashbuffer) + 1 + strlen(".Commit")]; 
	  sprintf(commitfilename,"%s#%s",".Commit",hashbuffer);
	  printf("file commit file name: %s\n",commitfilename);

	  // tokentpr is length portion of cmd  ( cmd = commit:projectname:length)
	  tokenptr = strtok(NULL, ":");
	  printf("server: commit file length in bytes is %s \n", tokenptr);

          // recieve the commit file
	  off_t file_size = atoi(tokenptr);
	  char *file_buffer = (char*) malloc(sizeof(char) * file_size);
	  int bytes_recv = read(client_descriptor,file_buffer, file_size);
	  printf("server: bytes recieved from client %d\n", bytes_recv);

	  char pathToCommit[strlen(projectname) + 1 + strlen(commitfilename)];
	  sprintf(pathToCommit,"%s/%s",projectname,commitfilename);
	  printf("path to commit: %s\n", pathToCommit);

          // create it and write to it 
	  int commit_fd = open(pathToCommit, O_CREAT | O_RDWR ,0666);
	  int writtenbytes = write(commit_fd,file_buffer,file_size);
	  if(writtenbytes <= 0){
	    printf("error in writing commit: %d bytes \n",writtenbytes);
	  }
	}
      } 
    }
  }
  return NULL;
}

int main(int argc, char ** argv)
{
  if(argc <= 1){
	printf("incorrect usage: ./WTFServer PORT\n");
	exit(1);
  }
  char *port = argv[1];

  int sockfd; // server will listen on sock_fd
  int new_fd;  // each new client connection has its own file descriptor
  struct addrinfo hints; // getaddrinfo will fill this struct out
  struct addrinfo *servinfo; // information about the server
  struct addrinfo *p; // ptr to traverse linkedlist (see struct addrinfo format )

  struct sockaddr_storage their_addr; // connector's address information
  socklen_t sin_size;

  // signal handler
  struct sigaction sa;

  int yes = 1;

  char s[INET6_ADDRSTRLEN];

  int rv;

  // clear out the hints struct so we can put stuff in it
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; // use IPv4 or IPv6, unspecificied
  hints.ai_socktype = SOCK_STREAM; // SOCK_STREAM is TCP, we will use it for this project
  hints.ai_flags = AI_PASSIVE; // AI_PASSIVE means that we will use localhost to bind

  if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }

  // loop through all the results and bind to the first we can
  for(p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype,
	    p->ai_protocol)) == -1) {
      perror("server: socket");
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
	  sizeof(int)) == -1) {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  freeaddrinfo(servinfo); // all done with this structure

  if (p == NULL)  {
    fprintf(stderr, "server: failed to bind\n");
    exit(1);
  }

  if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(1);
  }

  sa.sa_handler = sigchld_handler; // reap all dead processes
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGCHLD, &sa, NULL) == -1) {
    perror("sigaction");
    exit(1);
  }

  printf("server: waiting for connections on port 3491...\n");

  while(1) {  // main accept() loop
    sin_size = sizeof their_addr;
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
    if (new_fd == -1) {
      perror("accept");
      continue;
    }

    total_conns += 1;
    // inet_ntop converts the network address to a normal string for us to read
    inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
    printf("server: got connection from %s\n", s);
    printf("total connections now: %d\n", total_conns);
    pthread_t client_thread;
    pthread_create(&client_thread, NULL, handle_client_connection, (void*) &new_fd);
  }

  return 0;
}

#include "csapp.h"
#include "cache.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *header_user_agent = "Mozilla/5.0"
                                    " (X11; Linux x86_64; rv:45.0)"
                                    " Gecko/20100101 Firefox/45.0";
static const char *header_connection = "close";
static const char *header_proxy_connection = "close";

void *thread(void *vargp); 
void doit(int connfd);
int parse_uri(char *uri,char *host,char *port,char* path); 
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum,
                        char *shortmsg, char *longmsg);

cache_list_t *list = NULL;
sem_t mutex,w;
volatile long readcnt = 0;

/* 
 * The main function starts the proxy server by opening a listening
 * port for requests. It accepts the connection and creates a thread
 * for servicing client request. This then spawns the thread in
 * thread function which calls the doit function which processes
 * the request.
 */
int main(int argc, char **argv) 
{
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    list = cache_init();
    Sem_init(&mutex, 0, 1);
    Sem_init(&w, 0, 1);

    if (argc != 2){
       fprintf(stderr, "usage: %s <port>\n", argv[0]);
       exit(1);
    }
    if((listenfd = open_listenfd(argv[1])) < 0){
         return 1;
    }
    while (1){
       clientlen = sizeof(clientaddr);
       connfdp = Malloc(sizeof(int));
       *connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);
       Pthread_create(&tid,NULL,thread,connfdp);
    }   
}
/*
 * function for thread execution
 */
void *thread(void *vargp)
{
    int connfd= *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    signal(SIGPIPE, SIG_IGN);
    doit(connfd);
    if(close(connfd) < 0){
        return NULL;
    }
    return NULL;
}
/*
 * The do it function is where actual processing of the client request
 * and serving the server response to client by the proxy server
 * takesplace.The proxy parses the clint request with help of parse_uri
 * function.If the request is found in the cache it directly serves the 
 * request and exits.If its not found in cache then  it  sends the http
 * request to server.After recieveing the response it sends it back to the 
 * client and saves a copy in cache to serve any further requests.
 */
void doit(int connfd)
{
    char buf[MAXLINE], buf_server[MAXLINE], payload[MAX_OBJECT_SIZE];
    char method[MAXLINE], uri[MAXLINE], version;
    char host[MAXLINE],path[MAXLINE],port[MAXLINE];
    rio_t rio,rio_server;
    cache_block_t *block = NULL;
    int fd2;
    size_t size = 0;
    
    rio_readinitb(&rio, connfd);
    rio_readlineb(&rio, buf, MAXLINE);
    //sscanf must parse exactly 3 things for reqest line to be well informed
    if(sscanf(buf,"%s %s HTTP/1.%c", method, uri, &version) != 3
            || (version != '0' && version != '1')) {
       clienterror(connfd, buf, "400", "Bad Request",
             "Tiny recieved a malformed request");
       return;
    }
    //check that the method is GET 
    if (strcmp(method, "GET") != 0){
       clienterror(connfd, method, "501", "not implemented", 
         "proxy doesnot implement this method");
       return;
    }
   
    if(!(parse_uri(uri,host,port,path))){
       clienterror(connfd,uri,"404","NOT Found","could not parse request");
       return;
    }

    //starting cache operation to service request - read-write problem
    P(&mutex);
    readcnt++;
    if (readcnt == 1){
       P(&w);
    }
    V(&mutex);

    if ((block = find(list,host,port,path))!= NULL){
       rio_writen(connfd,block->payload,block->size);      	
       //cache hit
    }

    P(&mutex);
    readcnt--;
    if(readcnt == 0){
       V(&w);
    }
    V(&mutex);

    //if there is cache hit implement LRU policy and exit
    if(block != NULL){
      P(&w);
      remove_from_list(list,block);
      update_list(list,block);
      V(&w);
      return;
    }
    //ending cache read    
    if ((fd2 = open_clientfd(host,port)) <0){
       return;
    }
    rio_readinitb(&rio_server,fd2);
    //send the request and headers to web server
    sprintf(buf_server,"GET %s HTTP/1.0\r\n",path);
    rio_writen(fd2,buf_server,strlen(buf_server));
   
    int  host_found = 0;
    while(rio_readlineb(&rio,buf,MAXLINE) > 2){
        if (strstr(buf,"Host")){
            host_found = 1;
            rio_writen(fd2,buf,strlen(buf));
        }     
        else if(strstr(buf,"User-Agent")){
             sprintf(buf_server,"User-Agent: %s\r\n",header_user_agent);
             rio_writen(fd2,buf_server,strlen(buf_server));
        }
        else if(strstr(buf,"Connection")){
             sprintf(buf_server,"Connection: %s\r\n",header_connection);
             rio_writen(fd2,buf_server,strlen(buf_server));
        }
       else{
             rio_writen(fd2,buf,strlen(buf));
        }
    }
    sprintf(buf_server,"Proxy-Connection: %s\r\n",header_proxy_connection);
    rio_writen(fd2,buf_server,strlen(buf_server));
      
    if (host_found == 0){
        sprintf(buf_server,"Host: %s\r\n",host);
        rio_writen(fd2,buf_server,strlen(buf_server));
    }   
    rio_writen(fd2,"\r\n",2);
    //end of sending request

    //start reading the response    
    ssize_t len;
    sprintf(payload,"%s","");
    ssize_t payload_size=0;    
    while((len = rio_readnb(&rio_server,buf_server,MAXLINE))!= 0){
        
          rio_writen(connfd,buf_server,len);
          size = size + len;
          if (size <= MAX_OBJECT_SIZE){
             memcpy(payload+payload_size,buf_server,len);
             payload_size = size;  
          }
    }
    //starting cache write
    if (size <= MAX_OBJECT_SIZE){
      P(&w);
      block = create(host,port,path,payload,size);
      while((list->cache_size + size) > MAX_CACHE_SIZE){
         evict(list);
      }
      update_list(list,block);
      V(&w);
    } 
    //end cache write
    if(close(fd2) < 0){
       return;
    }
}

/*
 * The parse_uri is the helper function which is used to
 * parse the uri to extract host,port,path.It returns 1
 * on success and returns 0 on any errors
 */
int parse_uri(char *uri,char *host,char *port,char* path)
{
    char *ptr;
    ptr = uri+7;
    
    if (strncmp(uri,"http://",7) != 0){
       return 0;
    }
    //extracting the host
    while((*ptr != '/')&&(*ptr != ':')){
        *host = *ptr;
         host++;
         ptr++;
    }
    *host='\0';

    //extracting port
    if(*ptr == ':'){
       ptr++;
       while(*ptr != '/'){
           *port = *ptr;
            port++;
            ptr++;
       }
       *port='\0';
    }
    else{  //assign default port value of 80
       sprintf(port,"%d",80);
    }

    //extract the path
    strcpy(path,ptr);
    return 1;
    
}

/*
 * This helper function is used to send client error to the client
 * in user friendly http response body.
 */
void clienterror(int fd, char *cause, char *errnum,
                        char *shortmsg, char *longmsg)
{
   char buf[MAXLINE],body[MAXBUF];
    /*Build the HTTP response body */
   sprintf(body, "<html><title>proxy Error</title>");
   sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
   sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
   sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
   sprintf(body, "%s<hr><em>The Proxy Web server</em>\r\n",body);
 
   sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
   rio_writen(fd, buf, strlen(buf));
   sprintf(buf,"Content-type: text/html\r\n");
   rio_writen(fd, buf, strlen(buf));
   sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
   rio_writen(fd, buf, strlen(buf));
   rio_writen(fd,body, strlen(body));
}
  
      
      
    
   
  


        

#define _POSIX_C_SOURCE 200809L
#include "sx_bus_core.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_CLIENTS 16
static volatile sig_atomic_t g_run = 1;
static int clients[MAX_CLIENTS];
static sx_bus_ctx* g_ctx = NULL;
static int g_backend_rmx = 0;

static void on_sig(int s){(void)s; g_run=0;}
static int write_full(int fd,const unsigned char* p,size_t n){ size_t o=0; while(o<n){ ssize_t w=write(fd,p+o,n-o); if(w>0){o+=(size_t)w;continue;} if(w<0&&(errno==EINTR))continue; if(w<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){usleep(2000);continue;} return -1;} return 0; }
static void clients_init(){for(int i=0;i<MAX_CLIENTS;i++) clients[i]=-1;}
static void clients_add(int fd){for(int i=0;i<MAX_CLIENTS;i++) if(clients[i]<0){clients[i]=fd; return;} close(fd);} 
static void clients_broadcast(const char* s){size_t n=strlen(s); for(int i=0;i<MAX_CLIENTS;i++) if(clients[i]>=0 && write(clients[i],s,n)<0){close(clients[i]); clients[i]=-1;}}
static int make_server(const char* sock){ int s=socket(AF_UNIX,SOCK_STREAM,0); if(s<0) return -1; struct sockaddr_un a={0}; a.sun_family=AF_UNIX; strncpy(a.sun_path,sock,sizeof(a.sun_path)-1); unlink(sock); if(bind(s,(struct sockaddr*)&a,sizeof(a))<0) return -1; if(listen(s,8)<0) return -1; int fl=fcntl(s,F_GETFL,0); fcntl(s,F_SETFL,fl|O_NONBLOCK); return s; }

static int sx_write_direct(int bus,int adr,int val){ (void)bus; unsigned char p[2]={(unsigned char)(0x80|(adr&0x7F)), (unsigned char)(val&0xFF)}; return write_full(g_ctx->fd,p,2); }
static int sx_read_direct(int adr,int* out){ unsigned char a=(unsigned char)(adr&0x7F); if(write_full(g_ctx->fd,&a,1)!=0) return -1; usleep(90000); unsigned char d=0; ssize_t n=read(g_ctx->fd,&d,1); if(n==1){*out=d; return 0;} return -1; }

static unsigned char xchk(const unsigned char* p,int n){ unsigned char x=0; for(int i=0;i<n;i++) x^=p[i]; return x; }
static int rmx_send_body(const unsigned char* body,int blen){ unsigned char f[32]; f[0]=0x7D; f[1]=(unsigned char)(blen+3); memcpy(f+2,body,(size_t)blen); f[2+blen]=xchk(f,2+blen); return write_full(g_ctx->fd,f,(size_t)(blen+3)); }
static int rmx_readsx_direct(int bus,int adr,int* out){ unsigned char b[3]={0x06,(unsigned char)(bus&1),(unsigned char)(adr&0x7F)}; if(rmx_send_body(b,3)!=0) return -1; usleep(120000); unsigned char rx[64]; ssize_t n=read(g_ctx->fd,rx,sizeof(rx)); if(n>=7 && rx[0]==0x7D && rx[2]==0x06 && rx[3]==(unsigned char)(bus&1) && rx[4]==(unsigned char)(adr&0x7F)){ *out=rx[5]&0xFF; return 0;} return -1; }
static int rmx_writesx_direct(int bus,int adr,int val){ unsigned char b[4]={0x05,(unsigned char)(bus&1),(unsigned char)(adr&0x7F),(unsigned char)(val&0xFF)}; if(rmx_send_body(b,4)!=0) return -1; usleep(100000); unsigned char rx[32]; ssize_t n=read(g_ctx->fd,rx,sizeof(rx)); return (n>=5 && rx[0]==0x7D && rx[2]==0x00) ? 0 : -1; }

static int do_readadr(int bus,int adr,int* out){ return g_backend_rmx ? rmx_readsx_direct(bus,adr,out) : (bus==0?sx_read_direct(adr,out):-1); }
static int do_write(int bus,int adr,int val){ return g_backend_rmx ? rmx_writesx_direct(bus,adr,val) : sx_write_direct(bus,adr,val); }
static int do_track(int* out){ int d=-1; int rc = g_backend_rmx ? rmx_readsx_direct(0,127,&d) : sx_read_direct(127,&d); if(rc==0){ *out=(d>>7)&1; return 0;} return -1; }

static void handle_client_cmd(int idx){
 char buf[512]; ssize_t n=recv(clients[idx],buf,sizeof(buf)-1,MSG_DONTWAIT);
 if(n==0){close(clients[idx]); clients[idx]=-1; return;} if(n<0){if(errno!=EAGAIN&&errno!=EWOULDBLOCK){close(clients[idx]); clients[idx]=-1;} return;} buf[n]=0;
 char *sp=NULL,*ln=strtok_r(buf,"\r\n",&sp);
 while(ln){ int bus=0,adr=0,val=0;
  if(sscanf(ln,"READADR %d %d",&bus,&adr)==2){ int d=-1; if(do_readadr(bus,adr,&d)==0){ char f[64]; snprintf(f,sizeof(f),"FRAME %d %d %d\n",bus,adr,d&0xFF); write(clients[idx],f,strlen(f)); write(clients[idx],"OK\n",3);} else write(clients[idx],"ERR readadr\n",12);
  } else if(sscanf(ln,"WRITE %d %d %d",&bus,&adr,&val)==3){ if(do_write(bus,adr,val)==0){ char f[64]; snprintf(f,sizeof(f),"FRAME %d %d %d\n",bus,adr,val&0xFF); clients_broadcast(f); write(clients[idx],"OK\n",3);} else write(clients[idx],"ERR write\n",10);
  } else if(strncmp(ln,"GET_TRACK",9)==0){ int tr=-1; if(do_track(&tr)==0){ char t[32]; snprintf(t,sizeof(t),"TRACK %d\n",tr); write(clients[idx],t,strlen(t)); } else write(clients[idx],"TRACK -1\n",9);
  } else if(strncmp(ln,"SNAPSHOT",8)==0){ write(clients[idx],"SNAPSHOT_DONE\n",14);
  } else write(clients[idx],"ERR badcmd\n",11);
  ln=strtok_r(NULL,"\r\n",&sp);
 }
}

int main(int argc,char** argv){
 const char* serial = argc>1?argv[1]:"/dev/ttyUSB0";
 int baud = argc>2?atoi(argv[2]):19200;
 const char* sock = argc>3?argv[3]:"/tmp/sxbusd.sock";
 const char* backend = argc>4?argv[4]:"sx";
 g_backend_rmx = strcmp(backend,"rmx")==0;
 signal(SIGINT,on_sig); signal(SIGTERM,on_sig); signal(SIGPIPE,SIG_IGN);
 sx_bus_ctx ctx; if(sx_open(&ctx,serial,baud)!=0){perror("sx_open"); return 1;} g_ctx=&ctx;
 if(!g_backend_rmx){ if(sx_set_profile(&ctx,SX_PROFILE_SLX825_SX0_STREAM)!=0){fprintf(stderr,"profile failed\n"); return 1;} (void)sx_enable_feedback(&ctx); }
 int srv = make_server(sock); if(srv<0){perror("socket"); return 1;} clients_init();
 printf("sx_bus_daemon_dual backend=%s serial=%s baud=%d sock=%s\n", g_backend_rmx?"rmx":"sx", serial, baud, sock);
 while(g_run){ int cfd=accept(srv,NULL,NULL); if(cfd>=0) clients_add(cfd); for(int i=0;i<MAX_CLIENTS;i++) if(clients[i]>=0) handle_client_cmd(i); usleep(5000);} 
 close(srv); unlink(sock); sx_close(&ctx); return 0;
}

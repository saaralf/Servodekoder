#define _DEFAULT_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t baud_to_flag(int baud){switch(baud){case 9600:return B9600;case 19200:return B19200;case 38400:return B38400;case 57600:return B57600;case 115200:return B115200;default:return B19200;}}
static int set_line(int fd,int baud,int stop2){struct termios t; if(tcgetattr(fd,&t)) return -1; cfmakeraw(&t); cfsetispeed(&t,baud_to_flag(baud)); cfsetospeed(&t,baud_to_flag(baud)); t.c_cflag|=(CLOCAL|CREAD); t.c_cflag&=~PARENB; t.c_cflag&=~CSIZE; t.c_cflag|=CS8; if(stop2)t.c_cflag|=CSTOPB; else t.c_cflag&=~CSTOPB; t.c_cc[VMIN]=0; t.c_cc[VTIME]=2; return tcsetattr(fd,TCSANOW,&t);} 
static int wr(int fd,const uint8_t*p,int n){return write(fd,p,n)==n?0:-1;}
static int rd1(int fd,uint8_t*b){int n=read(fd,b,1); if(n==1)return 1; if(n==0||errno==EAGAIN||errno==EWOULDBLOCK)return 0; return -1;}
static uint8_t xchk(const uint8_t*p,int n){uint8_t x=0;for(int i=0;i<n;i++)x^=p[i];return x;}
static void bits(uint8_t v,char o[9]){for(int i=7;i>=0;i--)o[7-i]=((v>>i)&1)?'1':'-';o[8]=0;}

static int sx_read(int fd,int adr,uint8_t*val){uint8_t c[2]={(uint8_t)(adr&0x7F),0}; if(wr(fd,c,2))return -1; usleep(80000); return rd1(fd,val)==1?0:1;}
static int sx_write(int fd,int adr,uint8_t val){uint8_t c[2]={(uint8_t)(0x80|(adr&0x7F)),val}; return wr(fd,c,2);} 

static int rmx_tx(int fd,const uint8_t*body,int blen){uint8_t f[64]; f[0]=0x7D; f[1]=blen+3; memcpy(f+2,body,blen); f[2+blen]=xchk(f,2+blen); return wr(fd,f,blen+3);} 
static int rmx_readsx(int fd,int bus,int adr,uint8_t*val){uint8_t b[3]={0x06,(uint8_t)bus,(uint8_t)adr}; if(rmx_tx(fd,b,3))return -1; usleep(120000); uint8_t rx[64]; int n=read(fd,rx,sizeof(rx)); if(n>=7&&rx[0]==0x7D&&rx[2]==0x06&&rx[3]==bus&&rx[4]==adr){*val=rx[5];return 0;} return 1;}
static int rmx_writesx(int fd,int bus,int adr,uint8_t val){uint8_t b[4]={0x05,(uint8_t)bus,(uint8_t)adr,val}; if(rmx_tx(fd,b,4))return -1; usleep(100000); uint8_t rx[32]; int n=read(fd,rx,sizeof(rx)); if(n>=5&&rx[0]==0x7D&&rx[2]==0x00) return 0; return 1;}

int main(int argc,char**argv){
 if(argc<3){fprintf(stderr,"usage: %s <device> <rmx-sx1|slx-bus1|sx-raw>\n",argv[0]); return 2;}
 const char*dev=argv[1]; const char*mode=argv[2];
 int baud=19200,stop2=0; if(strcmp(mode,"rmx-sx1")==0){baud=57600;stop2=1;}
 int fd=open(dev,O_RDWR|O_NOCTTY|O_SYNC|O_NONBLOCK); if(fd<0){perror("open"); return 1;} if(set_line(fd,baud,stop2)){perror("termios"); return 1;}
 printf("MODE=%s DEV=%s BAUD=%d STOP=%d\n",mode,dev,baud,stop2?2:1);
 uint8_t v=0; char b[9];
 if(strcmp(mode,"rmx-sx1")==0){
   for(int i=0;i<10;i++){int r=rmx_readsx(fd,1,126,&v); if(r==0){bits(v,b); printf("R%02d BEFORE adr126=%3u 0x%02X %s\n",i+1,v,v,b);} else printf("R%02d BEFORE adr126=ERR\n",i+1);} 
   for(int i=0;i<10;i++){int w=rmx_writesx(fd,1,126,0x80); int r=rmx_readsx(fd,1,126,&v); if(r==0){bits(v,b); printf("R%02d AFTER  write=%s adr126=%3u 0x%02X %s\n",i+1,w==0?"OK":"ERR",v,v,b);} else printf("R%02d AFTER  write=%s adr126=ERR\n",i+1,w==0?"OK":"ERR");}
 }
 else if(strcmp(mode,"slx-bus1")==0){
   uint8_t fea0[2]={0xFE,0xA0}; wr(fd,fea0,2); usleep(150000);
   uint8_t setb[2]={0xFE,0x01}; wr(fd,setb,2); usleep(120000); // active bus 1
   for(int i=0;i<10;i++){int r=sx_read(fd,126,&v); if(r==0){bits(v,b); printf("R%02d BEFORE adr126=%3u 0x%02X %s\n",i+1,v,v,b);} else printf("R%02d BEFORE adr126=TIMEOUT\n",i+1);} 
   for(int i=0;i<10;i++){int w=sx_write(fd,126,0x80); usleep(80000); int r=sx_read(fd,126,&v); if(r==0){bits(v,b); printf("R%02d AFTER  write=%s adr126=%3u 0x%02X %s\n",i+1,w==0?"OK":"ERR",v,v,b);} else printf("R%02d AFTER  write=%s adr126=TIMEOUT\n",i+1,w==0?"OK":"ERR");}
 }
 else if(strcmp(mode,"sx-raw")==0){
   uint8_t fea0[2]={0xFE,0xA0}; wr(fd,fea0,2); usleep(150000);
   for(int i=0;i<10;i++){int r=sx_read(fd,126,&v); if(r==0){bits(v,b); printf("R%02d BEFORE adr126=%3u 0x%02X %s\n",i+1,v,v,b);} else printf("R%02d BEFORE adr126=TIMEOUT\n",i+1);} 
   for(int i=0;i<10;i++){int w=sx_write(fd,126,0x80); usleep(80000); int r=sx_read(fd,126,&v); if(r==0){bits(v,b); printf("R%02d AFTER  write=%s adr126=%3u 0x%02X %s\n",i+1,w==0?"OK":"ERR",v,v,b);} else printf("R%02d AFTER  write=%s adr126=TIMEOUT\n",i+1,w==0?"OK":"ERR");}
 } else {fprintf(stderr,"unknown mode\n"); return 2;}
 close(fd); return 0;
}

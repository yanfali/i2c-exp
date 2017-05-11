// Code originally from [afonso martone](http://www.alfonsomartone.itb.it/mzscbb.html)
//
//      nunchuck.c
//

#ifndef PORT
//#define PORT "/dev/i2c-1"    // reserved for capes and requiring pullup resistors
#define PORT "/dev/i2c-2"      // p9:pin19=SCL, p9:pin20:SDA
#endif

#ifndef ADDR
#define ADDR  0x52            // wii nunchuck address: 0x52
#endif

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stropts.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <string.h>

#define error(x...) { fprintf(stderr, "\nE%d: ", __LINE__); fprintf(stderr, x); fprintf(stderr, "\n\n"); exit(1); }


int main()
{
  // initialization: open port, ioctl address, send 0x40/0x00 init to nunchuck:
  int fd = open(PORT, O_RDWR), i=0, n=0, readcnt=1;
  if(fd<0) error("cant open %s - %m", PORT);
  if(ioctl(fd, I2C_SLAVE, ADDR) < 0) error("cant ioctl %s:0x%02x - %m", PORT, ADDR);
 
  if(write(fd, "\xF0\x55", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  usleep(10000);
  if(write(fd, "\xFB\x00", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  usleep(10000);
  
  /*
  if(write(fd, "\xF0\xAA", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  if(write(fd, "\x40\x00\x00\x00\x00\x00\x00", 6)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  if(write(fd, "\x40\x00\x00\x00\x00\x00\x00", 6)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  if(write(fd, "\x40\x00\x00\x00\x00\x00\x00", 4)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  */

  // request ID
  if(write(fd, "\xFA\x00", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  usleep(10000);
  // loop: read 6 bytes, parse, print
  unsigned char buf[6];
  memset(&buf, 0, 6);
  for(i = 0; i < 6; i++) {
    n = read(fd, buf+i, 1);
    if(n<0) error("read error %s:0x%02x - %m", PORT, ADDR);
  }
  printf("Ident %02x:%02x:%02x:%02x:%02x:%02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
  if(n<0) error("read error %s:0x%02x - %m", PORT, ADDR);
  sleep(2);
  i = 0;
  memset(&buf, 0, 6);

  for(;;)
  {
    n = read(fd, buf+i, 1); // read one byte (at index i)
    if(n<0) error("read error %s:0x%02x - %m", PORT, ADDR);
    if(!n) continue;
    //buf[i] = (buf[i]^0x17)+0x17;  // decode incoming byte
    if(++i<6) continue;     // continue to read until a packet is complete
    i=0;                    // reset the index

    // 6-byte packet complete
    printf("packet %06d: ", readcnt++);
    for (n = 0; n < 6; n++) {
      printf("%d %02x ", n, buf[n]);
      printf("%03d ", buf[n]);
    }
    printf("\n");
    char v = (~buf[5])&0b00000001;
    printf("%c ", v ? 'U' : '.');
    v = (~buf[5])&0b00000010;
    printf("%c ", v ? 'L' : '.');
    

    write(fd, "\x00", 1);       // send a zero to extension peripheral to start next cycle
    printf("\n");
    i=0;
    memset(&buf, 0, 6);
    usleep(100000);
  } 
}

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
#define ADDR_LED  0x70            // LED backpack: 0x70
#endif

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stropts.h>
#include <linux/i2c-dev.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define error(x...) { fprintf(stderr, "\nE%d: ", __LINE__); fprintf(stderr, x); fprintf(stderr, "\n\n"); exit(1); }

#define BUTTON_LEFT   0b00000010
#define BUTTON_UP     0b00000001
#define BUTTON_DOWN   0b01000000
#define BUTTON_RIGHT  0b10000000
#define BUTTON_A      0b00010000
#define BUTTON_B      0b01000000
#define BUTTON_PLUS   0b00000100
#define BUTTON_START  BUTTON_PLUS
#define BUTTON_MINUS  0b00010000
#define BUTTON_SELECT BUTTON_MINUS

// From [Adafruit_LED_Backpack](https://github.com/adafruit/Adafruit_LED_Backpack)
#define HT16K33_CMD_BRIGHTNESS  0xE0
#define HT16K33_CMD_BLINK       0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BLINK_OFF       0

uint8_t button_left(char c) {
    return (~c)&BUTTON_LEFT;
}

uint8_t button_up(char c) {
    return (~c)&BUTTON_UP;
}

uint8_t button_down(char c) {
    return (~c)&BUTTON_DOWN;
}

uint8_t button_right(char c) {
    return (~c)&BUTTON_RIGHT;
}

uint8_t button_a(char c) {
    return (~c)&BUTTON_A;
}

uint8_t button_b(char c) {
    return (~c)&BUTTON_B;
}

uint8_t button_start(char c) {
    return (~c)&BUTTON_START;
}

uint8_t button_select(char c) {
    return (~c)&BUTTON_SELECT;
}

void ack_packet(int fd) {
  write(fd, "\x00", 1);       // send a zero to extension peripheral to start next cycle
}

void set_brightness(int fd, uint8_t b) {
  if (b > 15) b = 15; // 16 levels of brightness
  uint8_t brightness = HT16K33_CMD_BRIGHTNESS | b;
  if(write(fd, &brightness, 1)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR_LED);
}

void set_blinkRate(int fd, uint8_t rate) {
  if (rate > 3) rate = 0; // turn off if not sure
  uint8_t blinkRate = HT16K33_CMD_BLINK | HT16K33_BLINK_DISPLAYON | (rate << 1);
  if(write(fd, &blinkRate, 1)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR_LED);
}

void clear_display(int fd) {
  uint8_t displayBuffer[17];
  memset(&displayBuffer, 0, sizeof(displayBuffer));
  if(write(fd, displayBuffer, sizeof(displayBuffer))<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
}

int init_i2c() {
  // initialization: open port, ioctl address, send 0x40/0x00 init to nunchuck:
  int fd = open(PORT, O_RDWR);
  if(fd<0) error("cant open %s - %m", PORT);
  if(ioctl(fd, I2C_SLAVE, ADDR) < 0) error("cant ioctl %s:0x%02x - %m", PORT, ADDR);
  return fd;
}

int init_i2c_led_backpack(int addr) {
  // initialization: open port, ioctl address, send 0x40/0x00 init to nunchuck:
  int fd = open(PORT, O_RDWR);
  if(fd<0) error("cant open %s - %m", PORT);
  if(ioctl(fd, I2C_SLAVE, addr) < 0) error("cant ioctl %s:0x%02x - %m", PORT, addr);
  // turn on oscillator
  if(write(fd, "\x21", 1)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  set_blinkRate(fd, HT16K33_BLINK_OFF);
  set_brightness(fd, 15);
  sleep(2);
  clear_display(fd);
  return fd;
}

// disable encryption - may not be necessary for third party peripherals
void disable_encryption(int fd) {
  if(write(fd, "\xF0\x55", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  if(write(fd, "\xFB\x00", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
}

void request_device_id(int fd) {
  // request ID
  if(write(fd, "\xFA\x00", 2)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
  // loop: read 6 bytes, parse, print
  unsigned char buf[6];
  int n = 0;
  for(int i = 0; i < 6; i++) {
    n = read(fd, buf+i, 1);
    if(n<0) error("read error %s:0x%02x - %m", PORT, ADDR);
  }
  printf("Ident %02x:%02x:%02x:%02x:%02x:%02x\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
  if(n<0) error("read error %s:0x%02x - %m", PORT, ADDR);
  ack_packet(fd);
  sleep(2);
}

int main()
{
  int i=0, n=0, readcnt=1;
  unsigned char buf[6];
  memset(buf, 0, sizeof(buf));

  int fd = init_i2c(); 
  disable_encryption(fd);
  request_device_id(fd);

  int bfd = init_i2c_led_backpack(0x70);

  uint8_t displayBuffer[17];
  memset(&displayBuffer, 0, sizeof(displayBuffer));


  for(;;)
  {
    n = read(fd, buf+i, 1); // read one byte (at index i)
    if(n<0) error("read error %s:0x%02x - %m", PORT, ADDR);
    if(!n) continue;

    //
    // If we were using "encryption" we would have to apply this transform.
    // This assumes a key has been loaded with 16 bytes of 0x00 as the key
    // buf[i] = (buf[i]^0x17)+0x17;  // decode incoming byte
    //
    
    if(++i<6) continue;     // continue to read until a packet is complete
    i=0;                    // reset the index

    // 6-byte packet complete
    printf("packet %06d: ", readcnt++);
    for (n = 4; n < 6; n++) {
      printf("%d %02x ", n, buf[n]);
      printf("%03d ", buf[n]);
    }

    printf("%c ", button_left(buf[5]) ? 'L' : '.');
    printf("%c ", button_up(buf[5]) ? 'U' : '.');
    printf("%c ", button_down(buf[4]) ? 'D' : '.');
    printf("%c ", button_right(buf[4]) ? 'R' : '.');
    printf("%c ", button_a(buf[5]) ? 'A' : '.');
    printf("%c ", button_b(buf[5]) ? 'B' : '.');
    printf("%c ", button_start(buf[4]) ? '+' : '.');
    printf("%c ", button_select(buf[4]) ? '-' : '.');
    
    ack_packet(fd);
    printf("\n");
    usleep(100000);
  } 
}

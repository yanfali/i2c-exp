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

#define OFF    0
#define GREEN  1
#define RED    2
#define YELLOW 3

// From [Adafruit_LED_Backpack](https://github.com/adafruit/Adafruit_LED_Backpack)
#define HT16K33_CMD_BRIGHTNESS  0xE0
#define HT16K33_CMD_BLINK       0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BLINK_OFF       0

uint8_t button_left(char c[]) {
    return (~c[5])&BUTTON_LEFT;
}

uint8_t button_up(char c[]) {
    return (~c[5])&BUTTON_UP;
}

uint8_t button_down(char c[]) {
    return (~c[4])&BUTTON_DOWN;
}

uint8_t button_right(char c[]) {
    return (~c[4])&BUTTON_RIGHT;
}

uint8_t button_a(char c[]) {
    return (~c[5])&BUTTON_A;
}

uint8_t button_b(char c[]) {
    return (~c[5])&BUTTON_B;
}

uint8_t button_start(char c[]) {
    return (~c[4])&BUTTON_START;
}

uint8_t button_select(char c[]) {
    return (~c[4])&BUTTON_SELECT;
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

void write_display(int fd, uint8_t displayBuffer[]) {
  if(write(fd, displayBuffer, 17)<0) error("cant setup %s:0x%02x - %m", PORT, ADDR);
}

void merge_buffer(uint8_t dst[], uint8_t src[], int color) {
	uint8_t *start = &dst[1];
	for (int i = 0; i < 8; i++) {
		if (color == GREEN) {
			start[i*2] |= src[i];
		} else if (color == RED) {
			start[i*2+1] |= src[i];
		} else if (color == YELLOW) { 
			// YELLOW
			start[i*2] |= src[i];
			start[i*2+1] |= src[i];
		}
	}
}

void clear_display(int fd) {
  uint8_t displayBuffer[17];
  memset(&displayBuffer, 0, sizeof(displayBuffer));
  write_display(fd, displayBuffer);
}

void cycle_display(int fd) {
  uint8_t displayBuffer[17];
  displayBuffer[0] = 0x0;
  for (int i = 0, index = 0; i < 8; i++) {
    index = i*2+1; 
    displayBuffer[index] = 0x00; // green off
    displayBuffer[index+1] = 0xff; // red on
  }
  write_display(fd, displayBuffer);
  usleep(250000);

  memset(&displayBuffer[1], 0xff, sizeof(displayBuffer)); // both on - yellow
  write_display(fd, displayBuffer);
  usleep(250000);

  for (int i = 0, index = 0; i < 8; i++) {
    index = i*2+1; 
    displayBuffer[index] = 0xff;   // green on
    displayBuffer[index+1] = 0x00; // red off
  }
  write_display(fd, displayBuffer);
  usleep(250000);
}

// diagnostic function
void corner_display(int fd) {
  uint8_t displayBuffer[17];
  memset(displayBuffer, 0, sizeof(displayBuffer));
  displayBuffer[0] = 0x0;
  // in relation to the connector which is nearest to 7,3 and 7,4
  displayBuffer[1] = 0b10000000;  // logical 0,0 - green
  displayBuffer[2] = 0b00000001;  // logical 7,0 - red
  displayBuffer[15] = 0b10000001; // logical 0,7 - yellow
  displayBuffer[16] = 0b10000000; // logical 7,7  - green
  write_display(fd, displayBuffer);
}

void all_on_display(int fd) {
  uint8_t displayBuffer[17];
  displayBuffer[0] = 0x0;
  memset(&displayBuffer[1], 0xff, sizeof(displayBuffer));
  write_display(fd, displayBuffer);
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
  cycle_display(fd);
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

// the definition of up or down depends on the display orientation
// TODO make orientation configurable
uint8_t eyeball[] = {
    0b01111110,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b10000001,
    0b01111110,
};

uint8_t center_pupil[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t left_pupil[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b01100000,
    0b01100000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t left_up_pupil[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b01100000,
    0b01100000,
    0b00000000,
};

uint8_t left_down_pupil[] = {
    0b00000000,
    0b01100000,
    0b01100000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t right_pupil[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000110,
    0b00000110,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t right_up_pupil[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000110,
    0b00000110,
    0b00000000,
};

uint8_t right_down_pupil[] = {
    0b00000000,
    0b00000110,
    0b00000110,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

uint8_t up_pupil[] = {
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00011000,
    0b00011000,
    0b00000000,
};

uint8_t down_pupil[] = {
    0b00000000,
    0b00011000,
    0b00011000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
    0b00000000,
};

void update_display_buffer(int fd, uint8_t buf[], uint8_t layer1[], uint8_t layer2[], int color1, int color2) {
  merge_buffer(buf, layer1, color1);
  merge_buffer(buf, layer2, color2);
  write_display(fd, buf);
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
  corner_display(bfd);

  uint8_t displayBuffer[17];
  memset(&displayBuffer, 0, sizeof(displayBuffer));

  merge_buffer(displayBuffer, eyeball, 1);
  merge_buffer(displayBuffer, center_pupil, 2);
  write_display(bfd, displayBuffer);

  int eye_color = GREEN;
  int pupil_color = RED;
  int blink_rate = 0;
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

    printf("%c ", button_left(buf) ? 'L' : '.');
    printf("%c ", button_up(buf) ? 'U' : '.');
    printf("%c ", button_down(buf) ? 'D' : '.');
    printf("%c ", button_right(buf) ? 'R' : '.');
    printf("%c ", button_a(buf) ? 'A' : '.');
    printf("%c ", button_b(buf) ? 'B' : '.');
    printf("%c ", button_start(buf) ? '+' : '.');
    printf("%c ", button_select(buf) ? '-' : '.');

    memset(&displayBuffer, 0, sizeof(displayBuffer));

    // change eye color for button presses
    if (button_a(buf)) {
         pupil_color = GREEN;
    } else if (button_b(buf)) {
         pupil_color = YELLOW;
    } else {
         pupil_color = RED;
    } 

    if (button_start(buf)) {
         pupil_color = OFF;
    }

    if (button_select(buf)) {
	 blink_rate++;
	 blink_rate %= 4;
         set_blinkRate(bfd, blink_rate);
    }

    if (button_left(buf) && button_up(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, left_up_pupil, eye_color, pupil_color);
    } else if (button_left(buf) && button_down(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, left_down_pupil, eye_color, pupil_color);
    } else if (button_left(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, left_pupil, eye_color, pupil_color);
    } else if (button_right(buf) && button_up(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, right_up_pupil, eye_color, pupil_color);
    } else if (button_right(buf) && button_down(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, right_down_pupil, eye_color, pupil_color);
    } else if (button_right(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, right_pupil, eye_color, pupil_color);
    } else if (button_up(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, up_pupil, eye_color, pupil_color);
    } else if (button_down(buf)) {
	update_display_buffer(bfd, displayBuffer, eyeball, down_pupil, eye_color, pupil_color);
    } else {
	update_display_buffer(bfd, displayBuffer, eyeball, center_pupil, eye_color, pupil_color);
    }
    
    
    ack_packet(fd);
    printf("\n");
    usleep(100000);
  } 
}

#include <Arduino.h>
#include <SPI.h>
#include "driver_st7920.h"


volatile static char instructionSet_ST7920 = 'B'; // Current instruction set: 'B' - basic, 'E' - extended.
volatile static bool displayStatus_ST7920 = true; // true - display ON, false - display OFF. (irrelevant to backlight).
volatile static bool underlineCursorStatus_ST7920 = true; // true - show underline cursor, false - hide underline cursor.
volatile static bool blinkCursorStatus_ST7920 = true; // true - show blink cursor, false - hide blink cursor.


/* Instruction structure is same to that in datasheet:
   0b RS RW DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0.
   Bits are reordered and transferred according to transaction rule:
   (byte 1) 1 1 1 1 RW RS 0;
   (byte 2) DB7 DB6 DB5 DB4 0 0 0 0;
   (byte 3) DB3 DB2 DB1 DB0 0 0 0 0. */
static void sendInstruction_ST7920(short instruction) {
  SPI.transfer((instruction >> 8 & 0b0000000010) |
               (instruction >> 6 & 0b0000000100) | 0b11111000);
  SPI.transfer(instruction & 0b0011110000);
  SPI.transfer(instruction << 4 & 0b0011110000);
  delayMicroseconds(80);
}


/* Provide 'E' to select "extended instruction" set.
   Provide 'B' to select "basic instruction" set.
   Avoid choosing same instruction set repeatly to save time. */
static void chooseInstructionSet_ST7920(char choice) {
  if (choice == 'E' and instructionSet_ST7920 == 'B') {
    instructionSet_ST7920 = 'E';
    sendInstruction_ST7920(0b0000110100);
  } else if (choice == 'B' and instructionSet_ST7920 == 'E') {
    instructionSet_ST7920 = 'B';
    sendInstruction_ST7920(0b0000110000);
  }
}


/* mode = 'C': cursor moves after writing a character
   mode = 'D': display shifts after writing a character
   direction = 'L': cursor or display moves left
   direction = 'R': cursor or display moves right */
static void setEntryMode_ST7920(char mode, char direction) {
  chooseInstructionSet_ST7920('B');
  if (mode == 'C' and direction == 'R') sendInstruction_ST7920(0b0000000110);
  else if (mode == 'C' and direction == 'L') sendInstruction_ST7920(0b0000000100);
  else if (mode == 'D' and direction == 'R') sendInstruction_ST7920(0b0000000101);
  else if (mode == 'D' and direction == 'L') sendInstruction_ST7920(0b0000000111);
}


/* direction = 'R': move cursor right by one, address counter += 1.
   direction = 'L': move cursor left by one, address counter -= 1. */
static void moveCursor_ST7920(char direction) {
  chooseInstructionSet_ST7920('B');
  if (direction == 'R') sendInstruction_ST7920(0b0000010100);
  else if (direction == 'L') sendInstruction_ST7920(0b0000010000);
}


/* direction = 'R': Shift entire display content right by one.
   direction = 'L': Shift entire display content left by one.
   Shift of content is achieved by changing pixel mapping offset of LCD driver.
   Data in DDRAM and address counter remains unchanged. */
static void shiftDisplay_ST7920(char direction) {
  chooseInstructionSet_ST7920('B');
  if (direction == 'R') sendInstruction_ST7920(0b0000011100);
  else if (direction == 'L') sendInstruction_ST7920(0b0000011000);
}


/* Only move cursor to home without changing data in DDRAM. */
static void homeCursor_ST7920() {
  chooseInstructionSet_ST7920('B');
  sendInstruction_ST7920(0b0000000010);
}


/* Correctly set the cursor position according to cursor change pattern.
   32 full sized 16*16 characters can be presented by 128*64 display at the same time.
   The position is a number between 0 (top left corner) and 31 (bottom right corner). */
static void setCursor_ST7920(byte position) {
  if ((position >= 0 && position <= 7) || (position >= 24 && position <= 31)) {
    sendInstruction_ST7920(0b0010000000 | position);
  } else if (position >= 8 && position <= 15) {
    sendInstruction_ST7920(0b0010000000 | position + 8);
  } else if (position >= 16 && position <= 23) {
    sendInstruction_ST7920(0b0010000000 | position - 8);
  }
}


void initialize_ST7920() {
  // Activate and config SPI communication.
  SPI.begin();
  SPI.beginTransaction(SPISettings(600000, MSBFIRST, SPI_MODE3));

  // Choose basic function set.
  instructionSet_ST7920 = 'B';
  sendInstruction_ST7920(0b0000110000);
  delayMicroseconds(20);

  // set display status according to global variables defined at the beginning.
  short displayControl = 0b0000001000;
  if (displayStatus_ST7920) displayControl |=  0b0000000100;
  if (underlineCursorStatus_ST7920) displayControl |= 0b0000000010;
  if (blinkCursorStatus_ST7920) displayControl |= 0b0000000001;
  sendInstruction_ST7920(displayControl);
  delayMicroseconds(20);

  // Clear display content.
  sendInstruction_ST7920(0b0000000001);
  delay(10);

  // set entry mode: cursor moves right
  sendInstruction_ST7920(0b0000000110);
}


void clearCharacterDisplay_ST7920() {
  chooseInstructionSet_ST7920('B');
  sendInstruction_ST7920(0b0000000001);
  delay(2);
}


void setDisplayStatus_ST7920(char option, bool status) {
  short instruction = 0b0000001000;

  switch (option) {
    case 'D':
      displayStatus_ST7920 = status;
      break;
    case 'C':
      underlineCursorStatus_ST7920 = status;
      break;
    case 'B':
      blinkCursorStatus_ST7920 = status;
      break;
  }

  if (displayStatus_ST7920) instruction |=  0b0000000100;
  if (underlineCursorStatus_ST7920) instruction |= 0b0000000010;
  if (blinkCursorStatus_ST7920) instruction |= 0b0000000001;

  chooseInstructionSet_ST7920('B');
  sendInstruction_ST7920(instruction);
}


void printHalfCharacters_ST7920(char chars[], byte length, byte row, byte column) {
  byte position = row * 16 + column; // range of position for half chars is 0-63
  setCursor_ST7920(position / 2); // move cursor to specified position

  if (position % 2 == 1) sendInstruction_ST7920(0b1000100000); // add a space at the beginning when needed
  for (byte i = 0; i < length; i++) {
    if (position == 16 || position == 32 || position == 48)
      setCursor_ST7920(position / 2); // move to the next line by set cursor on appropriate position on the display module
    sendInstruction_ST7920(0b1000000000 | chars[i]);
    position += 1;
  }
  if (position % 2 == 1) sendInstruction_ST7920(0b1000100000); // add a space at the end when needed
}


void printFullCharacters_ST7920(short chars[], byte length, byte row, byte column) {
  byte position = row * 8 + column;
  setCursor_ST7920(position);

  for (byte i = 0; i < length; i++) {
    if (position == 8 || position == 16 || position == 24)
      setCursor_ST7920(position);
    sendInstruction_ST7920(0b1000000000 | chars[i] >> 8 & 0b0000000011111111);
    sendInstruction_ST7920(0b1000000000 | chars[i] & 0b0000000011111111);
    position += 1;
  }
}


void test() {
  initialize_ST7920();

  delay(500);
  short chineseChars[] = {
    0xCEAA, 0xC1CB, 0xB6A9, 0xB5A5, 0xBCB8, 0xBAF5, 0xC5E3, 0xCBAF,
    0xB5E3, 0xCDB7, 0xB9FE, 0xD1FC, 0xBECD, 0xB2EE, 0xCFC2, 0xB9F2,
    0xC6A8, 0xB4F3, 0xB5E3, 0xCAC2, 0xB2BB, 0xB8D2, 0xB5C3, 0xD7EF,
    0xD2BB, 0xC4EA, 0xB5BD, 0xCDB7, 0xB2BB, 0xC0EB, 0xB8DA, 0xCEBB};

  printFullCharacters_ST7920(chineseChars, 32, 0, 0);
  
  /*
  for (int i = 0; i < 1; i++) {
    printHalfCharacters_ST7920("Hello world!", 12, 0, 0);
    delay(1000);
    printHalfCharacters_ST7920("Hola, mundo!", 12, 1, 1);
    delay(1000);
    printHalfCharacters_ST7920("Bojo le mod!", 12, 2, 2);
    delay(1000);
    printHalfCharacters_ST7920("Kon'nichiwa!", 12, 3, 3);
    delay(1000);

    setDisplayStatus_ST7920('D', false);
    delay(200);
    setDisplayStatus_ST7920('D', true);
    delay(200);
    setDisplayStatus_ST7920('D', false);
    delay(200);
    setDisplayStatus_ST7920('D', true);
    delay(200);
    setDisplayStatus_ST7920('D', false);
    delay(200);

    clearCharacterDisplay_ST7920();
    setDisplayStatus_ST7920('D', true);
  }

  while (true) {
  printHalfCharacters_ST7920("This is a display test for 128*64 LCD display module with ST7920", 64, 0, 0);
  delay(3000);
  clearCharacterDisplay_ST7920();
  printHalfCharacters_ST7920("If you can see these texts, then the program and module are fine", 64, 0, 0);
  delay(3000);
  clearCharacterDisplay_ST7920();
  }
  */
  
}

/* DDRAM address is used to locate the character going to be shown.
   Hence it can also be understood as "cursor".
   6 bits (A5-A0) are used to express DDRAM address, so the range is 0-63 (0b000000 - 0b111111). 
   
   ST7920 is designed for 256*64 display.
   However this driver is programmed for 128*64 display with ST7920.
   
   DDRAM address and corresponding cursor position for 256*64 display:
   Line 0: [00] [01] [02] [03] [04] [05] [06] [07] [08] [09] [10] [11] [12] [13] [14] [15]
   Line 1: ...
   ......: ...
   Line 4: [48] [49] [50] [51] [52] [53] [54] [55] [56] [57] [58] [59] [60] [61] [62] [63]

   That for 128*64 display (*be careful its very weird* blame the pcb designer):
   Line 0: [00] [01] [02] [03] [04] [05] [06] [07] (first 8 chars of line 0)
   Line 1: [16] [17] [18] [19] [20] [21] [22] [23] (first 8 chars of line 1)
   Line 2: [08] [09] [10] [11] [12] [13] [14] [15] (latter 8 chars of line 0)
   Line 3: [24] [25] [26] [27] [28] [29] [30] [31] (latter 8 chars of line 1) 
   
   Address counter of DDRAM will be added / subtracted by 1 within the range 0-63,
   but 32-63 is not shown on 128*64. */

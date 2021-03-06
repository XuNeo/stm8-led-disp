/**
 * @author Neo Xu (neo.xu1990@gmail.com)
 * @license The MIT License (MIT)
 * 
 * Copyright (c) 2019 Neo Xu
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
*/
#include "commands.h"
#include "serial_frame.h"
#include "fifo.h"
#include "usart.h"
#include "ezled.h"
/**
 * Receive characters from UART and process it in sframe. The exacted frame
 * is then processed with commands.c
*/
/**
 * The frame data definition for a valid commands
 * BYTE0: addr
 * BYTE1: command
 * BYTE2: parameter count in bytes
 * BYTE[2:BYTE1+1] parameters
*/

#define CMD_INVALID         0       //reserved command
#define CMD_HELLO           0xfe    //dummy command
#define CMD_SYS             0xff    //reserved command

#define CMD_SETBLINK        1       //start which led(s) to blink
#define CMD_SETBLINK_SPEED  2       //set the blink speed
#define CMD_SETCONTRASTA    3       //set the contrast level
#define CMD_SETCONTRASTB    4       //set the contrast level
#define CMD_SETCONTRASTC    10      //Set the contrast table for highlight.
#define CMD_PRINT           5       //print string to led.
#define CMD_SETSCROLL_SPEED 6       //set scroll speed
#define CMD_SAVE_SETTING    7       //save current settings as default settings.
#define CMD_ADD_FONT        8       //add temp font.
#define CMD_SET_HLIGHT      9       //set which led is to highlight.

#define CMD_SET_ADDR        0xa0    //set new address.

void command_set_blink(uint8_t *ppara, uint8_t len);
void command_set_blink_speed(uint8_t *ppara, uint8_t len);
void command_set_scroll_speed(uint8_t *ppara, uint8_t len);
void command_print(uint8_t *ppara, uint8_t len);
void command_set_contrastA(uint8_t *ppara, uint8_t len);
void command_set_contrastB(uint8_t *ppara, uint8_t len);
void command_set_contrastC(uint8_t *ppara, uint8_t len);
void command_save_settings(uint8_t *ppara, uint8_t len);
void command_add_font(uint8_t *ppara, uint8_t len);
void command_set_hlight(uint8_t *ppara, uint8_t len);
void command_set_addr(uint8_t *ppara, uint8_t len);

cmd_table_def cmd_table[]={
  {
    .command = CMD_HELLO,
    .phandler = 0,
    .pdesc = "command hello",
  },
  {
    .command = CMD_SYS,
    .phandler = 0,
    .pdesc = "reserved command",
  },
  {
    .command = CMD_SETBLINK,
    .phandler = command_set_blink,
    .pdesc = "",
  },
  {
    .command = CMD_SETBLINK_SPEED,
    .phandler = command_set_blink_speed,
    .pdesc = "",
  },
  {
    .command = CMD_SETSCROLL_SPEED,
    .phandler = command_set_scroll_speed,
    .pdesc = "",
  },
  {
    .command = CMD_SETCONTRASTA,
    .phandler = command_set_contrastA,
    .pdesc = "",
  },
  {
    .command = CMD_SETCONTRASTB,
    .phandler = command_set_contrastB,
    .pdesc = "",
  },
  {
    .command = CMD_SETCONTRASTC,
    .phandler = command_set_contrastC,
    .pdesc = "",
  },
  {
    .command = CMD_PRINT,
    .phandler = command_print,
    .pdesc = "",
  },
  {
    .command = CMD_SAVE_SETTING,
    .phandler = command_save_settings,
    .pdesc = "",
  },
  {
    .command = CMD_ADD_FONT,
    .phandler = command_add_font,
    .pdesc = "",
  },
  {
    .command = CMD_SET_HLIGHT,
    .phandler = command_set_hlight,
    .pdesc = "",
  },
  {
    .command = CMD_SET_ADDR,
    .phandler = command_set_addr,
    .pdesc = "",
  },
  
};
static uint8_t address;
static sframe_def sframe;
static fifo_def uart_fifo;
static inline void command_parser(uint8_t *pframe, uint32_t len){
  uint8_t cmd_code, para_len, addr;
  uint8_t i;
  if(pframe == 0) return;
  if(len < 3) return; //we have addr+cmd+len at least
  addr = *pframe++;
  if(addr != address)
    if(addr != 0) //address zero is broadcast address.
      return;
  cmd_code = *pframe++;
  para_len = *pframe++;
  for(i=0; i<(sizeof(cmd_table)/sizeof(cmd_table_def)); i++){
    if(cmd_code == cmd_table[i].command){
      //found it. call it.
      if(cmd_table[i].phandler)
        cmd_table[i].phandler(pframe, para_len);
      break;
    }
  }
}

//add fifo support on usart receive side.
static void _usart_rx_callback(uint8_t ch){
  fifo_write1B(&uart_fifo, ch);
}

//the function will be called when a valid frame is decoded.
static void _sframe_callback(uint8_t* pbuff, uint32_t len){
  command_parser(pbuff, len);
}

void commands_init(uint8_t addr){
  static uint8_t frame_buff[128];
  static uint8_t fifobuff[128];
  fifo_init(&uart_fifo, fifobuff, 128);
  usart_init(115200, (void*)_usart_rx_callback);
  sframe_init(&sframe, frame_buff, 128, _sframe_callback);  //used to decode frame and store decoded frame to buffer.
  address = addr;
}

void commands_set_addr(uint8_t addr){
  address = addr;
}

uint8_t commands_get_addr(void){
  return address;
}



//poll this functio to process command received(frame received)
void commands_poll(void){
  uint8_t ch;
  while(fifo_read1B(&uart_fifo, &ch) == fifo_err_ok)
    sframe_decode(&sframe, &ch, 1);
}

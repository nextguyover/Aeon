#ifndef DISP_H
#define DISP_H

#define DISP_BLACK 0x0   /// 000
#define DISP_WHITE 0x1   /// 001
#define DISP_YELLOW 0x2  /// 010
#define DISP_RED 0x3     /// 011
#define DISP_BLUE 0x5    /// 101
#define DISP_GREEN 0x6   /// 110

void disp_send_command(uint8_t reg);
void disp_send_data(uint8_t data);
void disp_init();
void disp_init_regs(void);
void disp_turn_on();
void disp_clear(uint8_t color);
void disp_sleep(void);
void disp_exit();

#endif  // DISP_H
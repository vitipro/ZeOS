#ifndef __SCREEN_H__
#define __SCREEN_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>

#define MAX_SCREENS 30
#define NUM_ROWS 25
#define NUM_COLUMNS 80

struct screen_struct {
  int screen_ID;
  int owner_PID;
  unsigned int x, y;		// position where write() makes the printing
  Byte colordata;
  Byte padding[3];
  Word data[NUM_ROWS][NUM_COLUMNS];
};

struct current_screen {
  struct task_struct *task;
  int fd;
};

extern struct list_head tasks_with_screens;

extern struct list_head* l;

struct current_screen cS;

int tasks_with_screens_size;

int n_screens;

DWord aux_ebp1, aux_ebp2;

void update_screen(void);

struct screen_struct* next_available_screen(int fd, struct task_struct* task);

struct screen_struct* prev_available_screen(int fd, struct task_struct* task);

int get_next_screen(int fd);

int get_prev_screen(int fd);

void next_screen();

void previous_screen();

int next_available_fd(int n);

int previous_available_fd(int n);

void set_ss_screenpag(page_table_entry *PT, unsigned page,unsigned frame);

void set_pag_screens(struct task_struct *task);

int sys_write_virt_screen(char *buffer,int size, int fd);

int escape_codes(char *buffer,int size, struct screen_struct *S, int  i );

int color_codes(char *buffer,int size, struct screen_struct *S, int  i );

void mover_cursor(int deltax, int deltay, struct screen_struct *S);

void cambiar_foreground(Byte color, struct screen_struct *S);

void cambiar_background(Byte color, struct screen_struct *S);

void print_virt(char c, int fd);

void print_xy_virt(Byte mx, Byte my, char c, int fd);

#endif  /* __SCREEN_H__ */

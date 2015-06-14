/*--------------------------------------------------------------------------
     ukbd.h  -  Donald Cross, March 1988.
                Copyright (C) 1988-1996 by Donald Cross & Bill Brown
                Keyboard stuff for USCREEN libraries.

     KeyPressed() added by Bill Brown, June 1988.
----------------------------------------------------------------------------*/

#ifndef __UKBD__
#define __UKBD__

int far KeyPressed(void);


/*-----   Macros which return the status of shift-type keys . . .  -----*/

#define CAPS               64
#define NUM                32
#define SCROLL             16
#define ALT                 8
#define CTRL                4
#define LSHIFT              2
#define RSHIFT              1
#define SHIFT               3

#define  capsactive()      (bioskey(2) & CAPS)
#define  numactive()       (bioskey(2) & NUM)
#define  scrollactive()    (bioskey(2) & SCROLL)
#define  altpressed()      (bioskey(2) & ALT)
#define  ctrlpressed()     (bioskey(2) & CTRL)
#define  lshiftpressed()   (bioskey(2) & LSHIFT)
#define  rshiftpressed()   (bioskey(2) & RSHIFT)
#define  shiftpressed()    (bioskey(2) & SHIFT)

#define  keystate()        peekb(0x0000,0x0418)

#define  capspressed()     (keystate() & CAPS)
#define  numpressed()      (keystate() & NUM)
#define  scrollpressed()   (keystate() & SCROLL)

/*----------------------------------------------------------------------*/


/*  Function keys . . . */

#define   F1       (59<<8)
#define   F2       (60<<8)
#define   F3       (61<<8)
#define   F4       (62<<8)
#define   F5       (63<<8)
#define   F6       (64<<8)
#define   F7       (65<<8)
#define   F8       (66<<8)
#define   F9       (67<<8)
#define   F10      (68<<8)

/*  Shifted function keys . . . */

#define   S_F1      (84<<8)
#define   S_F2      (85<<8)
#define   S_F3      (86<<8)
#define   S_F4      (87<<8)
#define   S_F5      (88<<8)
#define   S_F6      (89<<8)
#define   S_F7      (90<<8)
#define   S_F8      (91<<8)
#define   S_F9      (92<<8)
#define   S_F10     (93<<8)

/*  Control function keys, such as <Ctrl>-<F1> . . . */

#define   C_F1      (94<<8)
#define   C_F2      (95<<8)
#define   C_F3      (96<<8)
#define   C_F4      (97<<8)
#define   C_F5      (98<<8)
#define   C_F6      (99<<8)
#define   C_F7      (100<<8)
#define   C_F8      (101<<8)
#define   C_F9      (102<<8)
#define   C_F10     (103<<8)

/*  Alt function keys, such as <Alt>-<F1> . . .  */

#define  A_F1       (104<<8)
#define  A_F2       (105<<8)
#define  A_F3       (106<<8)
#define  A_F4       (107<<8)
#define  A_F5       (108<<8)
#define  A_F6       (109<<8)
#define  A_F7       (110<<8)
#define  A_F8       (111<<8)
#define  A_F9       (112<<8)
#define  A_F10      (113<<8)

/*  Other Alt-keys . . . */

#define  A_1        (120<<8)
#define  A_2        (121<<8)
#define  A_3        (122<<8)
#define  A_4        (123<<8)
#define  A_5        (124<<8)
#define  A_6        (125<<8)
#define  A_7        (126<<8)
#define  A_8        (127<<8)
#define  A_9        (128<<8)
#define  A_0        (129<<8)

#define  A_HYPHEN    (130<<8)
#define  A_Q         (16<<8)
#define  A_W         (17<<8)
#define  A_E         (18<<8)
#define  A_R         (19<<8)
#define  A_T         (20<<8)
#define  A_Y         (21<<8)
#define  A_U         (22<<8)
#define  A_I         (23<<8)
#define  A_O         (24<<8)
#define  A_P         (25<<8)

#define  A_A         (30<<8)
#define  A_S         (31<<8)
#define  A_D         (32<<8)
#define  A_F         (33<<8)
#define  A_G         (34<<8)
#define  A_H         (35<<8)
#define  A_J         (36<<8)
#define  A_K         (37<<8)
#define  A_L         (38<<8)

#define  A_Z         (44<<8)
#define  A_X         (45<<8)
#define  A_C         (46<<8)
#define  A_V         (47<<8)
#define  A_B         (48<<8)
#define  A_N         (49<<8)
#define  A_M         (50<<8)

/*  <Shift>-<Tab> . . .  */

#define  S_TAB       (15<<8)

/*  Numeric keypad special keys . . . */

#define  HOME         (71<<8)
#define  UPAR         (72<<8)
#define  PGUP         (73<<8)
#define  LAR          (75<<8)
#define  RAR          (77<<8)
#define  END          (79<<8)
#define  DOWNAR       (80<<8)
#define  PGDN         (81<<8)
#define  INSERT       (82<<8)
#define  DELETE       (83<<8)

/*  More control keys which return extended scan codes . . . */

#define  C_LAR        (115<<8)
#define  C_RAR        (116<<8)
#define  C_END        (117<<8)
#define  C_PGDN       (118<<8)
#define  C_HOME       (119<<8)
#define  C_PGUP       (132<<8)

/*  Commonly used keys which do not generate extended scan codes . . . */

#define   BACKSPACE       8
#define   SPACE          32
#define   ENTER          13
#define   ESCAPE         27
#define   TAB             9

/*   Control-alpha keys . . .
     (No, I didn't type all these in.  I wrote a program to do it!)   */

#define     C_A        1
#define     C_B        2
#define     C_C        3
#define     C_D        4
#define     C_E        5
#define     C_F        6
#define     C_G        7
#define     C_H        8
#define     C_I        9
#define     C_J       10
#define     C_K       11
#define     C_L       12
#define     C_M       13
#define     C_N       14
#define     C_O       15
#define     C_P       16
#define     C_Q       17
#define     C_R       18
#define     C_S       19
#define     C_T       20
#define     C_U       21
#define     C_V       22
#define     C_W       23
#define     C_X       24
#define     C_Y       25
#define     C_Z       26

#endif

/*
    $Log: kbd.h,v $
    Revision 1.1  2005/11/25 19:47:27  dcross
    Recovered lots of old chess source files from the dead!
    Found these on CDROM marked "14 September 1999".
    Added cvs log tag and moved old revision history after that.

*/


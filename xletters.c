/* X Letters - Catch falling words - version 2.0.3 */
/* Copyright (C) 1998 by
 *   Peter Horvai (peter.horvai@ens.fr)
 *   David A. Madore (david.madore@ens.fr)
 * Copyright (C) 2003 by
 *   Adrian Daerr (adrian.daerr@pmmh.espci.fr) */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
 * the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* print diagnostic info at runtime, 0-3 increasing verbosity  */
#define DEBUG 0

/* ********** INCLUDE FILES ********** */

/* General include files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>

#include <sys/mman.h>
#include <sys/types.h>
//#include <architecture/byte_order.h> /* defines __BIG_ENDIAN__ etc. */

/* Xlib include files */
#include <X11/Xlib.h>

/* Xt include files */
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>

/* Xaw include files */
#include <X11/Xaw/Box.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Toggle.h>

/* Configuration file */
#include "xletters.h"

/* UTF-8 to UCS-4 conversion */
#include "utf8.h"

/* ********** TYPES AND VARIABLES ********** */

/* Game variables */
char deathmode; /* Send and receive words on standard input/output */
char lifemode; /* Make words fall periodically, as usual */
char trainingmode;
char scorecounts;
int level;
long score,vpoints,levpoints;
int lives;
char bonuslevel;
char showkana; /* draw kana next to a word */
char showcomment; /* draw translation next to a word */

/* Data structure describing a kana transcription of a falling word */
typedef struct kana_state_s {
  ucs4_t *kana; /* Syllabic transcription (kana) to be typed; malloc()ated */
  XChar2b *kanaX; /* the same kana encoded for X-Windows; malloc()ated */
  int *kana2kanji; /* For every position in the kana string, gives the
		      corresponding position in the kanji string;
		      malloc()ated */
  int kanalen; /* The length of the transcription */
  int kanatyped; /* Number of characters correctly typed */
  int kawidth,katwidth; /* Width in pixels of whole string and typed part */
  int pendtyped; /* Number of pending input-characters */
  ucs4_t *comment; /* The translation or comment; malloc()ated */
  XChar2b *commentX; /* The same comment encoded for X-Windows; malloc()ated */
  int colen; /* The length of the comment */
  int cowidth; /* Width in pixels of whole comment string */
  struct kana_state_s *next; /* Pointer to next chained structure or NULL */
} kana_state_t;

typedef struct word_state_s {
  XChar2b *kanji; /* The actual word (e.g. kanji); malloc()ated */
  int kanjilen; /* The length of that word */
  int kanjityped; /* Number of characters correctly typed */
  int kjwidth,kjtwidth; /* Width in pixels of whole string and typed part */
  int purekana; /* true if kanji == kana */
  long x; /* x position of left-most point, in pixels */
  long y; /* y position of baseline, in *millipixels* */
  long rate; /* Descent rate in millipixels per time unit */
  long kaxoffs,kayoffs; /* Offset of kana string to kanji, in pixels */
  long coxoffs,coyoffs; /* Offset of comment string to kanji, in pixels */
  kana_state_t *kanastruct; /* First element of chained list of
	 		      syllabic transcriptions (kana) */
  XChar2b *kana; /* The current syllabic transcription (kana) */
  int kanalen; /* The length of the current transcription */
  int kanatyped; /* Number of characters correctly typed */
  int kawidth,katwidth; /* Width in pixels of whole string and typed part */
  XChar2b *comment; /* The current translation or comment */
  int colen; /* The length of the current comment */
  int cowidth; /* Width in pixels of whole comment string */
} word_state_t;

/* The words that are falling */
word_state_t words[MAXWORDS];
/* The number of words that are falling */
int nb_words;

/* Romaji transcription of a word may not be longer than this,
   which means Kana length may be at most half of this value,
   and Kanji at most a sixth in the worst (?) case.
   This is also the maximum comment length. */
#define MAX_WORDSIZE 1023

/* Whether the game is paused */
char paused;

/* Times are measured in fundamental time units (normally 75ms) */
unsigned long current_time; /* Time since beginning of play */
unsigned long lastword_time; /* Time since last word started falling */
unsigned long level_time; /* Time since beginning of level */

/* The word being typed in deathmatch mode */
ucs4_t deathword[MAX_WORDSIZE+1];
/* The length of that word; the number of pending (dead) keystrokes */
int deathln,deathpend;

#define KEYBUF_MARGIN 128
/* Keys pressed since level start (at most MAX_WORDSIZE) */
ucs4_t keybuf[MAX_WORDSIZE+KEYBUF_MARGIN];
int keybufpos;

/* The word file */
ucs4_t **dictionary; /* Dictionary entries */
long dict_words; /* Number of words in dictionary file */

/* input to Unicode conversion structure */
typedef struct iconv_s {
  ucs4_t *kana;
  int klen;
  ucs4_t *romaji;
  int rlen;
} iconv_t;
iconv_t *inputconverter;
int max_romaji; /* length of longest romaji entry in inputconverter */

/* Remember to explain the k2k error message once in a while */
char explain_k2k_error=0;

/* ---------- X related variables ---------- */

/* Fallback resources */
char *fallback_resources[] = {
  "*input: True",
  "*background: Grey75",
  "*foreground: White",
  "*Box.background: DarkSlateGrey",
  "*Label.foreground: White",
  "*label.foreground: Green",
  "*Command.foreground: Yellow",
  "*Toggle.foreground: Yellow",
  "*quitButton.label: Quit",
  "*pauseButton.label: Pause",
  "*modeButton.label: Type",
  "*sendButton.label: Send",
  "*nextButton.label: Next",
  "*quitButton.accelerators:"
  "<KeyPress>Escape: set() notify()",
  "*pauseButton.accelerators:"
  "<KeyPress>Pause: toggle() notify()\\n\\\n"
  "<KeyPress>Tab: toggle() notify()",
  "*modeButton.accelerators:"
  "<KeyPress>BackSpace: toggle() notify()",
  "*sendButton.accelerators:"
  "<KeyPress>Return: set() notify() unset()",
  "*nextButton.accelerators:"
  "<KeyPress>Next: set() notify() unset()",
  NULL
};

/* Application resources */
typedef struct app_res_val_s {
  XFontStruct *word_font;
  XFontStruct *kana_font;
  XFontStruct *comment_font;
  Pixel word_color,typed_color;
  Pixel kana_color,kanaTyped_color;
  Pixel comment_color;
  int kanji_freq;
  char show_kana;
  char show_comment;
  char *death_mode;
  Boolean training_mode;
} app_res_val_t;

app_res_val_t app_res_val;

XtResource app_res_tab[] = {
  {"wordFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
   XtOffsetOf(app_res_val_t,word_font),
   XtRString, FONTNAME
  },
  {"wordColor", XtCForeground, XtRPixel, sizeof(Pixel),
   XtOffsetOf(app_res_val_t,word_color),
   XtRString, WORD_COLOR
  },
  {"typedColor", "HighlightColor", XtRPixel, sizeof(Pixel),
   XtOffsetOf(app_res_val_t,typed_color),
   XtRString, TYPED_COLOR
  },
  {"kanaFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
   XtOffsetOf(app_res_val_t,kana_font),
   XtRString, FONTNAME
  },
  {"kanaColor", XtCForeground, XtRPixel, sizeof(Pixel),
   XtOffsetOf(app_res_val_t,kana_color),
   XtRString, KANA_COLOR
  },
  {"kanaTypedColor", "HighlightColor", XtRPixel, sizeof(Pixel),
   XtOffsetOf(app_res_val_t,kanaTyped_color),
   XtRString, KANATYPED_COLOR
  },
  {"commentFont", XtCFont, XtRFontStruct, sizeof(XFontStruct *),
   XtOffsetOf(app_res_val_t,comment_font),
   XtRString, COMMENTFONTNAME
  },
  {"commentColor", XtCForeground, XtRPixel, sizeof(Pixel),
   XtOffsetOf(app_res_val_t,comment_color),
   XtRString, COMMENT_COLOR
  },
  {"kanjiFreq", "KanjiFrequency", XtRInt, sizeof(int),
   XtOffsetOf(app_res_val_t,kanji_freq),
   XtRImmediate, (XtPointer)KANJI_FREQ
  },
  {"showKana", "ShowKana", XtRUnsignedChar, sizeof(unsigned char),
   XtOffsetOf(app_res_val_t,show_kana),
   XtRImmediate, (XtPointer)SHOWKANA
  },
  {"showComment", "ShowComment", XtRUnsignedChar, sizeof(unsigned char),
   XtOffsetOf(app_res_val_t,show_comment),
   XtRImmediate, (XtPointer)SHOWCOMMENT
  },
  {"deathMode", "DeathMode", XtRString, sizeof(char *),
   XtOffsetOf(app_res_val_t,death_mode),
   XtRImmediate, "normal"
  },
  {"trainingMode", "TrainingMode", XtRBoolean, sizeof(Boolean),
   XtOffsetOf(app_res_val_t,training_mode),
   XtRImmediate, False
  },
};

/* Recognized options */
XrmOptionDescRec options[] = {
  {"-wfn", "*wordFont", XrmoptionSepArg, NULL},
  {"-wc", "*wordColor", XrmoptionSepArg, NULL},
  {"-tc", "*typedColor", XrmoptionSepArg, NULL},
  {"-kfn", "*kanaFont", XrmoptionSepArg, NULL},
  {"-kc", "*kanaColor", XrmoptionSepArg, NULL},
  {"-ktc", "*kanaTypedColor", XrmoptionSepArg, NULL},
  {"-cfn", "*commentFont", XrmoptionSepArg, NULL},
  {"-cc", "*commentColor", XrmoptionSepArg, NULL},
  {"-freq", "*kanjiFreq", XrmoptionSepArg, NULL},
  {"-showk", "*showKana", XrmoptionSepArg, NULL},
  {"-showc", "*showComment", XrmoptionSepArg, NULL},
  {"-gbg", "*gameSpace.background", XrmoptionSepArg, NULL},
  {"-death", "*deathMode", XrmoptionNoArg, (XtPointer)"death"},
  {"-duel", "*deathMode", XrmoptionNoArg, (XtPointer)"duel"},
  {"-nodeath", "*deathMode", XrmoptionNoArg, (XtPointer)"normal"},
  {"-train", "*trainingMode", XrmoptionNoArg, (XtPointer)"True"},
  {"-notrain", "*trainingMode", XrmoptionNoArg, (XtPointer)"False"},
};

/* The timer interval reference */
XtIntervalId timeout;

/* The widgets */
Widget toplevel,ground_box,label;
Widget quit_button,pause_button,mode_button,send_button,next_button;
Widget lives_label,score_label,level_label;
Widget type_space;
Widget game_space;

/* The application context */
XtAppContext app_context;

/* Various Xlib related variables */
Display *dpy;
Window win;
GC gc0; /* Used to erase background */
GC gcW,gcWT; /* To draw or erase (kanji-) word and its typed part */
GC gcK,gcKT; /* To draw or erase kana and its typed part */
GC gcC; /* To draw or erase translation */
XFontStruct *fnt_info;
Font fnt;

/* ---------- High score table ---------- */

/* High score table */
typedef struct hstable_entry_s {
  char name[1024];
  uid_t uid;
  int level;
  long score;
  unsigned long dur;
} hstable_entry_t;

hstable_entry_t hstable[SCORELEN];

uid_t euid;
gid_t egid;

/* ********** HIGH SCORE TABLE MANAGEMENT ROUTINES ********** */

void
print_hstable_entry(hstable_entry_t *ent,int n)
{
  int i,f;
  if (n==-1) printf("You: ");
  else printf("%4d ",n+1);
  f=1;
  for (i=0;i<20;i++) {
    if (f&&(ent->name[i]==0)) f=0;
    if (f) {
      if (ent->name[i]=='_') printf(" ");
      else printf("%c",ent->name[i]);
    } else printf(" ");
  }
  printf("%4d%6ld%3lu'%02lu\n",ent->level,ent->score,
         ent->dur/60000,(ent->dur/1000)%60);
}

void cleanexit(void);

void
show_hstable(void)
     /* Show high score table and quit */
{
  hstable_entry_t yours;
  FILE *hsf;
  int i,replace,place;
  struct passwd *pwent;
  char hsedit;
  if (trainingmode) exit(0);
  yours.uid = getuid();
  pwent=getpwuid(yours.uid);
  strncpy(yours.name,pwent->pw_gecos,1000);
  yours.name[1000]=0;
  for (i=0;i<1000;i++) {
    if (yours.name[i]==' ') yours.name[i]='_';
    if (yours.name[i]==',') yours.name[i]=0;
  }
  yours.level=level;
  yours.score=score;
  yours.dur=current_time*BASETIME;
  printf("     Name                Lev.Score Time\n");
  print_hstable_entry(&yours,-1);
  seteuid(euid);
  setegid(egid);
  hsedit=1;
  hsf = fopen(SCOREFILE,"r+");
  if (hsf==NULL) {
    hsf = fopen(SCOREFILE,"w+");
    if (hsf==NULL) {
      hsedit=0;
      hsf = fopen(SCOREFILE,"r");
      if (hsf==NULL) {
        perror("Can't open high score file");
        exit(1);
      }
    }
  }
  rewind(hsf);
  for (i=0;i<SCORELEN;i++) {
    unsigned long x;
    if (fscanf(hsf,"%s %lu %d %ld %lu\n",hstable[i].name,
               &x,&hstable[i].level,&hstable[i].score,
               &hstable[i].dur)==5) {
      /* Beware: the above lines contain a buffer overrun.  Don't mess
       * up with the high score tables!  In particular, if the program
       * is made suid or sgid, only people with the corresponding uid
       * or gid should be allowed to modify the table. */
      hstable[i].uid=x;
    } else {
      strcpy(hstable[i].name,"Joe_Player");
      hstable[i].uid=9999;
      hstable[i].level=0;
      hstable[i].score=0;
      hstable[i].dur=0;
    }
  }
#if MULTENT
  replace=SCORELEN-1;
#else
  for (replace=0;replace<SCORELEN-1;replace++) {
    if (hstable[replace].uid==yours.uid)
      break;
  }
#endif
  if (scorecounts&&(yours.score>hstable[replace].score)) {
    printf("You made it into the high score file!\n");
    for (place=0;place<SCORELEN;place++) {
      if (yours.score>hstable[place].score)
        break;
    }
    for (i=replace;i>place;i--) {
      hstable[i]=hstable[i-1];
    }
    hstable[place]=yours;
  }
  printf("HIGH SCORES:\n");
  for (i=0;i<SCORELEN;i++)
    print_hstable_entry(hstable+i,i);
  if (hsedit) {
    rewind(hsf);
    for (i=0;i<SCORELEN;i++) {
      fprintf(hsf,"%s %lu %d %ld %lu\n",hstable[i].name,
              (long)hstable[i].uid,hstable[i].level,hstable[i].score,
              hstable[i].dur);
    }
  }
  fclose(hsf);
  cleanexit();
}

/* ********** DEBUGGING ROUTINES ********** */

#define debugmsg(level, format, args...) \
  if (DEBUG >= level) fprintf(stderr, format, ##args)
//void debugmsg(int level, const char *fmt, ...)
//{
//  va_list argp;
//  if (DEBUG >= level) {
//    va_start(argp, fmt);
//    vfprintf(stderr, fmt, argp);
//    va_end{argp}
//  }
//}

/* prints (allmost) all about a word */
void debug_word_info_1(word_state_t *pw) {
  int i;
  kana_state_t *pk;
  fprintf(stderr,"++++ word info\n");
  fprintf(stderr,"kanji=%p, kanastruct=%p, kana=%p, comment=%p\n",
          pw->kanji, pw->kanastruct, pw->kana, pw->comment);
  fprintf(stderr,"kanjilen=%d, kanjityped=%d, kjwidth=%d, kjtwidth=%d, ",
          pw->kanjilen, pw->kanjityped, pw->kjwidth, pw->kjtwidth);
  fprintf(stderr,"kawidth=%d, katwidth=%d,\nkanalen=%d, kanatyped=%d, ",
          pw->kawidth, pw->katwidth, pw->kanalen, pw->kanatyped);
  fprintf(stderr,"purekana=%s, colen=%d, cowidth=%d\ncomment=",
          pw->purekana?"yes":"no", pw->colen, pw->cowidth);
  if (pw->comment != NULL)
    for (i=0; i<pw->colen; i++)
      fprintf(stderr,"%c",pw->comment[i].byte2);
  fprintf(stderr,",\nx=%ld, y=%ld, rate=%ld, kaxoffs=%ld, kayoffs=%ld, ",
          pw->x, pw->y, pw->rate, pw->kaxoffs, pw->kayoffs);
  fprintf(stderr,"coxoffs=%ld, coxoffs=%ld\n",
          pw->coxoffs, pw->coyoffs);
  pk = pw->kanastruct;
  while (pk != NULL) {
    fprintf(stderr,"**%p** kana(2kanji)=",pk);
    for (i=0; i<pk->kanalen; i++)
      fprintf(stderr," 0x%x(%d)",pk->kana[i],pk->kana2kanji[i]);
    fprintf(stderr,"\nkana=%p, kanaX=%p, k2k=%p, comment=%p, commentX=%p\n",
            pk->kana, pk->kanaX, pk->kana2kanji, pk->comment, pk->commentX);
    fprintf(stderr,"kanalen=%d, kanatyped=%d, kawidth=%d, katwidth=%d,\n",
            pk->kanalen, pk->kanatyped, pk->kawidth, pk->katwidth);
    fprintf(stderr,"pendtyped=%d, colen=%d, cowidth=%d, comment=",
            pk->pendtyped, pk->colen, pk->cowidth);
    if (pk->comment != NULL)
      for (i=0; i<pk->colen; i++)
        fprintf(stderr,"%c",ASCII(pk->comment[i]));
    fprintf(stderr,",\n");
    pk = pk->next;
  }
  fprintf(stderr,"----\n");
}

/* prints live info about a word */
void debug_word_info_2(word_state_t *pw) {
  kana_state_t *pk;
  fprintf(stderr,"++++ more info for word at %d(%d):\n", 
          pw-words, (pw-words)/sizeof(word_state_t));
  fprintf(stderr,"kanjityped=%d, kjtwidth=%d, kanatyped=%d, katwidth=%d\n",
          pw->kanjityped, pw->kjtwidth, pw->kanatyped, pw->katwidth);
  if (DEBUG>2) {
    pk = pw->kanastruct;
    while (pk != NULL) {
      fprintf(stderr,"** pk=%p\n", pk);
      fprintf(stderr,"** kanatyped=%d, katwidth=%d, pendtyped=%d\n",
              pk->kanatyped, pk->katwidth, pk->pendtyped);
      pk = pk->next;
    }
  }
  fprintf(stderr,"----\n");
}

/* ********** SMALL GAME ROUTINES ********** */

/* free malloced parts of a word */
void free_word(word_state_t *pw) {
  kana_state_t *pk1,*pk2;
  if (pw->kanji != NULL) free(pw->kanji);
  pk1 = pw->kanastruct;
  while (pk1 != NULL) {
    if (pk1->kana != NULL) free(pk1->kana);
    if (pk1->kanaX != NULL) free(pk1->kanaX);
    if (pk1->kana2kanji != NULL) free(pk1->kana2kanji);
    if (pk1->comment != NULL) free(pk1->comment);
    if (pk1->commentX != NULL) free(pk1->commentX);
    pk2 = pk1->next;
    free(pk1);
    pk1 = pk2;
  }
}

void
cleanexit(void)
     /* Clean up and quit */
{
  int i;
  if (dictionary[0] != NULL) free(dictionary[0]);
  if (dictionary != NULL) free(dictionary);
  if (inputconverter[0].kana != NULL) free(inputconverter[0].kana);
  if (inputconverter != NULL) free(inputconverter);
  for (i=0;i<nb_words;i++) free_word(words+i);
  exit(0);
}

/* Return the base descent rate (in millipixels per time unit) for the
   level. */
long level_speed(int lev)
{
  return 900+80*lev;
}

/* How long to wait before to drop a new word on the given level */
long level_rate(int lev)
{
  int l;
  l = ((lev<20)?lev:20);
  return 10+(50000-l*2500)/level_speed(lev);
}

/* Score for a word */
long word_score(word_state_t *pw)
{
  return (pw->y<HEIGHT*200)?8+pw->kanjilen:5+pw->kanjilen;
}

void
show_lives(void)
     /* Update the lives label. */
{
  char s[256];
  sprintf(s,"Lives: %d",lives);
  XtVaSetValues(lives_label,
                XtNlabel,s,
                NULL);
}

void
show_score(void)
     /* Update the score label. */
{
  char s[256];
  if (vpoints>0)
    sprintf(s,"Score: %ld (+%ld)",score,vpoints);
  else
    sprintf(s,"Score: %ld",score);
  XtVaSetValues(score_label,
                XtNlabel,s,
                NULL);
}

void
show_level(void)
     /* Update the level label. */
{
  char s[256];
  sprintf(s,"Level: %d",level);
  XtVaSetValues(level_label,
                XtNlabel,s,
                NULL);
}

void
add_score(long n)
     /* Increase player's score. */
{
  long newscore;
  long k;
  k=n;
  if (vpoints<k) k=vpoints;
  newscore=score+n+k;
  vpoints-=k;
  levpoints+=n;
  if ((newscore/2500)>(score/2500)) {
    lives++;
    show_lives();
  }
  score=newscore;
  show_score();
}

void
lose_life(void)
     /* Remove a life from the player. */
{
  if (lives==1) {
    show_hstable();
  }
  lives--;
  show_lives();
}

void expose_proc(Widget w,XtPointer client_data,XEvent *evt,Boolean *cnt);

void
clear_all(void)
     /* Remove all falling words. */
{
  Boolean garbage;
  while (nb_words > 0) {
    nb_words--;
    free_word(words+nb_words);
  }
  expose_proc(game_space,NULL,NULL,&garbage);
}

void
new_level(char nobonus)
     /* Switch to next level. */
{
  if (lifemode) clear_all();
  if (bonuslevel||nobonus||!lifemode) {
    level++;
    bonuslevel=0;
    levpoints=0;
    show_level();
  } else bonuslevel=1;
  level_time=current_time;
  lastword_time=-level_rate(level);
  keybufpos=0;
}

void
draw_word(word_state_t *w)
     /* Base routine for drawing a word - or erasing it. */
{
  XDrawString16(dpy,win,gcWT,w->x,w->y/1000,
                w->kanji,w->kanjityped);
  XDrawString16(dpy,win,gcW,w->x+w->kjtwidth,w->y/1000,
                w->kanji+w->kanjityped,w->kanjilen-w->kanjityped);
  if (w->comment != NULL)
    if ((showcomment==2) || ((showcomment>0) && (w->y/1000 > SHOWKANA_Y)))
      XDrawString16(dpy,win,gcC,w->x+w->coxoffs,w->y/1000+w->coyoffs,
                    w->comment,w->colen);
  if ((w->kana != NULL) && (w->purekana == 0)) {
    if ((showkana & 2) || ((showkana & 1) && (w->y/1000 > SHOWKANA_Y)))
      XDrawString16(dpy,win,gcKT,w->x+w->kaxoffs,w->y/1000+w->kayoffs,
		    w->kana,w->kanatyped);
    if ((showkana & 8) || ((showkana & 4) && (w->y/1000 > SHOWKANA_Y)))
      XDrawString16(dpy,win,gcK,w->x+w->kaxoffs+w->katwidth,
		    w->y/1000+w->kayoffs,
		    w->kana+w->kanatyped,
		    w->kanalen-w->kanatyped);
  }
}

void timer_proc(XtPointer client_data,XtIntervalId *id);

void
retime(void)
     /* Make sure the game is running. */
{
  paused = 0;
  timeout = XtAppAddTimeOut(app_context,BASETIME,timer_proc,NULL);
}

void
resume(void)
     /* Resume the game if it was paused. */
{
  if (paused)
    retime();
}

void
pauze(void)
     /* Pause the game if it wasn't. */
{
  if (!paused) {
    XtRemoveTimeOut(timeout);
    paused = 1;
  }
}

/* convert unicode string to an XChar2b structure */
void unicode_to_XChar2b(ucs4_t *in, XChar2b *out, int len) {
  int i;
  for (i=0; i<len; i++) {
    out[i].byte1 = (in[i] >> 8) & 0xff;
    out[i].byte2 = in[i] & 0xff;
  }
}

/* ********** MAIN GAME ROUTINES ********** */

void
quit_callback(Widget w,XtPointer client_data,XtPointer call_data)
     /* Callback called by the quit button */
{
  show_hstable();
}

void
pause_callback(Widget w,XtPointer client_data,XtPointer call_data)
     /* Callback called by the pause button */
{
  char b;
  XtVaGetValues(w,
                XtNstate,&b,
                NULL);
  if (b)
    pauze();
  else
    resume();
}

int in_dict(const ucs4_t *s);

void
send_callback(Widget w,XtPointer client_data,XtPointer call_data)
     /* Callback called by the send button */
{
  if (in_dict(deathword) == 2)
    {
      /* printf("%s\n",deathword); */
      fflush(stdout);
      /* this block may be inside or out the if.
       * inside = unfinished word not sent, stays
       * outside = unfinished word silently disapears (not sent)
       */
      deathword[0]=0;
      deathln=0;
      deathpend=0;
      in_dict(deathword); /* Reset dictionary pointer */
      XtVaSetValues(type_space,
                    XtNlabel,deathword,
                    XtNwidth,WIDTH,
                    NULL);
      /* This is the end of the block */
    }
}

void
next_callback(Widget w,XtPointer client_data,XtPointer call_data)
     /* Callback called by the next button */
{
  int i;
  long n;
  if (!bonuslevel)
    for (i=0;i<nb_words;i++)
      if (words[i].y>=HEIGHT*200) {
        XBell(dpy,0);
        return;
      }
  n=350-levpoints*2;
  if (n<0) n=0;
  vpoints+=n;
  show_score();
  new_level(1);
}

void
x_resume(void)
     /* Resume the game, untoggling the Pause button if necessary. */
{
  if (!paused) return;
  XtVaSetValues(pause_button,
                XtNstate,False,
                NULL);
  resume();
}

int textWidth(XChar2b *s, int len) /* calculate the width */
{
  int dir_ret,f_a_ret,f_d_ret;
  XCharStruct overall;
  XTextExtents16(fnt_info,s,len,&dir_ret,&f_a_ret,&f_d_ret,&overall);
  //debugmsg(2,"width=%d, lbearing=%d, rbearing=%d\n",
  //         overall.width, overall.lbearing, overall.rbearing);
  return overall.width;
}

/* move kana structure to beginning of chain */
void make_kana_first(word_state_t *pw, kana_state_t *pk) {
  kana_state_t **ppk;
  if (pw->kanastruct == pk) return; /* nothing to be done */
  /* first find kana struct pk in list */
  ppk = &(pw->kanastruct);
  while (*ppk != NULL) {
    if (pk == *ppk) break;
    ppk = &((*ppk)->next);
  }
  if (*ppk == NULL) {
    fprintf(stderr,"error: struct to be moved to front is not in list!");
    return;
  }
  /* remove struct from list */
  *ppk = pk->next;
  /* and reinsert at beginning */
  pk->next = pw->kanastruct;
  pw->kanastruct = pk;
  return;
}

/* update word structure as a function of prefs */
void update_word_struct(word_state_t *pw) {
  int i,max_kanji=0, max_kana=0, max_pend=0;
  kana_state_t *pk, *selected_kana;
  selected_kana = pw->kanastruct;
  /* loop on all kana structures */
  pk = pw->kanastruct;
  while (pk != NULL) {
    /* how many kanji correspond to typed kana part ? */
    if (pk->kanatyped > 0)
      i = pk->kana2kanji[pk->kanatyped-1];
    else
      i = 0;
    if (i > max_kanji) {/* structure with most completed kanji wins */
      max_kanji = i;
      max_kana = pk->kanatyped;
      max_pend = pk->pendtyped;
      selected_kana = pk;
      debugmsg(3,"kana struct at %p selected because kanji=%d\n",pk,i);
    } else if (i == max_kanji) {/* at equal number of completed kanji, */
      if (pk->kanatyped > max_kana) {/* struct with most completed kana wins */
        max_kana = pk->kanatyped;
        max_pend = pk->pendtyped;
        selected_kana = pk;
        debugmsg(3,"kana struct at %p selected because kana=%d\n",pk,max_kana);
      } else if ((pk->kanatyped==max_kana) && (pk->pendtyped >max_pend)) {
        max_pend = pk->pendtyped;/* finally consider pending keystrokes */
        selected_kana = pk;
        debugmsg(3,"kana struct at %p selected because pend=%d\n",pk,max_pend);
      }
    }
    pk = pk->next;
  }
  /* move selected kana to front of list */
  make_kana_first(pw, selected_kana);
  /* update word structure */
  pw->kanjityped = max_kanji;
  pw->kjtwidth = 2*textWidth(pw->kanji,pw->kanjityped);
  pw->kana = selected_kana->kanaX;
  pw->kanalen = selected_kana->kanalen;
  pw->kanatyped = selected_kana->kanatyped;
  pw->kawidth = selected_kana->kawidth;
  pw->katwidth = selected_kana->katwidth;
  pw->comment = selected_kana->commentX;
  pw->colen = selected_kana->colen;
  pw->cowidth = selected_kana->cowidth;
  return;
}

/* update word, typed parts, kana,... as a function of typed key and prefs */
char update_word(word_state_t *pw) {
  int i,j,k;
  char word_is_complete=0;
  word_state_t old_word;
  kana_state_t *pk;
  ucs4_t romaji[max_romaji];
  old_word = *pw;
  /* loop on all kana structures */
  pk = pw->kanastruct;
  while (pk != NULL) {
    pk->pendtyped++;
    /* copy pending keys */
    for (i=0;i<pk->pendtyped;i++)
      romaji[i] = keybuf[keybufpos-pk->pendtyped+i];
    if ((DEBUG > 1) && (pw==words)) {/*debug*/
      fprintf(stderr,"matching %d dead char(s): ",pk->pendtyped);
      for (k=0;k<pk->pendtyped;k++)
        fprintf(stderr,"%c(0x%x)",ASCII(romaji[k]),romaji[k]);
      fprintf(stderr," for");
      for (k=pk->kanatyped;k<pk->kanalen;k++)
        fprintf(stderr," 0x%x", *(pk->kana+k));
      fprintf(stderr,"\n");
    }
    fflush(stdout);
    /* does this complete a kana expression ? */
    /* first try direct matching */
    if (bcmp((char*)(pk->kana+pk->kanatyped),
             (char*)romaji, pk->pendtyped*sizeof(ucs4_t)) == 0) {
      if ((DEBUG > 1) && (pw==words)) /*debug*/
        fprintf(stderr,"  direct match !\n");
      pk->kanatyped += pk->pendtyped;
      pk->pendtyped = 0;
      pk->katwidth = 2*textWidth(pk->kanaX,pk->kanatyped);
      if (pk->kanatyped >= pk->kanalen) word_is_complete=1;
    } else {/* use the inputconverter */
      for (i=0;inputconverter[i].kana != NULL;i++) {
        /* could the kana #i be part of our word ? */
        j = pk->kanalen - pk->kanatyped - inputconverter[i].klen;
        if ((j >= 0) && (bcmp((char*)(pk->kana+pk->kanatyped),
                              inputconverter[i].kana,
                              inputconverter[i].klen*sizeof(ucs4_t)) == 0)) {
          /* if yes, is it matched by the typed input ? */
          j = pk->pendtyped<inputconverter[i].rlen?
            pk->pendtyped:inputconverter[i].rlen;
          if ((DEBUG > 1) && (pw==words)) {/*debug*/
            fprintf(stderr,"  comparing to first %d character(s) of ",j);
            for (k=0;k<inputconverter[i].rlen;k++)
              fprintf(stderr,"%c",ASCII(inputconverter[i].romaji[k]));
            fprintf(stderr," ... ");
          }
          if (bcmp(romaji, inputconverter[i].romaji, j*sizeof(ucs4_t)) == 0) {
            if (j == inputconverter[i].rlen) {/* full expression */
              if ((DEBUG > 1) && (pw==words)) {/*debug*/
                fprintf(stderr,"bingo, %d kana matched.\n",
                        inputconverter[i].klen);
              }
              pk->kanatyped += inputconverter[i].klen;
              pk->pendtyped = 0;
              pk->katwidth = 2*textWidth(pk->kanaX,pk->kanatyped);
              if (pk->kanatyped == pk->kanalen) word_is_complete=1;
            }
            else if ((DEBUG > 1) && (pw==words)) {/*debug*/
              fprintf(stderr,"ok, waiting for rest.\n");
            }
            /* if expression only partly typed, do nothing */
            break;
          }
          else if ((DEBUG > 1) && (pw==words)) {/*debug*/
            fprintf(stderr,"doesn't match.\n");
          }
        }
      }
      if (inputconverter[i].kana == NULL) {
        if ((DEBUG > 1) && (pw==words)) {/*debug*/
          fprintf(stderr,"  nothing matches, resetting dead chars...\n");
        }
#if CONTEXT
        fprintf(stderr,"CONTEXT == 1 not implemented yet...\n");
#endif
        pk->kanatyped = 0;
        pk->pendtyped = 0;
        pk->katwidth = 0;
      }
    }
    pk = pk->next;
  }
  /* update depending fields of the word structure */
  update_word_struct(pw);
  /* update on display */
  if (word_is_complete) {
    draw_word(&old_word);
  } else {
    draw_word(&old_word);
    draw_word(pw);
  }
  return word_is_complete;
}

/* returns 0 not existant, 1 possible, 2 match */
/* reset by *s = 0 */
int
in_dict(const ucs4_t *s)/* xxx broken */
{
  return 1;
}

void enter_word() /* Get new word falling. */
{
  int kj_asc,kj_desc,ka_asc,ka_desc,co_asc,co_desc;
  kana_state_t *pkana_item;
  words[nb_words].kanjityped = 0;
  /* calculate width of strings */
  {
    int dir_ret,f_a_ret,f_d_ret;
    XCharStruct overall;
    /* calculate width of kanji part */
    XTextExtents16(fnt_info,words[nb_words].kanji,
                   words[nb_words].kanjilen,
		   &dir_ret,&f_a_ret,&f_d_ret,&overall);
    words[nb_words].kjwidth = 2*overall.width;
    words[nb_words].kjtwidth = 0;
    kj_asc = fnt_info->ascent;
    kj_desc = fnt_info->descent;
    /* calculate width of kana and comment parts */
    ka_asc = 0; ka_desc = 0;
    co_asc = 0; co_desc = 0;
    pkana_item = words[nb_words].kanastruct;
    while (pkana_item != NULL) {
      XTextExtents16(fnt_info,pkana_item->kanaX,pkana_item->kanalen,
		     &dir_ret,&f_a_ret,&f_d_ret,&overall);
      pkana_item->kawidth = 2*overall.width;
      pkana_item->katwidth = 0;
      if (ka_asc < fnt_info->ascent) ka_asc=fnt_info->ascent;
      if (ka_desc < fnt_info->descent) ka_desc=fnt_info->descent;
      if (pkana_item->commentX != NULL) {
        XTextExtents16(fnt_info,pkana_item->commentX,pkana_item->colen,
  		     &dir_ret,&f_a_ret,&f_d_ret,&overall);
        pkana_item->cowidth = overall.width;
        if (co_asc < fnt_info->ascent) co_asc=fnt_info->ascent;
        if (co_desc < fnt_info->descent) co_desc=fnt_info->descent;
      }
      pkana_item = pkana_item->next;
    }
  }
  words[nb_words].kaxoffs = 0;
  words[nb_words].kayoffs = - (kj_asc + 2 + ka_desc);
  words[nb_words].coxoffs = 0;
  words[nb_words].coyoffs = kj_desc + 2 + co_asc;
  /* xxx following should take comment and furigana into account... */
  if (words[nb_words].kjwidth<WIDTH)
    words[nb_words].x = random()%(WIDTH-words[nb_words].kjwidth);
  else words[nb_words].x = 0;
  if (trainingmode) {
    words[nb_words].y = (ka_asc - words[nb_words].kayoffs
			 +random()%(HEIGHT -ka_asc +words[nb_words].kayoffs
				    -words[nb_words].coyoffs -co_desc))*1000;
  } else {
    /* xxx */
#if NOVCOLLIDE
    {
      int i;
      long y;
      long sep;
      sep=(fnt_info->ascent+fnt_info->descent)*1000;
      y=0;
      for (i=0;i<nb_words;i++) {
        if (y>words[i].y-sep)
          y=words[i].y-sep;
      }
      words[nb_words].y = y;
    }
#else
    words[nb_words].y = 0;
#endif
  }
  if (trainingmode) {
    words[nb_words].rate = 0;
  } else {
    words[nb_words].rate = level_speed(level)*7/(2+words[nb_words].kanjilen);
#if NOCOLLIDE
    {
      int i;
      int left;
      int right;
      char redo;
      left=words[nb_words].x;
      right=words[nb_words].x+words[nb_words].kjwidth;
      do {
        redo=0;
        for (i=0;i<nb_words;i++) {
          if ((words[i].x<right)&&(words[i].x+words[i].kjwidth>=left)) {
            if (words[i].rate<words[nb_words].rate)
              words[i].rate=words[nb_words].rate;
            if (words[i].x<left) {
              left=words[i].x;
              redo=1;
            }
            if (words[i].x+words[i].kjwidth>right) {
              right=words[i].x+words[i].kjwidth;
              redo=1;
            }
          }
        }
      } while (redo);
    }
#endif
  }
  update_word_struct(words+nb_words);
  if (DEBUG > 1) debug_word_info_1(words+nb_words);
  draw_word(words+nb_words);
  lastword_time=current_time;
  nb_words++;
}

/* recursive matching of kana to kanji */
char rcalc_kana2kanji(ucs4_t *kana, ucs4_t *kanji, int *k2k, 
                      int kalen, int kjlen, int base) {
  int pre, post, i, j, k;
  char result=0;
  debugmsg(2,"k2k: kana=%p, kanji=%p, k2k=%p, kalen=%d, kjlen=%d, base=%d\n",
           kana, kanji, k2k, kalen, kjlen, base);
  debugmsg(2,"k2k: kana=");
  for (i=0; i<kalen; i++) debugmsg(2," 0x%x",kana[i]);
  debugmsg(2,"\nk2k: kanji=");
  for (i=0; i<kjlen; i++) debugmsg(2," 0x%x",kanji[i]);
  debugmsg(2,"\n");
  /* check for common pre- and post-fix */
  for (pre=0; pre<kjlen; pre++) {
    if (kana[pre] != kanji[pre]) break;
    k2k[pre] = base+1+pre;
  }
  for (i=pre; i<kalen; i++) k2k[i] = base+pre;
  for (post=0; post<kjlen; post++) {
    k2k[kalen-1-post] = base+kjlen-post;
    if (kana[kalen-1-post] != kanji[kjlen-1-post]) break;
  }
  debugmsg(2,"k2k: found %d pre- and %d post-matching kana\n", pre, post);
  if (pre+post+1 >= kalen) return 0;/* k2k already fully initialized */
  if (pre+post >= kjlen) {
    fprintf(stderr,"strange: kanji is a true subset of kana !\n");
    return 0;
  }
  if (pre+post+1 == kjlen)/* 1 kanji remaining: unique mapping, already done */
    return 0;
  /* more complex: try to decompose */
  /* nothing simple, try to decompose */
  debugmsg(2,"k2k: trying to find matching kana...");
  for (i=pre+1; i<kjlen-1-post; i++) {
    for (j=pre+1; j<kalen-1-post; j++) 
      if (kanji[i] == kana[j]) {
        debugmsg(2," (%d:%d)",j,i);
        for (k=j+1; k<kalen-1-post; k++)
          if (kanji[i] == kana[k]) break;
        if (k>=kalen-1-post) { /* unique matching kana */
          debugmsg(2," unique, recurring\n");
          k2k[j] = base+1+i;
          result = rcalc_kana2kanji(kana+pre,kanji+pre,k2k+pre,
                           j-pre,i-pre,base+pre);
          result |= rcalc_kana2kanji(kana+j+1,kanji+i+1,k2k+j+1,
                           kalen-1-post-j,kjlen-1-post-i,k2k[j]);
          return result;
        }
      }
  }
  debugmsg(2," none\nk2k: looking for marker...");
  /* last hope: find a marker */
  for (j=pre+1; j<kalen-post; j++)
    if (kana[j] == UCS4_POINT) break;
  if (j<kalen-post) {/* we have a marker */
    for (i=1; i<kalen-post-j; i++)/* how many markers ? */
      if (kana[j+i] != UCS4_POINT) break;
    debugmsg(2," found at %d (%d total)...",j,i);
    if (i > kjlen-pre-post) {
      fprintf(stderr,"too many point markers in kana transcription ?\n");
      return 1;
    }
    for (k=i*(j-pre); k>0; k--)/* check that first i kanji contain no kana */
      if (kanji[pre+k%(j-pre)] == kana[pre+k%i]) break;
    if (k==0) {
      k2k[j-1] = base+pre+i;
      if (j+i < kalen-post) {
        debugmsg(2," exploitable, recurring\n");
        result = rcalc_kana2kanji(kana+j+i,kanji+pre+i,k2k+j+i,
                         kalen-j-i-post,kjlen-pre-post-i,k2k[j-1]);
      } else {
        debugmsg(2," completes this part\n");
        result = 0;
      }
      return result;
    }
  }
  /* Some blocks of kanji resisted our effort */
  debugmsg(2," none, will print warning\n");
  return 1;
}

int calc_kana2kanji(ucs4_t *kana, ucs4_t *kanji, int *k2k, 
                    int kalen, int kjlen, int wordfline) {
  int i, j;
  if (rcalc_kana2kanji(kana,kanji,k2k,kalen,kjlen,0)) {
    fprintf(stderr,"Can't fully establish kanji:kana correspondence for wordfile line %d, sorry\n",wordfline);
    if (explain_k2k_error%16 == 0) {
      fprintf(stderr,"You can help me with markers ! See dictionary.html\n");
      explain_k2k_error++;
    }
  }
  /* delete markers */
  for (i=kalen-1; i>=0; i--)
    if (kana[i] == UCS4_POINT) {
      for (j=i+1; j<kalen; j++) {
        kana[j-1] = kana[j];
        k2k[j-1] = k2k[j];
      }
      kalen--;
    }
  /* integrity test */
  for (i=0; i<kalen-1; i++) {
    if ((k2k[i] < 0) || (k2k[i] >kjlen)) {
      fprintf(stderr,"index %d:%d out of range in kana2kanji\n",
              i,k2k[i]);
      k2k[i]=0; /* can otherwise make program crash */
    }
    if (i>0 && k2k[i] < k2k[i-1]) {
      fprintf(stderr,"decrease from index %d:%d to %d:%d shouldn't occur\n",
              i-1,k2k[i-1],i,k2k[i]);
    }
  }
  debugmsg(1,"kana2kanji={");
  for (i=0; i<kalen-1; i++)
    debugmsg(1,"%d,",k2k[i]);
  debugmsg(1,"%d}\n",k2k[kalen-1]);
  return kalen;
}

kana_state_t **pure_kana_word(word_state_t *pw, ucs4_t *kanji) {
  int i;
  int *k2k;
  ucs4_t *kana;
  XChar2b *kanaX;
  kana_state_t *pk;

  pk = malloc(sizeof(kana_state_t));
  kana = malloc((pw->kanjilen)*sizeof(ucs4_t));
  kanaX = malloc((pw->kanjilen)*sizeof(ucs4_t));
  k2k = malloc((pw->kanjilen)*sizeof(int));
  if (kana==NULL || kanaX==NULL || pk==NULL || k2k==NULL) {
    perror("Out of memory");
    exit(1);
  }
  for (i=0;i<pw->kanjilen;i++) {
    kana[i] = kanji[i];
    k2k[i] = i+1;
  }
  unicode_to_XChar2b(kana,kanaX,pw->kanjilen);
  pk->kana = kana;
  pk->kanaX = kanaX;
  pk->kanalen = pw->kanjilen;
  pk->kana2kanji = k2k;
  pk->kanatyped = 0;
  pk->pendtyped = 0;
  pk->comment = NULL;
  pk->commentX = NULL;
  pk->next = NULL;
  pw->kanastruct = pk;
  pw->purekana = -1;
  debugmsg(3,"completed as pure kana word\n");
  return &(pk->next);
}

/* Select one transcription and throw away the rest */
void preselect_kana(word_state_t *pw) {
  int n=0, choice;
  kana_state_t *pk,**ppk;
  pk = pw->kanastruct;
  while (pk != NULL) {/* count kana transcriptions */
    n++;
    pk = pk->next;
  }
  if (n>1) {
    choice = random()%n;/* choose one randomly */
    debugmsg(2,"preselecting transcription no.%d/%d\n",choice+1,n);
    pk = pw->kanastruct;
    ppk = &(pw->kanastruct);
    n=0;
    while (n<choice) {
      ppk = &((*ppk)->next);
      n++;
    }
    /* isolate it */
    pk = *ppk;
    *ppk = pk->next;
    /* insert it as first and only */
    ppk = &(pw->kanastruct);
    pk->next = NULL;
    pw->kanastruct = pk;
    /* free the rest */
    while (*ppk != NULL) {
      pk = *ppk;
      ppk = &(pk->next);
      if (pk->kana != NULL) free(pk->kana);
      if (pk->kanaX != NULL) free(pk->kanaX);
      if (pk->kana2kanji != NULL) free(pk->kana2kanji);
      if (pk->comment != NULL) free(pk->comment);
      if (pk->commentX != NULL) free(pk->commentX);
      free(pk);
    }
  }
  return;
}

/* Create a new word to let fall. If dentry_num is non-negative, it
   indicates the dictionary entry to use */
void new_word(int dentry_num)
{
  int i,j,len;
  int *k2k;
  XChar2b *kanjiX,*kanaX,*commentX;
  ucs4_t *kanji,*kana,*comment;
  word_state_t *pw;
  kana_state_t *pk;
  kana_state_t **ppkana_next;

  if (nb_words>=MAXWORDS) { return; }
  pw=words+nb_words;
  pw->kanastruct=NULL;
  pw->purekana = 0;
  if ((!trainingmode)&&bonuslevel) {
    len = random()%5+5;
    kanji = malloc(len*sizeof(ucs4_t));
    kanjiX = malloc(len*sizeof(XChar2b));
    if (kanji==NULL || kanjiX==NULL) {
      perror("Out of memory");
      exit(1);
    }
    for (i=0;i<len;i++) {
      kanji[i] = 33+random()%94;
    }
    debugmsg(1,"new random word: %d chars:",len);
    for (i=0;i<len;i++) debugmsg(1," 0x%x",kanji[i]);
    debugmsg(1,"\n");
    unicode_to_XChar2b(kanji,kanjiX,len);
    pw->kanji = kanjiX;
    pw->kanjilen = len;
    pure_kana_word(pw, kanji);
  } else {
    int choice,kanjilen,curpos=0; /* position in dictionary entry */
    ucs4_t *dict_entry;

    if ((dentry_num >= 0) && (dentry_num < dict_words))
      choice = dentry_num;
    else
      choice = random()%dict_words; /* choose one word randomly */
    /* xxx add difficulty selection (frequency, jouyou grade, ...) here */
    dict_entry = dictionary[choice];
    debugmsg(1,"new word %d: selected dictionary entry %d\n",nb_words,choice);
    while ((dict_entry[curpos] != UCS4_SPC) && 
           (dict_entry[curpos] != UCS4_NULL)) curpos++;
    kanjilen = curpos;
    /* malloc() and copy kanji part */
    kanji = calloc(kanjilen,sizeof(ucs4_t));
    kanjiX = calloc(kanjilen,sizeof(XChar2b));
    if ((kanji==NULL) || (kanjiX==NULL)) {
      perror("Out of memory");
      exit(1);
    }
    for (i=0;i<kanjilen;i++) kanji[i] = dict_entry[i];
    unicode_to_XChar2b(kanji,kanjiX,kanjilen);
    pw->kanji = kanjiX;
    pw->kanjilen = kanjilen;
    debugmsg(3,"%d kanji at %p\n",kanjilen,kanjiX);
    /* rewind to first entry with same kanji content */
    while (choice>0 && (bcmp(dictionary[choice-1], dict_entry,
			     (kanjilen+1)*sizeof(ucs4_t))==0)) choice--;
    ppkana_next = &(pw->kanastruct);
    do { /* for all dict-entries with same kanji content */
      dict_entry = dictionary[choice];
      /* calculate line length */
      for (len = kanjilen; dict_entry[len] != 0; len++);
      curpos = kanjilen;
      while (curpos < len) {/* parse dictionary entry (line) */
	if (dict_entry[curpos] == UCS4_LSQUAREBRACKET) {/* kana tag */
	  for (j=++curpos;curpos<len;curpos++)
	    if (dict_entry[curpos] == UCS4_RSQUAREBRACKET) break;
	  if (curpos==len) 
	    fprintf(stderr,
		    "runaway kana tag in dictionary line %d\n",choice+1);
	  pk = malloc(sizeof(kana_state_t));
	  kana = malloc((curpos-j)*sizeof(ucs4_t));
	  kanaX = malloc((curpos-j)*sizeof(XChar2b));
	  k2k = malloc((curpos-j)*sizeof(int));
	  if (kana==NULL || kanaX==NULL || pk==NULL || k2k==NULL) {
	    perror("Out of memory");
	    exit(1);
	  }
	  for (i=0;i<curpos-j;i++)
	    kana[i] = dict_entry[j+i];
	  pk->kana = kana;
	  pk->kanalen = calc_kana2kanji(kana,kanji,k2k,
                                                curpos-j,kanjilen,choice+1);
          unicode_to_XChar2b(kana,kanaX,pk->kanalen);
          pk->kanaX = kanaX;
	  pk->kana2kanji = k2k;
	  pk->kanatyped = 0;
	  pk->pendtyped = 0;
	  pk->comment = NULL;
	  pk->next = NULL;
          debugmsg(3,"%d new kana, pk=%p, kana=%p, kanaX=%p, kana2kanji=%p\n",
                   pk->kanalen, pk, kana, kanaX, k2k);
	  *ppkana_next = pk;
	  ppkana_next = &(pk->next);
	}
	if (dict_entry[curpos] == UCS4_SLASH) {/* comment tag */
	  for (j=++curpos;curpos<len;curpos++)
	    if ((dict_entry[curpos] == UCS4_SLASH) &&
		((dict_entry[curpos+1] == UCS4_SPC) ||
		 (dict_entry[curpos+1] == UCS4_NULL))) break;
	  if (curpos==len)
	    fprintf(stderr,
		    "runaway comment tag in dictionary line %d\n",choice+1);
          /* found comment is now copied into all preceding
             kanastruct's which do not yet have any */
          /* first check if we have a pure kana word */
          if (pw->kanastruct == NULL) 
            ppkana_next = pure_kana_word(pw, kanji);
          /* now loop over all kana entries created so far */
	  pk = pw->kanastruct;
	  while (pk != NULL) {
	    if (pk->comment == NULL) {
	      comment = malloc((curpos-j)*sizeof(ucs4_t));
	      commentX = malloc((curpos-j)*sizeof(XChar2b));
	      if (comment==NULL || commentX==NULL) {
		perror("Out of memory");
		exit(1);
	      }
	      for (i=0;i<curpos-j;i++)
		comment[i] = dict_entry[j+i];
              unicode_to_XChar2b(comment,commentX,curpos-j);
	      pk->comment = comment;
	      pk->commentX = commentX;
	      pk->colen = curpos-j;
              debugmsg(3,"comment for pk=%p: comment=%p, coX=%p, colen=%d\n",
                       pk, comment, commentX, curpos-j);
	    }
	    pk = pk->next;
	  }
	}
	if (dict_entry[curpos] == UCS4_LT) {/* meta tag */
	  for (j=++curpos;curpos<len;curpos++)
	    if (dict_entry[curpos] == UCS4_GT) break;
	  if (curpos==len)
	    fprintf(stderr,
		    "runaway meta tag in dictionary line %d\n",choice+1);
	}
	curpos++;
      }
    } while ((++choice)<dict_words && (bcmp(dict_entry, dictionary[choice],
					    (kanjilen+1)*sizeof(ucs4_t))==0));
    /* complete word if we haven't found any kana */
    if (pw->kanastruct == NULL) 
       ppkana_next = pure_kana_word(pw, kanji);
    else if (showkana & 16) preselect_kana(pw);
  }
  free(kanji);
  enter_word();
  return;
}

void
input_proc(XtPointer client_data,int *source,XtInputId *id)
     /* Called when the standard input has something to say */
{
  char tmp[MAX_WORDSIZE+1];
  int len;

  fgets(tmp,MAX_WORDSIZE+1,stdin);
  for (len=0;(((unsigned char)tmp[len])>=33);len++);
  tmp[len]=0;
  /* len = strlen(tmp); */
  /* xxx do input-conversion and find word in dictionary */
  new_word(42/*xxx*/);/* create word from this dict_entry */
}

void
timer_proc(XtPointer client_data,XtIntervalId *id)
     /* Timeout handler called every time unit (when the game is
      * unpaused) */
{
  int i;
  current_time++;
  if (current_time>=level_time+LEVELDUR)
    new_level(0);
  for (i=0;i<nb_words;i++) {
    word_state_t w;
    w = words[i];
    words[i].y+=words[i].rate;
    if (words[i].y>HEIGHT*1000) {
      if (!bonuslevel) {
        int j;
        nb_words--;
        for (j=i;j<nb_words;j++)
          words[j]=words[j+1];
        i--; /* YUCK ALERT */
        draw_word(&w);
        free_word(&w);
        lose_life();
      } else {
        new_level(0);
        break;
      }
    } else {
      draw_word(words+i);
      draw_word(&w);
    }
  }
  if (lifemode&&(current_time>=lastword_time+level_rate(level))) {
    new_word(-1);
  }
  retime();
}

#define KBD_1BUF_SIZE 256 /* Rather ridiculous: 1 should be enough */

void
key_proc(Widget w,XtPointer client_data,XEvent *evt,Boolean *cnt)
     /* Event handler called when a key is pressed */
{
  int i,j;
  long score;
  char buf[KBD_1BUF_SIZE];
  char romaji[max_romaji+1];
  int buflen;
  int p;
  KeySym keysym;
  Boolean mode;
  buflen=XLookupString(&evt->xkey,buf,KBD_1BUF_SIZE,&keysym,NULL);
  for (p=0;p<buflen;p++) {
    if ((buf[p]<=32)||(buf[p]>=127))
      continue;
    if (!trainingmode) x_resume();
    keybuf[keybufpos++]=buf[p];
    if (keybufpos >= MAX_WORDSIZE+KEYBUF_MARGIN) {
      for (i=0;i<MAX_WORDSIZE;i++) keybuf[i]=keybuf[i+KEYBUF_MARGIN];
      keybufpos -= KEYBUF_MARGIN;
    }
    if (deathmode)
      XtVaGetValues(mode_button,XtNstate,&mode,NULL);
    else mode=0;
    if (mode) {
      if (deathln<MAX_WORDSIZE) {/* xxx to do: input-conversion ! */
        deathpend ++;
        for (i=0;i<deathpend;i++)
          romaji[i] = keybuf[keybufpos-deathpend+i];
        romaji[i] = 0;
        printf("%d dead chars in type_space: %s\n",deathpend,romaji);
        fflush(stdout);

        deathword[deathln]=buf[p];
        deathword[deathln+1]=0;
        deathln++;
        if (in_dict(deathword) == 0)
          {
            deathword[0] = 0;
            deathln = 0;
            in_dict(deathword);
          }
        XtVaSetValues(type_space,
                      XtNlabel,deathword,
                      XtNwidth,WIDTH,
                      NULL);
      }
    } else {
      for (i=nb_words-1;i>=0;i--) {
	if (update_word(words+i)) {
          debugmsg(1,"word no.%d has been destroyed\n",i);
          score = word_score(words+i);
          free_word(words+i);
	  nb_words--;
	  for (j=i;j<nb_words;j++)
	    words[j]=words[j+1];
	  if (!trainingmode)
	    add_score(score);
	  else while (nb_words==0) new_word(-1);
	}
      }
      if ((DEBUG > 1) && (nb_words > 0)) debug_word_info_2(words);
    }
  }
  *cnt=True;
}

void
expose_proc(Widget w,XtPointer client_data,XEvent *evt,Boolean *cnt)
     /* Event handler called when an Expose event is received */
{
  int i;
  XFillRectangle(dpy,win,gc0,0,0,WIDTH,HEIGHT);
  for (i=0;i<nb_words;i++)
    draw_word(words+i);
  *cnt=False;
}

/* Open the word file. */
void wordopen(const char *wordfile)
{
  FILE *word_fd;
  long int word_chars,word_lines,cursor,count,produced,consumed;
  ucs4_t *word_mem;
  char *buffer;

  errno = 0;
  word_fd = fopen(wordfile,"r");
  if (word_fd == NULL) {
    perror(wordfile);
    exit(1);
  }
  /* count number of lines in word file */
  buffer = alloca(6*MAX_WORDSIZE);
  if (buffer == NULL) {
    perror(wordfile);
    exit(1);
  }
  word_chars = 0;
  word_lines = 0;
  while (fgets(buffer,6*MAX_WORDSIZE,word_fd)) {
    word_chars += strlen(buffer);
    word_lines++;
  }
  rewind(word_fd);
  debugmsg(2,"word file has %ld chars, %ld lines\n", word_chars,word_lines);
  word_mem = (ucs4_t*) malloc((word_chars+1)*sizeof(ucs4_t));/* worst case */
  dictionary = (ucs4_t**) malloc(word_lines*sizeof(ucs4_t*));
  if ((word_mem == NULL) || (dictionary == NULL)) {
    perror("allocating dictionary");
    exit(1);
  }
  dict_words = word_lines;
  count = 0;
  while (fgets(buffer,6*MAX_WORDSIZE,word_fd)) {
    for (cursor = 0; cursor < strlen(buffer); cursor++)
      if (buffer[cursor] == '\n') {
        buffer[cursor] = '\0';
        break;
      }
    // convert line to UCS-4
    dictionary[count] = word_mem;
    cursor = 0;
    produced = 0;
    while (cursor < strlen(buffer)) {
      consumed=utf8_mbtowc(word_mem, buffer+cursor, strlen(buffer+cursor));
      if (consumed>0) {
        word_mem++;
        cursor += consumed;
      } else {
        switch (consumed) {
        case RET_TOOFEW:
          fprintf(stderr,"premature end of line in word file\n");
          break;
        case RET_ILSEQ:
          fprintf(stderr,
                  "not a valid UTF-8 sequence in word file\n");
          break;
        }
        fprintf(stderr,"fatal error reading word file\n");
        fprintf(stderr,"at line no. %ld: %s\n",count,buffer);
        exit(1);
      }
    }
    *(word_mem++) = 0;/* terminate entry with 0 */
    count++;
  }
  if (count != dict_words)
      fprintf(stderr,"number of lines in wordfile changed from %ld to %ld ?\n",
         dict_words, count);
  debugmsg(1,"dictionary contains %ld words\n",dict_words);
  for (count=0;count<dict_words;count++) {
    debugmsg(3,"entry %ld at %p\n",count,dictionary[count]);
  }
  fclose(word_fd);
  return;
}

/* Open the input-conversion file. */
void inputconvopen(const char *iconvfile)
{
  FILE *iconv_fd;
  long int iconv_lines,iconv_chars,count;
  int cursor,consumed;
  ucs4_t *iconv_mem;
  char *buffer;

  errno = 0;
  iconv_fd = fopen(iconvfile,"r");
  if (iconv_fd < 0)
    {
      perror(iconvfile);
      exit(1);
    }
  /* count number of lines in conversion file */
  buffer = alloca(6*MAX_WORDSIZE);
  if (buffer == NULL) {
    perror(iconvfile);
    exit(1);
  }
  iconv_chars = 0;
  iconv_lines = 0;
  while (fgets(buffer,6*MAX_WORDSIZE,iconv_fd)) {
    if ((*buffer != '\n') &&
        (*buffer != '#') &&
        (*buffer != '$') &&
        (*buffer != ' ')) {
      iconv_chars += strlen(buffer);
      iconv_lines++;
    }
  }
  rewind(iconv_fd);
  debugmsg(2,"input conversion file has %ld chars, %ld lines\n",
           iconv_chars, iconv_lines);
  iconv_mem = (ucs4_t*) malloc(iconv_chars*sizeof(ucs4_t));
  inputconverter = (iconv_t*) malloc((iconv_lines+1)*sizeof(iconv_t));
  if ((iconv_mem == NULL) || (inputconverter == NULL)) {
    perror("allocating inputconverter");
    exit(1);
  }
  count = 0;
  while (fgets(buffer,6*MAX_WORDSIZE,iconv_fd)) {
    if ((*buffer != '\n') &&
        (*buffer != '#') &&
        (*buffer != '$') &&
        (*buffer != ' ')) {
      // first convert kanji part to UCS-4
      inputconverter[count].kana = iconv_mem;
      inputconverter[count].klen = 0;
      cursor = 0;
      while ((buffer[cursor] != '\40') && (buffer[cursor] != '\0')) {
        consumed=utf8_mbtowc(inputconverter[count].kana
                             +inputconverter[count].klen,
                             buffer+cursor, strlen(buffer+cursor));
        if (consumed>0) {
          inputconverter[count].klen++;
          cursor += consumed;
        } else {
          switch (consumed) {
          case RET_TOOFEW:
            fprintf(stderr,"premature end of line in input-conversion file\n");
            break;
          case RET_ILSEQ:
            fprintf(stderr,
                    "not a valid UTF-8 sequence in input-conversion file\n");
            break;
          }
          fprintf(stderr,"fatal error reading input-conversion file\n");
          fprintf(stderr,"at expression no. %ld: %s\n",count,buffer);
          exit(1);
        }
      }
      iconv_mem += inputconverter[count].klen;
      // skip to romaji transcription
      if (buffer[cursor] == '\0') {
        fprintf(stderr,"line too short in input-conversion file (fatal)\n");
        fprintf(stderr,"at expression no. %ld: %s\n",count,buffer);
        exit(1);
      }
      while (buffer[cursor] == '\40') cursor++;
      // now convert romaji part
      inputconverter[count].romaji = iconv_mem;
      inputconverter[count].rlen = 0;
      while ((buffer[cursor] != '\40') && (buffer[cursor] != '\n') && 
             (buffer[cursor] != '\0')) {
        consumed=utf8_mbtowc(inputconverter[count].romaji
                             +inputconverter[count].rlen,
                             buffer+cursor, strlen(buffer+cursor));
        if (consumed>0) {
          inputconverter[count].rlen++;
          cursor += consumed;
        } else {
          switch (consumed) {
          case RET_TOOFEW:
            fprintf(stderr,"premature end of line in input-conversion file\n");
            break;
          case RET_ILSEQ:
            fprintf(stderr,
                    "not a valid UTF-8 sequence in input-conversion file\n");
            break;
          }
          fprintf(stderr,"fatal error reading input-conversion file\n");
          fprintf(stderr,"at expression no. %ld: %s\n",count,buffer);
          exit(1);
        }
      }
      iconv_mem += inputconverter[count].rlen;
      // new converter entry complete
      count++;
    }
  }
  fclose(iconv_fd);
  debugmsg(1,"inputconverter contains %ld expressions\n",count);
  inputconverter[count].kana = NULL; /* NULL entry marks end */
  inputconverter[count].romaji = NULL;
  max_romaji = 0;
  while (--count>=0)
    if (max_romaji < inputconverter[count].rlen)
      max_romaji = inputconverter[count].rlen;
  if (DEBUG > 2) {
    int i;
    count=0;
    while (inputconverter[count].kana != NULL) {
      fprintf(stderr,"rlen=%d, klen=%d, romaji=",
              inputconverter[count].rlen,
              inputconverter[count].klen);
      for (i=0;i<inputconverter[count].rlen;i++)
        fprintf(stderr," %x",inputconverter[count].romaji[i]);
      fprintf(stderr,", kana=");
      for (i=0;i<inputconverter[count].klen;i++)
        fprintf(stderr," %x",inputconverter[count].kana[i]);
      fprintf(stderr,"\n");
      count++;
    }
  }
  return;
}


/* The main function (yeah, this is too long) */

int
main(int argc,char **argv)
{
  /* Save uid and gid for later */
  euid = geteuid();
  egid = getegid();
  seteuid(getuid());
  setegid(getgid());
  /* Randomize. */
  srandom(time(NULL));
  /* read dictionary and input conversion files */
  wordopen(WORDFILE);
  inputconvopen(ICONVFILE);
  /* Read application resources. */
  toplevel = XtVaAppInitialize(&app_context,APP_CLASS,
                               options,XtNumber(options),
                               &argc,argv,fallback_resources,
                               NULL);
  if (argc>1) {
    /* We don't deal properly with GNU long options, but I don't
     * intend to link with GNU getopt simply so people are able to
     * type --vers instead of --version */
    if ((strcmp(argv[1],"-version")==0)||(strcmp(argv[1],"-v")==0)
        ||(strcmp(argv[1],"-V")==0)
        ||(strcmp(argv[1],"--version")==0)||(strcmp(argv[1],"--ver")==0)) {
      fprintf(stderr,
              "xletters - catch falling words\n"
              "Version jp-2.2\n"
              "Copyright (C) 1998 by\n"
              "  Peter Horvai (peter.horvai@ens.fr)\n"
              "  David A. Madore (david.madore@ens.fr)\n"
              "Copyright (C) 2003 by\n"
              "  Adrian Daerr (adrian.daerr@gmx.net)\n"
              "xletters comes with ABSOLUTELY NO WARRANTY:\n"
              "see the GNU general public license for more details.\n"
              );
      exit(0);
    } else if ((strcmp(argv[1],"-help")==0)||(strcmp(argv[1],"-h")==0)
               ||(strcmp(argv[1],"--help")==0)) {
      fprintf(stderr,
              "Usage is:\n"
              "%s [-version] [-help] [-bg color] [-fg color] [-fn font]\n"
              "[-name name] [-title string] [-geometry geom] [-display disp]\n"
              "[-xrm resource] [-wfn font] [-wc color] [-tc color] [-gbg color]\n"
              "[-notrain] [-train] [-nodeath] [-death] [-duel]\n",
              argv[0]);
      exit(0);
    } else {
      fprintf(stderr,"Unrecognized option %s\n",argv[1]);
      fprintf(stderr,"Type %s -help for help\n",argv[0]);
      exit(1);
    }
  }
  XtGetApplicationResources(toplevel,&app_res_val,app_res_tab,
                            XtNumber(app_res_tab),NULL,0);
  if (strcmp(app_res_val.death_mode,"normal")==0) {
    deathmode=0;
    lifemode=1;
  } else if (strcmp(app_res_val.death_mode,"death")==0) {
    deathmode=1;
    lifemode=1;
  } else if (strcmp(app_res_val.death_mode,"duel")==0) {
    deathmode=1;
    lifemode=0;
  } else {
    fprintf(stderr,"Unrecognized deathmode %s\n",app_res_val.death_mode);
    exit(1);
  }
  trainingmode = app_res_val.training_mode;
  if (trainingmode) { deathmode=0; lifemode=0; }
  scorecounts = !(deathmode||trainingmode);
  showkana = app_res_val.show_kana;
  if (showkana & 16) showkana |= 8; /* to avoid frustration */
  showcomment = app_res_val.show_comment;
  debugmsg(1,"showkana=%d, showcomment=%d\n",showkana,showcomment);
  /* Create widgets. */
  ground_box = XtVaCreateManagedWidget("groundBox",boxWidgetClass,toplevel,
                                       NULL);
  label = XtVaCreateManagedWidget("label",labelWidgetClass,ground_box,
                                  XtNlabel,"xletters",
                                  NULL);
  quit_button = XtVaCreateManagedWidget("quitButton",commandWidgetClass,
                                        ground_box,
                                        NULL);
  if (!trainingmode)
    pause_button = XtVaCreateManagedWidget("pauseButton",toggleWidgetClass,
                                           ground_box,
                                           NULL);
  if (deathmode)
    mode_button = XtVaCreateManagedWidget("modeButton",toggleWidgetClass,
                                          ground_box,
                                          NULL);
  if (deathmode)
    send_button = XtVaCreateManagedWidget("sendButton",commandWidgetClass,
                                          ground_box,
                                          NULL);
  if (lifemode)
    next_button = XtVaCreateManagedWidget("nextButton",commandWidgetClass,
                                          ground_box,
                                          NULL);
  if (!trainingmode) {
    lives_label = XtVaCreateManagedWidget("livesLabel",labelWidgetClass,
                                          ground_box,
                                          NULL);
    score_label = XtVaCreateManagedWidget("scoreLabel",labelWidgetClass,
                                          ground_box,
                                          NULL);
    level_label = XtVaCreateManagedWidget("levelLabel",labelWidgetClass,
                                          ground_box,
                                          NULL);
  }
  if (deathmode)
    type_space = XtVaCreateManagedWidget("typeSpace",labelWidgetClass,
                                         ground_box,
                                         XtNwidth,WIDTH,
                                         XtNlabel,"",
                                         XtNencoding,XawTextEncodingChar2b,
                                         XtNfont,app_res_val.word_font,
                                         NULL);
  game_space = XtVaCreateManagedWidget("gameSpace",coreWidgetClass,ground_box,
                                       XtNwidth,WIDTH,
                                       XtNheight,HEIGHT,
                                       NULL);
  XtRealizeWidget(toplevel);
  /* Xlib miscellanea */
  dpy = XtDisplay(game_space);
  win = XtWindow(game_space);
  gc0 = XCreateGC(dpy,win,0,NULL);
  {
    XGCValues val;
    val.function = GXxor;
    gcW = XCreateGC(dpy,win,GCFunction,&val);
    gcWT = XCreateGC(dpy,win,GCFunction,&val);
    gcK = XCreateGC(dpy,win,GCFunction,&val);
    gcKT = XCreateGC(dpy,win,GCFunction,&val);
    gcC = XCreateGC(dpy,win,GCFunction,&val);
  }
  { /* Set foreground colors */
    Pixel bgc,c;
    XtVaGetValues(game_space,
                  XtNbackground,&bgc,
                  NULL);
    XSetForeground(dpy,gc0,bgc);
    c = app_res_val.word_color;
    XSetForeground(dpy,gcW,bgc^c);
    c = app_res_val.typed_color;
    XSetForeground(dpy,gcWT,bgc^c);
    c = app_res_val.kana_color;
    XSetForeground(dpy,gcK,bgc^c);
    c = app_res_val.kanaTyped_color;
    XSetForeground(dpy,gcKT,bgc^c);
    c = app_res_val.comment_color;
    XSetForeground(dpy,gcC,bgc^c);
  }
  /* Set text fonts */
  fnt_info = app_res_val.word_font;
  fnt = fnt_info->fid;
  XSetFont(dpy,gcW,fnt);
  XSetFont(dpy,gcWT,fnt);
  fnt_info = app_res_val.kana_font;
  fnt = fnt_info->fid;
  XSetFont(dpy,gcK,fnt);
  XSetFont(dpy,gcKT,fnt);
  fnt_info = app_res_val.comment_font;
  fnt = fnt_info->fid;
  XSetFont(dpy,gcC,fnt);
  /* Setup callbacks and handlers */
  if (!trainingmode)
    retime();
  if (deathmode)
    XtAppAddInput(app_context,0,(XtPointer)XtInputReadMask,input_proc,NULL);
  XtAddCallback(quit_button,XtNcallback,quit_callback,NULL);
  if (!trainingmode)
    XtAddCallback(pause_button,XtNcallback,pause_callback,NULL);
  if (deathmode)
    XtAddCallback(send_button,XtNcallback,send_callback,NULL);
  if (lifemode)
    XtAddCallback(next_button,XtNcallback,next_callback,NULL);
  XtAddEventHandler(game_space,KeyPressMask,False,key_proc,NULL);
  if (deathmode)
    XtAddEventHandler(type_space,KeyPressMask,False,key_proc,NULL);
  XtAddEventHandler(game_space,ExposureMask,False,expose_proc,NULL);
  XtInstallAllAccelerators(game_space,ground_box);
  if (deathmode)
    XtInstallAllAccelerators(type_space,ground_box);
  XtInstallAllAccelerators(ground_box,ground_box);
  /* Etcetera */
  if (!trainingmode) {
    lives = 5;
    score = 0;
    vpoints = 0;
    level = 1;
    bonuslevel = 0;
    lastword_time = -level_rate(level);
    show_lives();
    show_score();
    show_level();
  } else while (nb_words==0) new_word(-1);
  XtAppMainLoop(app_context);
  return 0;
}

/* X Letters - Catch falling words */
/* Copyright (C) 1998 by
 *   Peter Horvai (peter.horvai@ens.fr)
 *   David A. Madore (david.madore@ens.fr)
 * Copyright (C) 2003 by
 *   Adrian Daerr (adrian.daerr@pmmh.espci.fr)
 */

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


/* The two following constants determine the size of the game space,
 * in pixels.  Note that they are intentionally not modifiable through
 * resources, as that would be cheating. */

/* The width of the play area, in pixels */
#define WIDTH 1024

/* The height of the play area, in pixels */
#define HEIGHT 1024



/* The following four constants are game parameters.  They do not
 * change the ``look'' of the game but rather its ``feel''. */

/* The maximal number of words simultaneously falling */
#define MAXWORDS 256

/* Whether the program should avoid at all cost the collision between
 * two words */
#define NOCOLLIDE 1

/* Whether the program should avoid at all cost the vertical collision
 * between two words (in case the deathmatch sends many words very
 * fast, or during a pause).  This is recommended. */
#define NOVCOLLIDE 1

/* Whether making a mistake will take you back to the last place in
 * the word that matches what you have just typed (rather than right
 * back to the beginning) */
/* broken since 2.0.0 ! */
#define CONTEXT 0

/* The duration of a half-level in time units. orig:600 */
#define LEVELDUR 600



/* The following constants are configuration variables */

/* The file from which non-bonus words are read */
#define WORDFILE "/tmp/xletters-words.utf8"

/* The file from which input-conversions are read */
#define ICONVFILE "/tmp/inputconv.txt"
/*#define ICONVFILE "/usr/local/share/xletters/inputconv.txt"*/

/* The high score file */
/* if you change this, you should probably also modify Makefile.am */
#define SCOREFILE "/var/local/games/lib/xletters/scores_jp-en"

/* Number of high score entries */
#define SCORELEN 20

/* Whether a single user is allowed to have several entries in the
 * high score table */
#define MULTENT 1


/* The following constants are default values for game resources */

/* Show kana next to word ? Sum desired behaviours to get parameter value:
 *  1 show typed part of furigana, in the lower part of the screen
 *  2 show typed part of furigana, always
 *  4 show untyped part of furigana, in the lower part of the screen
 *  8 show untyped part of furigana, always
 * 16 pick one furigana transcription at the beginning (i.e. 
 *    to destroy word, only this transcription will work). This implies +8 
 *    to avoid frustration. */
#define SHOWKANA 6

/* "lower part of the screen" in the above means y>SHOWKANA_Y */
#define SHOWKANA_Y (HEIGHT/2)

/* Show comment/translation next to word ? Possible values are:
 *  0 don't show comment/translation
 *  1 show comment/translation, in the lower part of the screen
 *  2 show comment/translation, always */
#define SHOWCOMMENT 2

/* The font to use, if not overriden by a resource specification */
#define FONTNAME "-*-*-*-*-*-ja-18-*-*-*-*-*-iso10646-*"

/* The color of the words, if not overriden by a resource
 * specification */
#define WORD_COLOR "Blue"

/* The color of the letters correctly typed, if not overriden by a
 * resource specification */
#define TYPED_COLOR "Red"

/* Same as previous, for the kana transcription */
#define FONTNAME "-*-*-*-*-*-ja-18-*-*-*-*-*-iso10646-*"
#define KANA_COLOR "CornflowerBlue"
#define KANATYPED_COLOR "Salmon"

/* And one more set of prefs for the translation */
#define COMMENTFONTNAME "-*-fixed-medium-r-normal-*-15-*-*-*-*-*-iso8859-1"
#define COMMENT_COLOR "LimeGreen"

#define KANJI_FREQ 0 /* limit to the ... most frequent kanji */

/* It is safe to leave the following untouched */

/* The base time unit, in milliseconds - you probably shoudln't change
 * this. */
#define BASETIME 75

/* The application class - you probably shouldn't change this. */
#define APP_CLASS "XLetters"

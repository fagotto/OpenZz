/* 
    Zz Dynamic Parser Library
    Copyright (C) 1989 - I.N.F.N - S.Cabasino, P.S.Paolucci, G.M.Todesco

    The Zz Library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    The Zz Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __ZLEX_H__
#define __ZLEX_H__

#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <printf.h>
#include <string.h>
#include <stdlib.h>

/*-----------------------------------------------------------------------------*/

struct s_content
{
    struct s_tag *tag;
    //long value;
    union {
        long      lvalue;
        int32_t   ivalue;
        float     fvalue;
        void*     pvalue;
        char*     svalue;
        int64_t   llvalue;
        double    dvalue;
    } val;
};

#define s_content_lvalue(S)  ((S).val.lvalue)
#define s_content_ivalue(S)  ((S).val.ivalue)
#define s_content_fvalue(S)  ((S).val.fvalue)
#define s_content_pvalue(S)  ((S).val.pvalue)
#define s_content_svalue(S)  ((S).val.svalue)
#define s_content_llvalue(S) ((S).val.llvalue)
#define s_content_dvalue(S)  ((S).val.dvalue)

/* for compatibility */
#define s_content_value(S)   s_content_llvalue(S)
//#define s_content_value(S)   s_content_ivalue(S)

/* access to object "class" */
#define s_content_tag(S)     ((S).tag)

/*-----------------------------------------------------------------------------*/

#include "zz.h"

typedef struct s_content *(*s_tag_cast_fun)(struct s_content* src, struct s_tag* tgt_type_tag, struct s_content* tgt);

struct s_tag {
  char *name;
  int (*sprint)(char *, struct s_content *);
  int (*fprint)(FILE *, struct s_content *);
  int (*copy)(struct s_content *tgt, struct s_content *src);
  int (*len)(struct s_content *);
  int (*delete)(struct s_content *);
  int (*param_on)(struct s_content *, const char*);
  int (*param_off)(struct s_content *, const char*);
  s_tag_cast_fun cast;
};

#ifdef ZLEX
int init_zlex_done=0;
#else
extern int init_zlex_done;
#endif

#define INIT_ZLEX {if(!init_zlex_done)init_zlex();}

// Definisce puntatori globali di tipo s_tag
#ifdef ZLEX
struct s_tag 
#else
extern struct s_tag 
#endif

/* source tokens */
 *tag_t,
 *tag_int,		/* numero intero (decimale)	*/
 *tag_hex,	        /* numero intero (esadecimale)	*/
 *tag_float, 		/* numero float			*/
 *tag_double, 		/* numero double		*/
 *tag_int64, 		/* 64bit integer number		*/
 *tag_qstring,		/* stringa fra virgolette	*/
 *tag_ident,		/* identificatore		*/
 *tag_eol,		/* fine linea			*/
 *tag_eof,		/* fine file			*/ 
 *tag_char,		/* carattere singolo diverso da 
	 			["0-9A-Za-AEOLEOF] 	*/

/* private tokens */

 *tag_none,		/* token vuoto  		*/	
 *tag_address,		/* puntatore generico 		*/
 *tag_procedure,	/* puntatore a funzione		*/
 *tag_qprocedure,	/* puntatore a funzione	(quoted)*/
 *tag_list,		/* lista			*/


/* zz-specific tokens */

 *tag_cont,		/* '...'			*/
 *tag_special,		/* codice per match-ident ecc.	*/
 *tag_sint,		/* non-terminale		*/
 *tag_param,		/* parametro			*/
 *tag_bead		/* bead				*/
 
;

// forward declarations

int sprint_string(char *buffer,struct s_content *cnt);
int fprint_string(FILE *chan,struct s_content *cnt);

int sprint_int(char *buffer,struct s_content *cnt);
int fprint_int(FILE *chan,struct s_content *cnt);

int sprint_int64(char *buffer,struct s_content *cnt);
int fprint_int64(FILE *chan,struct s_content *cnt);

int sprint_hex(char *buffer,struct s_content *cnt);
int fprint_hex(FILE *chan,struct s_content *cnt);

int sprint_float(char *buffer,struct s_content *cnt);
int fprint_float(FILE *chan,struct s_content *cnt);

int sprint_double(char *buffer,struct s_content *cnt);
int fprint_double(FILE *chan,struct s_content *cnt);

int sprint_eol(char *buffer,struct s_content *cnt);
int fprint_eol(FILE *chan,struct s_content *cnt);

int sprint_eof(char *buffer,struct s_content *cnt);
int fprint_eof(FILE *chan,struct s_content *cnt);

int sprint_none(char *buffer,struct s_content *cnt);
int fprint_none(FILE *chan,struct s_content *cnt);

int init_zlex();

int zlex_set_precedence(struct s_content *cnt,int prec,int left_assoc);

char *zlex_strsave(const char *name);
int std_len(struct s_content *cnt);
int std_sprint(char *buffer,struct s_content *cnt);
int std_fprint(FILE *chan,struct s_content *cnt);
int std_copy(struct s_content *tar,struct s_content *src);

struct s_tag* find_tag(const char* name);

//char *content_image();
void zlex(char **ptr, struct s_content *cnt);
zz_ret zlex_set_case_sensitive(int boolean);
zz_ret zlex_set_parse_eol(int boolean);
zz_ret zlex_set_default_real_as_double(int boolean);
zz_ret zlex_set_default_integer_as_int64(int boolean);
int show_zlex_memory();

struct s_tag *find_tag(const char *name);

/*-----------------------------------------------------------------------------*/

#endif /*__ZLEX_H__*/


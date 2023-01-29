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

#ifndef __KERNEL_H__
#define __KERNEL_H__

#include "zz.h"

#define OPEN(S) {INIT_ZLEX open_rule(zlex_strsave( #S ));}
#define GSB(S)  append_nt_bead(zlex_strsave( #S),0);
#define M(S)    {struct s_content cnt;cnt.tag=tag_qstring;\
		s_content_svalue(cnt)=(char*)S;append_t_bead(&cnt);}
#define M_EOF   {struct s_content cnt;cnt.tag=tag_eof;\
		s_content_value(cnt)=0;append_t_bead(&cnt);}
#define END     insert_rule(0,close_rule());

/*
#define ACTION(A) {int A();k_action(A);}
#define RETURN_PROCEDURE(A) {int A();k_return_procedure(A);}
#define RETURN_STRING(S) k_return_string(S);
*/

#define RETURN_IDENT(S) {struct s_content tmp;char *s=S;zlex(&s,&tmp);\
			  setaction_return(&tmp,tmp.tag->name);}

#define PASS setaction_pass();
#define LIST setaction_list();
#define APPEND setaction_append();
#define MERGE setaction_merge();
#define MERGE_ALL setaction_merge_all();

// Usano argc,argv[],ret come parametri. ret ritorna il risultato (se applicabile).
// Il valore di ritorno della funzione chiamata non viene considerato quindi più corretttamente sarebbero funzioni di tipo void.

#define PROC(P)   {int P(); setaction_exeproc(P,0);}
#define FUN(T,P)  {int P(); setaction_exeproc(P,T);}

// Chiamata che passa uno o più parametri ma non ritorna un valore (tipicamente per le rules /stat-> ...).
// tag_none serve per segnalare questa eventualità.
#define SPROC(P)  {zz_ret P(); setaction_exesproc(P,tag_none);}

// P() ritorna un valore che viene assegnato al non terminale che apre la regola (es. /qstring -> ...)
// zz_ret è il typedef che viene definito dalla piattaforma per accomodare un valore di ritorno che possa
// essere poi inserito in s_content.value a prescindere dal suo tipo.
// zz_ret viene definito con l'int di lunghezza in bit uguale alle dimensione di un puntatore.
// Su MacOS è un long
// Le funzioni devono avere come valore di ritorno uno zz_ret e effettuare un cast (zz_ret) sul return per evitare warnings

#define SFUN(T,P) {zz_ret P(); setaction_exesproc(P,T);}

int kernel();
int zkernel();

#endif // __KERNEL_H__

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

#ifndef __SYS_H__
#define __SYS_H__

#include "list.h"

int zz_int64tohex(int argc, struct s_content argv[], struct s_content* ret);
int zz_doubletofloat(int argc, struct s_content argv[], struct s_content* ret);
int s_do_while_loops(int argc,struct s_content argv[],struct s_content *ret,int while_loop);
int change_extension(char *fullfilename,const char *filetype);
void get_extension(char *fullfilename, char *filetype);

zz_ret zz_qtoi(char* q);
zz_ret zz_inttohex(int i);
zz_ret zz_inttostring(int i);
zz_ret strcat_filename();
zz_ret subtag(char *,char *);
zz_ret s_trace(int trace);
zz_ret s_print(struct s_list *list);
zz_ret s_error(struct s_list *list);
zz_ret s_getenv(char *name);

int proc_beep(int argc, struct s_content argv[], struct s_content* ret);
int proc_beep_reset(int argc, struct s_content argv[], struct s_content* ret);

#endif // __SYS_H__

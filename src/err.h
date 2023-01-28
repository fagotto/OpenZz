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

#ifndef __OPENZZ_ERR_H__
#define __OPENZZ_ERR_H__

#define INFO	0
#define WARNING	1
#define ERROR   2
#define FATAL_ERROR   3
#define LEXICAL_ERROR   4
#define INTERNAL_ERROR   5

#include "zlex.h"

int zz_error(long type, char *fmt,...);
int error_head(int type);
int error_token(struct s_content *cnt);
int error_tail();
int error_tail_1();
int open_err_file();
int errprintf(char *fmt,...);
int get_error_number();
int syntax_error(void (*info_routine)());
int check_error_max_number();

//int set_max_error_n(int argc, struct s_content argv[], struct s_content *ret);
// int set_max_error_n(int argc,struct s_content argv[],int *ret);
int zz_get_error_number();

#endif /* __ERR_H__ */

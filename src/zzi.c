/* 
    Zz Dynamic Parser Library
    Copyright (C) 1989 - I.N.F.N - S.Cabasino, P.S.Paolucci, G.M.Todesco

    As opposed to the majority of the files in the Zz library,
    this file is released under the GPL.

    This file(zzi.c) is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2, or
    (at your option) any later version.

    This file and the Zz library are distributed in the hope that they
    will be useful, but WITHOUT ANY WARRANTY; without even the implied 
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU Lesser General Public License for more details.

    The GNU General Public License is available online or
    can be requested from the Free Software Foundation,
    59 Temple Place, Suite 330, Boston, MA 02111 USA. 
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include "zz.h"
#include "zlex.h"
#include "trace.h"
#include "source.h"
#include "parse.h"
#include "rule.h"
#include "err.h"

void next_token_tt(struct s_source *);

//  find_nt(); //table.c

/*--------------------------------------------------------------------*/

/**
 * Initialization function for using zz with interactive tty interface:
 */
int source_tt()
{
  struct s_source *src = new_source(next_token_tt);

  src->type = SOURCE_TT;
  src->src.tt.prompt = zz_get_prompt();
  src->src.tt.old = src->src.tt.s = 0;
  src->src.tt.row[0]='\0';

  return 1;
}


/*--------------------------------------------------------------------*/



int zz_parse_tt()
{
  /* really necessary... hope it doesn't come first in the calling sequence..
     if(!zz_chanout)
     zz_set_output(0);
  */
  source_tt();

  return parse(find_nt("root"));
}




/*---------------------------------------------------------------------*/


void next_token_tt(cur_source)
     struct s_source *cur_source;
{
  static char *line_read = (char *)NULL;       // Tmp ptr for gnu libreadline
  char *s;

  if(!cur_source->src.tt.s) {                  /* NEED TO READ A NEW LINE OF DATA */
    zz_trace("reading new line...\n");
   
    cur_source->src.tt.prompt = ".. ";

    if(find_prompt_proc)
      (*find_prompt_proc)(&(cur_source->src.tt.prompt));

    s = cur_source->src.tt.row;
    /* Error reporting may inspect the current row before lexing begins. */
    cur_source->src.tt.row[0] = '\0';
    cur_source->src.tt.old = cur_source->src.tt.row;

#ifdef HAVE_READLINE
    line_read = readline(cur_source->src.tt.prompt);
#else
    /* fgets fallback uses one extra byte to detect oversized logical lines. */
    line_read = malloc(sizeof(cur_source->src.tt.row) + 1);
    if (!line_read) {
      zz_error(FATAL_ERROR, "Out of memory reading interactive input");
      exit(EXIT_FAILURE);
    }
    fputs(cur_source->src.tt.prompt, stdout);
    fflush(stdout);
    if (!fgets(line_read, sizeof(cur_source->src.tt.row) + 1, stdin)) {
      free(line_read);
      line_read = NULL;
    } else {
      size_t len = strlen(line_read);
      if (len && line_read[len - 1] == '\n')
        line_read[len - 1] = '\0';
      else if (len >= sizeof(cur_source->src.tt.row)) {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) { }
      }
    }
#endif
    if (line_read) {
      if (strlen(line_read) >= sizeof(cur_source->src.tt.row)) {
        /* Reject the whole line: never execute a silently truncated command. */
        zz_error(ERROR, "Interactive input line too long (maximum %d characters)",
                 (int)sizeof(cur_source->src.tt.row) - 1);
        line_read[0] = '\0';
      }
#ifdef HAVE_READLINE
      if (*line_read) add_history(line_read);
#endif
      strcpy(cur_source->src.tt.row, line_read);

      cur_source->line_n ++;

      cur_source->src.tt.old = cur_source->src.tt.s = cur_source->src.tt.row;
    
      zlex(&(cur_source->src.tt.s), &curToken);

      // Free the memory used in the temporary line buffer
      free (line_read);
      line_read = (char *)NULL;
    }
    else {                        /* NULL received from readline - signifies EOF */
      cur_source->eof = 1;
        curToken.tag = tag_eof;
    }
  }
  else  {                         /* HAVE DATA - DO NOT NEED TO READ A NEW LINE */
    cur_source->src.tt.old = cur_source->src.tt.s;
    zlex(&(cur_source->src.tt.s),&curToken);
  }

  if(curToken.tag == tag_eol) {
    cur_source->src.tt.s=0;
    zz_trace("tag_eol... s=0\n");
  }
}


/*--------------------------------------------------------------------*/

static char rcsid[] = "$Id: zzi.c,v 1.8 2002/06/03 11:06:13 kibun Exp $ ";

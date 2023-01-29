/* 
    Zz Dynamic Parser Library
    Copyright (C) 1989 - I.N.F.N - S.Cabasino, P.S.Paolucci, G.M.Todesco

    The Zz Library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This Zz Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*============================================================================
  SOURCE.C
  Defines routines for managing the reading of data from various sources.
  Note that interactive terminal (tty) control is done through another 
  file (zzi.c) because the functionality is usually only required for
  development/testing purposes and not in the general zz library.
=============================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "avl.h"
#include "zlex.h"
#include "list.h"
#include "trace.h"
#include "source.h"
#include "parse.h"
#include "param.h"
#include "rule.h"
#include "action.h"
#include "sys.h"

int (*find_prompt_proc)()=0;
int (*source_line_routine)()=0;

// Global data - current source(ptr to item in source stack) and current token
struct s_source *cur_source;
struct s_content curToken;
 
#define SOURCE_N 49 /*era 49 e prima ancora 20*/
#define BACK_N 20

// Source stack and stack top index
static struct s_source source_stack[SOURCE_N];
static int source_sp=0;

// 'Back' stack with stack top index
static struct s_content back[BACK_N];
static int back_n=0;

/* no dot here !!! */
static const char *in_ext = "zz";

/**
 * Declare and provide constructors for output channel
 * Default output is set to stdout
 */
FILE *zz_chanout=0;
// Following initializers work but are not needed because each source
// tests to make sure the output stream is valiid.
// static void zz_chanout_construct (void) __attribute__((constructor));
// static void zz_chanout_construct (void) { zz_chanout = stdout; }

// Prototypes:
void next_token_file(struct s_source *src);
void next_token_list(struct s_source *src);


//int get_extension(); //sys.c
//int change_extension(); //sys.c
//int action(); //action.c

/*---------------------------------------------------------------------*/

void zz_set_output(const char *filename)
{
  if(filename) {
    zz_chanout = fopen(filename,"w");

    if(!zz_chanout) {
      int __errno=errno;
      printf("zz: unable to open output file %s (%s)\n",filename, strerror(errno));
      zz_chanout=stdout;
    }
  }
  else /* code relies on this feauture... ugly!!! */
    zz_chanout=stdout;
}

void zz_set_output_stream(FILE *output_stream)
{
  zz_assert(output_stream);
  zz_chanout=output_stream;
}

/*--------------------------------------------------------------------*/


/**
 * new_source() - set's the global 'cur_source' pointer to point to
 *   to a new (initialized with default values) s_source object.
 *   Call this to creat a new source and then populate the fields of
 *   'cur_source' as appropriate.
 */
struct s_source *new_source(void (*next_token_function)(struct s_source *src))
{
  INIT_ZLEX /* For safety */
    
    if (!next_token_function) {
      printf("Internal error - null next_token_function passed to new_source()");
      exit(0);
    }

    if(source_sp>=SOURCE_N-1) {
      printf("internal error - new_source: too many sources\n");
      exit(1);
    }
  
  if(cur_source) {
    if(cur_source->type==SOURCE_LIST)
      cur_source->src.list.current_pos = get_list_pos(cur_source->src.list.list);

    cur_source->cur_token = curToken;
  }

  // Set the global cur_source pointer to the next free object on the source stack:
  cur_source = & source_stack [source_sp++];

  // Initialize default values in the new source
  cur_source->type = NO_SOURCE;
  cur_source->eof=0;
  cur_source->line_n = 0;

  // Assign appropriate token processing function:
  cur_source->read_next_token = next_token_function;

  return cur_source;
}


/*--------------------------------------------------------------------*/

int pop_source()
{
struct s_source *next;
if(!cur_source) return 0;
if(cur_source->type == SOURCE_FILE)
  {
   fclose(cur_source->src.file.chan);
   free(cur_source->src.file.filename);
  }
cur_source->type = NO_SOURCE;
source_sp--;
if(source_sp<=0) cur_source = 0;
else
  {
   cur_source = &source_stack[source_sp-1];
   if(cur_source->type==SOURCE_LIST)
     list_seek(cur_source->src.list.list,cur_source->src.list.current_pos);
      curToken = cur_source->cur_token;
  }
return 1;
}


/*--------------------------------------------------------------------*/

// Initialization function for file source:
int source_file(char *filename)
{
  FILE *chan;
  chan = fopen(filename,"r");

  if(!chan) 
    return 0;
  
  new_source(next_token_file);

  cur_source->type = SOURCE_FILE;
  cur_source->src.file.chan = chan;
  cur_source->src.file.filename = (char*)malloc(strlen(filename)+1);
  strcpy(cur_source->src.file.filename,filename);
  cur_source->src.file.old = cur_source->src.file.s = 0;
  cur_source->src.file.row[0]='\0';
  return 1;
}

/*--------------------------------------------------------------------*/


int source_pipe()
{
  new_source(next_token_file);

  cur_source->type = SOURCE_FILE;
  cur_source->src.file.chan = stdin;
  cur_source->src.file.filename = "stdin";
  cur_source->src.file.old = cur_source->src.file.s = 0;
  cur_source->src.file.row[0]='\0';
  return 1;
}


/*--------------------------------------------------------------------*/

int source_list(struct s_content *list)
{
  if(list->tag!=tag_list)
    {
      printf("Internal error - source_list; bad argument\n");
      exit(1);
    }

  new_source(next_token_list);

  cur_source->type = SOURCE_LIST;
  cur_source->src.list.list = list;
//  cur_source->src.list.id = id;
  list_seek(list,0);
  cur_source->line_n=1;
  return 1;
}


/*--------------------------------------------------------------------*/
/*
  blocca lo stream di input. un successivo next_token ritornera' EOF
*/
void stop_source()
{
  if(cur_source) 
    cur_source->eof = 1;
}


/*--------------------------------------------------------------------*/

/**
 * Read the next token
*/
int next_token(struct s_content *token)
{
  int status;

  if(back_n>0) {
    zz_trace("next_token back!!!\n");
    *token = back[--back_n];
    return 1;
  }

  if(!cur_source || cur_source->eof) {
    token->tag=tag_eof;
    return 0;
  }

  status = 1;

  while(status)
    {
      zz_trace(">>> status=%d\n", status);

      // Cause the next token to be processed from current source:
      cur_source->read_next_token(cur_source);

      switch(status)
	{
	case 1:
	  if(curToken.tag != tag_cont)
	    status=0;
	  else 
	    status=2;
	  break;
	case 2:
	  if(curToken.tag == tag_eol)
	    status=1;
	  else 
	    if(curToken.tag != tag_cont)
	      status=0;
	  //else if(curToken.tag==tag_eof) status=0;
	}
    } 

  *token = curToken;

  zz_trace("next_token '%z'\n", token); 

  return !cur_source->eof;
}

/*
  if(kill && curToken.tag!=tag_eol)
  {
  curToken.tag = tag_eof;
  if(cur_source->type==SOURCE_FILE) kill=0;
  return;      
  }
*/


/*--------------------------------------------------------------------*/


void next_token_file(struct s_source *src)
{
int i;
char *s,*t;
void *ret;
if(!cur_source->src.file.s) 
  {
    zz_trace("reading new line...\n");
   /* NEED TO READ A NEW LINE */
   cur_source->src.file.row[0]='\0';
   ret = fgets(cur_source->src.file.row,132,cur_source->src.file.chan);
   cur_source->eof = (ret==NULL);
   cur_source->line_n ++;
   s = t = cur_source->src.file.row;
   for(i=0;i<132 && *t && *t!='\n';i++) t++;
   if(*t=='\n') *t='\0';
   if(source_line_routine && source_sp==1)
     (*source_line_routine)(s,cur_source->line_n,cur_source->src.file.filename);
   cur_source->src.file.s = cur_source->src.file.old = s;
   if(cur_source->eof)
       curToken.tag = tag_eof;
   else
     zlex(&(cur_source->src.file.s),&curToken);
  }
else
  {
   /* DO NOT NEED TO READ NEW LINE */
   cur_source->src.file.old = cur_source->src.file.s;
   zlex(&(cur_source->src.file.s),&curToken);
  }
 if(curToken.tag == tag_eol)
   {
     cur_source->src.file.s=0;
     zz_trace("tag_eol... s=0\n");
   }
}


/*--------------------------------------------------------------------*/


void next_token_list(struct s_source *src)
{
struct s_content *tmp;
tmp = next_list_item(cur_source->src.list.list);
if(!tmp)
  {
      curToken.tag=tag_eol;
   s_content_value(curToken)=0;
   cur_source->eof=1;
  }
else
  {
   if(tmp->tag==tag_eol) 
     cur_source->line_n++;
      curToken = *tmp;
  }
}


/*---------------------------------------------------------------------*/


int zz_parse_pipe()
{
  int ret;

  if(!zz_chanout) 
    zz_set_output(0);

  if(!source_pipe()) {
    printf("zz: pipe not found\n");
    return 0;
  }

  ret = parse(find_nt("root"));

  pop_source();

  return ret;
}


/*--------------------------------------------------------------------*/


int back_token(struct s_content *token)
{
if(back_n>=BACK_N-1) return 0;
back[back_n++] = *token;
return 1;
}


/*--------------------------------------------------------------------*/


static void cur_list_row(struct s_content *lst,char row[],int *curpos)
{
char item[256],*t;
struct s_content *cnt;
int k,j,i,pos,len,flag,totlen;
pos = get_list_pos(lst);
i = pos-2;
len = 0;
flag=0;
while(i>=0)
  {
   list_seek(lst,i);
   cnt=next_list_item(lst);
   if(cnt->tag==tag_eol && i<pos-2) break;
   sprintz(item,"%z",cnt);
   len+=strlen(item)+1;
   if(len>60) {flag=1;break;}
   i--;
  }
i++;
row[0]='\0';
if(flag)
  strcpy(row,"... ");
list_seek(lst,i);
for(j=i;j<pos-1;j++)
  {
   cnt = next_list_item(lst);
   if(cnt)
     {
      sprintz(item,"%z ",cnt);
      strcat(row,item);
     }
  }  
k = totlen = strlen(row);
while(1)
  {
   cnt = next_list_item(lst);
   if(!cnt || cnt->tag==tag_eol || cnt->tag==tag_eof) break;
   sprintz(item,"%z ",cnt);
   if(strlen(item)+totlen>200) 
     {
      strcat(row,"...");
      break;
     }
   strcat(row,item);
//   totlen+strlen(item);
    totlen=totlen+strlen(item);
  }  
t = row+k;while(*t==' ' || *t=='\t')k++,t++;
*curpos = k;
list_seek(lst,pos);
}


/*--------------------------------------------------------------------*/

char *source_name(struct s_source *source)
{
char *s;
switch(source->type)
  {
  case SOURCE_FILE:
    s=source->src.file.filename;
    break;
  case SOURCE_TT:
    s="stdin";
    break;
  case SOURCE_LIST:
    s="ZZ-action";
    break;
  default:
    s="unknown source type";
  }
return s;
}


/*--------------------------------------------------------------------*/


int source_line(struct s_source *source)
{
  return source->line_n;
}


/*--------------------------------------------------------------------*/


zz_ret get_current_line()
{
struct s_source *source;
int sp;
sp = source_sp-1;
if(sp<0)
  {
   return -1;
  }
while(sp>0 && source_stack[sp].type==SOURCE_LIST)
  sp--;
if(sp<0)
  return -1;
source = &source_stack[sp];
return source->line_n;
}


/*--------------------------------------------------------------------*/

char *get_source_name()
{
  if(!cur_source) 
    return "NOSOURCE";
  else 
    return source_name(cur_source);
}


/*--------------------------------------------------------------------*/


int get_source_line()
{
  if(!cur_source) 
    return 0;
  else 
    return source_line(cur_source);
}

/*--------------------------------------------------------------------*/

void get_source_file(char *buffer)
{
int i;
struct s_source *source;
for(i=source_sp-1;i>=0;i--)
  {
   if(source_stack[i].type==SOURCE_FILE ||
      source_stack[i].type==SOURCE_TT) break;
  }
if(i<0)
  strcpy(buffer,"noname");
else
  {
   if(source_stack[i].type==SOURCE_FILE)
     strcpy(buffer,source_stack[i].src.file.filename);
   else
     strcpy(buffer,"stdin");
  }
}


/*---------------------------------------------------------------------*/


int zz_parse_file(const char* filename)
{
  int ret;
  char full[256],type[40];
  type[0] = '\0';

  if(!zz_chanout) 
    zz_set_output(0);

  strcpy(full,filename);

  get_extension(full,type);

  if(!type[0] && strcmp(full,"/dev/tty"))
    change_extension(full, in_ext);

  if(!source_file(full)) {
    printf("zz_parse_file(): file %s not found\n",full);
    return 0;
  }

  ret = parse(find_nt("root"));
  pop_source();

  return ret;
}


/*--------------------------------------------------------------------*/


int fprint_source_position(FILE *chan,int print_action_flag)
{
struct s_source *source; 
char *t;
char row[256];
char *prompt = "| ";
int i,errpos,sp,flag;

sp = source_sp-1;
if(sp<0)
  {
   fprintf(chan,"%sno active input stream\n",prompt);
   return 0;
  }
if(!print_action_flag)
  {
   while(sp>0 && source_stack[sp].type==SOURCE_LIST)
     sp--;
   if(sp<0)
     {
      fprintf(chan,"%sno file input stream\n",prompt);
      return 0;
    }
  }

flag=1;
while(sp>=0)
  {
    if(flag==0) fprintf(chan,"%scalled by:\n",prompt);
    flag=0;
    source = &source_stack[sp--];
    switch(source->type)
      {
       case SOURCE_TT:
         strcpy(row,source->src.tt.row);
	 t = source->src.tt.old;while(*t==' ' || *t=='\t')t++;
	 errpos = t-source->src.tt.row;
	 break;
    
       case SOURCE_FILE:
	 strcpy(row,source->src.file.row);
	 for(i=0;row[i] && row[i]!='\n';i++)
         ;
     row[i]='\0';
	 t = source->src.file.old;while(*t==' ' || *t=='\t')t++;
	 errpos = t-source->src.file.row;
	 break;
    
       case SOURCE_LIST:
	 cur_list_row(source->src.list.list,row,&errpos);     
	 break;
    
       default:
	 row[0]='\0';
	 errpos= -1;
      }
    if(errpos>=0)
      {
       if(source->eof)
         fprintf(chan,"%sEND OF FILE\n",prompt);
       else
	 {
	  for(i=0;row[i];i++)
	    if(row[i]=='\t')
	      row[i]=' ';
	  fprintf(chan,"%s%s\n%s",prompt,row,prompt);
	  for(i=0;i<errpos;i++) fprintf(chan," ");
	  fprintf(chan,"^\n");
	 }
      }
    fprintf(chan,"%sline %d of %s\n",
	    prompt,source->line_n,source_name(source));
/*    if(source->type==SOURCE_TT || source->type==SOURCE_FILE) break; */
  }
return 1;
}


/*--------------------------------------------------------------------*/

/*
set_cont_prompt()
{
  if(cur_source->type!=SOURCE_TT)
    return;

  cur_source->src.tt.prompt = ".. ";
}
*/

/*--------------------------------------------------------------------*/


int pretend_eof()
{
if(cur_source)
    cur_source->eof=1;
return 1;
}


/*--------------------------------------------------------------------*/

zz_ret read_once_only(char *id)
{
struct node {char *id;} *p; 
static TREE *tree=0;
if(!tree) tree=avl_tree_nodup_str(struct node,id);
p=avl_locate(tree,id);
if(!p)
  {
   p=(struct node*)malloc(sizeof(struct node));
   p->id = (char*)malloc(strlen(id)+1);
   strcpy(p->id,id);
   avl_insert(tree,p);
  }
else
  {
   if(cur_source) cur_source->eof=1;
  }

return 1;
}

/*---------------------------------------------------------------------*/

void zz_set_default_extension(const char *ext)
{
  in_ext = ext;
}


/*---------------------------------------------------------------------*/


static char rcsid[] = "$Id: source.c,v 1.15 2002/05/09 17:23:49 kibun Exp $ ";

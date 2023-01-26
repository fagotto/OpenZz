//
// Created by Matteo Fago on 20/01/23.
//

#ifndef ZZ_PARSE_H__
#define ZZ_PARSE_H__

#include "rule.h"

extern struct s_nt *nt_any,*nt_param,*nt_gparam;

void make_closure();
//void lr_add_nt(struct s_nt_tran *tran, void *z);
void lr_add_nt(void *p, void *z);
void try_shift(int set_index);
int check_shift(int set_index);
int try_reduce(int set_index,struct s_rule *rule);
int check_reduce();
int lr_loop();
void lr_reduce();
int recovery();
static int print_expected();
int set_recovery(char *ntname,char *termlist);
int set_nt_prompt(char *ntname,const char *prompt);
int dump_dot(struct s_dot *dot,int off);
void dump_stack();
void dump_set(int set_index);
void write_dot_stat();
int print_report();
void fprint_param(FILE *chan);
int parse(struct s_nt *start_nt);


#endif //ZZ_PARSE_H__

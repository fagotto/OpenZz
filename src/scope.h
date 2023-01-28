//
// Created by Matteo Fago on 22/01/23.
//

#ifndef OPENZZ_1_0_4_SCOPE_H
#define OPENZZ_1_0_4_SCOPE_H

struct s_rule;

// Sap: int push_rule(),pop_rule();
//int push_rule(struct s_rule *rule);
void push_rule(void *,void *);
void pop_rule(void *,void *);
void do_list_rule(void *,void *);
int delete_scope(char *);

int zz_push_scope(char *);
int zz_pop_scope();
int delete_and_push_scope(char *);

int insert_rule(char *scope_name,struct s_rule *rule);
int do_list_rules(char *sintname,int kflag);
int list_all_rules();
int list_all_krules();
int list_rules(char*s);
int list_krules(char*s);
int write_rules(char *filename);

#endif //OPENZZ_1_0_4_SCOPE_H

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

zz_ret delete_scope(char *);
zz_ret zz_push_scope(char *);
zz_ret zz_pop_scope();
zz_ret delete_and_push_scope(char *);

zz_ret list_rules(char*s);
zz_ret list_all_rules();
zz_ret list_all_krules();
zz_ret list_krules(char*s);
zz_ret write_rules(char *filename);

zz_ret do_list_rules(char *sintname,int kflag);

int insert_rule(char *scope_name,struct s_rule *rule);


#endif //OPENZZ_1_0_4_SCOPE_H

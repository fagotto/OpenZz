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
int delete_scope(char *name);

int insert_rule(char *scope_name,struct s_rule *rule);

#endif //OPENZZ_1_0_4_SCOPE_H

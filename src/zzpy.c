/* Native ZZ Python preprocessor. Grammar files are trusted executable ZZ. */
#include "zz.h"
#include "zzbind.h"
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>

#define LIMIT (2u * 1024u * 1024u)
#define INPUT_LIMIT 32768u
#define DEPTH 32
static FILE *output;
static const char *source_name;
static unsigned source_line;
static unsigned token_count;
static size_t allocated;
struct allocation { void *p; struct allocation *next; };
static struct allocation *arena;
struct buffer { char *s; size_t n; };
static void die(const char *s) {
    fprintf(stderr, "zzpy: %s:%u: %s\n", source_name ? source_name : "<grammar>", source_line, s);
    exit(1);
}
static void *keep(size_t n) {
    struct allocation *a;
    if (n > LIMIT || allocated > LIMIT - n) die("fragment memory limit exceeded");
    a = malloc(sizeof(*a));
    if (!a || !(a->p = malloc(n))) die("out of memory");
    allocated += n; a->next = arena; arena = a; return a->p;
}
static void add(struct buffer *b, const char *s) {
    size_t n = strlen(s); char *p;
    if (n > LIMIT || b->n > LIMIT - n) die("normalized source limit exceeded");
    p = realloc(b->s, b->n + n + 1); if (!p) die("out of memory");
    b->s = p; memcpy(p + b->n, s, n + 1); b->n += n;
}
static void ch(struct buffer *b, char c) { char s[2] = {c, 0}; add(b, s); }
static char *join(const char *a, const char *b, const char *c) {
    size_t n = strlen(a) + strlen(b) + strlen(c) + 1;
    char *s = keep(n); snprintf(s, n, "%s%s%s", a, b, c); return s;
}
static const char *value(struct s_content *a, int i) { return zz_scnt_getv_svalue(a, i); }
static int cat(int n, struct s_content a[], struct s_content *r) {
    (void)n; zz_scnt_set_svalue(r, join(value(a,0),value(a,1),value(a,2))); return 1;
}
static int line(int n, struct s_content a[], struct s_content *r) {
    (void)n; zz_scnt_set_svalue(r, join(value(a,0),"\n","")); return 1;
}
static int assign(int n, struct s_content a[], struct s_content *r) {
    const char *p=value(a,0); unsigned depth=0; char quote=0; int escape=0;
    (void)n;
    if (!strcmp(p,"True") || !strcmp(p,"False") || !strcmp(p,"None")) die("cannot assign to a Python constant");
    if (!isalpha((unsigned char)*p) && *p!='_') die("invalid assignment target");
    while (isalnum((unsigned char)*p) || *p=='_') p++;
    if (*p && *p!='[') die("unsupported assignment target");
    for (;*p;p++) {
        if (quote) { if (!escape && *p==quote) quote=0; if (!escape && *p=='\\') escape=1; else escape=0; }
        else if (*p=='\'' || *p=='"') quote=*p;
        else if (*p=='[') depth++;
        else if (*p==']') { if (!depth || (!--depth && p[1])) die("unsupported assignment target"); }
    }
    if (depth || quote) die("invalid assignment target");
    zz_scnt_set_svalue(r,join(join(value(a,0)," = ",value(a,1)),"\n","")); return 1;
}
static int sequence(int n, struct s_content a[], struct s_content *r) {
    (void)n; zz_scnt_set_svalue(r, join(value(a,0),value(a,1),"")); return 1;
}
static int compound(int n, struct s_content a[], struct s_content *r) {
    struct buffer b = {0}; const char *p = value(a,1); int beginning = 1;
    (void)n; add(&b,value(a,0)); add(&b,"\n");
    while (*p) { if (beginning) add(&b,"    "); beginning = *p == '\n'; ch(&b,*p++); }
    zz_scnt_set_svalue(r,join(b.s,"","")); free(b.s); return 1;
}
static int publish(int n, struct s_content a[], struct s_content *r) {
    (void)n; (void)r;
    if (fputs(value(a,0),output) == EOF || ftell(output) > (long)LIMIT) die("output limit or write failure");
    return 1;
}
static int atom_name(int n, struct s_content a[], struct s_content *r) {
    static const char *reserved[] = {"as","assert","async","await","break","class","continue","def","del","elif","else","except","finally","for","from","global","import","in","is","lambda","nonlocal","raise","return","try","with","yield",NULL};
    const char **word, *name=value(a,0); (void)n;
    for (word=reserved;*word;word++) if (!strcmp(name,*word)) die("unsupported Python keyword");
    zz_scnt_set_svalue(r,join(name,"","")); return 1;
}
static int number(int n, struct s_content a[], struct s_content *r) {
    char s[32]; (void)n; snprintf(s,sizeof s,"%d",zz_scnt_getv_ivalue(a,0));
    zz_scnt_set_svalue(r,join(s,"","")); return 1;
}
static void bind(const char *nt,const char *word,const char *a,const char *b,const char *c,
                 zz_fun fn,const char *tag) {
    zz_bind_open(nt); if (word) zz_bind_keyword(word);
    if (a) zz_bind_match(a); if (b) zz_bind_match(b); if (c) zz_bind_match(c);
    if (tag) zz_bind_call_exe_proc(fn,tag); else zz_bind_call_exe_no_tag(fn); zz_bind_close();
}
static int parse(const char *s) {
    return zz_parse_string(s) && !zz_get_error_number();
}
#include "zzpy_base.h"
static void initialize(void) {
    zz_init(); zz_set_output_stream(stderr);
    zz_lex_add_new_tag2("pyblock",NULL,NULL);
    bind("qstring","pycat","string_t","string_t","string_t",cat,"qstring");
    bind("pyblock","pyassign","qstring","qstring",NULL,assign,"pyblock");
    bind("pyblock","pyline","qstring",NULL,NULL,line,"pyblock");
    bind("pyblock","pysequence","pyblock","pyblock",NULL,sequence,"pyblock");
    bind("pyblock","pycompound","qstring","pyblock",NULL,compound,"pyblock");
    bind("stat","pypublish","pyblock",NULL,NULL,publish,NULL);
    bind("p_atom",NULL,"ident",NULL,NULL,atom_name,"qstring");
    bind("p_atom",NULL,"int",NULL,NULL,number,"qstring");
    if (!parse(zzpy_base)) die("invalid base grammar");
}
static void quoted(struct buffer *b,const char *s,size_t n) {
    size_t i; ch(b,'"');
    for (i=0;i<n;i++) {
        if (s[i]=='"' || s[i]=='\\') ch(b,'\\');
        if (s[i]=='\n') add(b,"\\n"); else ch(b,s[i]);
    }
    ch(b,'"');
}
/* Lexical normalization only: grammar and extension dispatch live in ZZ. */
static void tokens(struct buffer *b,const char *p,int importing) {
    unsigned parens=0;
    while (*p && *p!='#') {
        const char *start=p;
        if (*p==' ' || *p=='\r') { p++; continue; }
        if (++token_count > 4096) die("source token limit exceeded");
        if (*p=='\t') die("tabs are not supported");
        if (*p=='\'' || *p=='"') {
            char quote=*p++; int escaped=0;
            while (*p) {
                if ((unsigned char)*p < 32) die("control character in string");
                if (!escaped && *p==quote) break;
                if (escaped && !strchr("\\\"'nrtbfav",*p)) die("unsupported string escape");
                if (!escaped && *p=='\\') escaped=1; else escaped=0;
                p++;
            }
            if (!*p) die("unterminated string"); p++;
            if ((size_t)(p-start)>400) die("string literal limit exceeded");
            if (!importing) add(b,"__string ");
            if (importing) { if (memchr(start+1,'\\',(size_t)(p-start-2))) die("escaped import paths not supported"); quoted(b,start+1,(size_t)(p-start-2)); }
            else quoted(b,start,(size_t)(p-start));
        } else if (isdigit((unsigned char)*p)) {
            while (isdigit((unsigned char)*p)) p++;
            if (p-start>100 || (p-start>1 && *start=='0')) die("invalid or overlong decimal integer");
            add(b,"__number "); quoted(b,start,(size_t)(p-start));
        } else if (isalpha((unsigned char)*p) || *p=='_') {
            while (isalnum((unsigned char)*p) || *p=='_') p++;
            if (p-start>100 || (p-start>=2 && start[0]=='_' && start[1]=='_')) die("reserved or overlong identifier");
            while (start<p) { if ((unsigned char)*start>=128) die("ASCII identifiers required"); ch(b,*start++); }
        } else if (p[0]=='/' && p[1]=='/') { add(b,"__floordiv"); p+=2;
        } else {
            if (*p=='(' || *p=='[') { if (++parens>DEPTH) die("expression nesting limit exceeded"); }
            if (*p==')' || *p==']') { if (!parens) die("unmatched closing delimiter"); parens--; }
            if (!strchr("+-*/%<>=!()[]{}:,",*p)) die("unsupported character");
            ch(b,*p++);
            if ((*p=='=' && strchr("<>=!",p[-1])) || (*p=='/' && p[-1]=='/')) ch(b,*p++);
        }
        ch(b,' ');
    }
    if (parens) die("multiline expressions are not supported");
}
static void flush(struct buffer *b) {
    if (b->n && !parse(b->s)) die("ZZ rejected source or grammar (line denotes end of current unit)");
    free(b->s); b->s=NULL; b->n=0;
}
static char *read_source(const char *path) {
    FILE *f=fopen(path,"rb"); char *s; size_t n;
    if (!f) die("cannot open source"); s=malloc(INPUT_LIMIT+2); if (!s) die("out of memory");
    n=fread(s,1,INPUT_LIMIT+1,f); if (ferror(f)) die("source read failed"); fclose(f);
    if (n>INPUT_LIMIT || memchr(s,0,n)) die("source too large or contains NUL"); s[n]=0; return s;
}
static void translate(char *s) {
    struct buffer unit={0}; unsigned stack[DEPTH+1]={0},depth=0;
    char *p=s; int native=0, braces=0; char quote=0; int escaped=0;
    while (*p) {
        char *end=strchr(p,'\n'), *line_start=p, *t; unsigned indent=0;
        if (end) *end=0;
        source_line++;
        while (*p==' ') { indent++; p++; }
        if (native) {
            t=line_start;
        } else {
            if (!*p || *p=='#') goto next;
            if (*p=='\t') die("tabs are not supported");
            if (indent>stack[depth]) {
                if (!unit.n || depth==DEPTH) die("unexpected or excessive indentation");
                stack[++depth]=indent; add(&unit,"__IN ");
            } else {
                while (indent<stack[depth]) { depth--; add(&unit,"__OUT "); }
                if (indent!=stack[depth]) die("inconsistent indentation");
                if (!indent) flush(&unit);
            }
            if (!indent && !strncmp(p,"syntax zz {",11)) { native=1; t=p; }
            else {
                int importing=!indent && !strncmp(p,"import ",7);
                if (!indent && !importing) add(&unit,"__emit ");
                tokens(&unit,p,importing);
                add(&unit,importing ? ";\n" : "__NL ");
                if (importing) flush(&unit);
                goto next;
            }
        }
        /* Native lexical island: braces count outside quoted strings and !! comments. */
        for (;*t;t++) {
            if (!quote && t[0]=='!' && t[1]=='!') break;
            if (quote) { if (!escaped && *t==quote) quote=0; if (!escaped && *t=='\\') escaped=1; else escaped=0; }
            else if (*t=='"') quote=*t;
            else if (*t=='{') { if (++braces>DEPTH) die("native nesting limit exceeded"); }
            else if (*t=='}') { if (--braces<0) die("unbalanced native block"); }
            ch(&unit,*t);
        }
        add(&unit,"\n");
        if (!braces && !quote) { native=0; flush(&unit); }
next:
        if (!end) break; p=end+1;
    }
    if (native) die("unterminated native block");
    while (depth) { depth--; add(&unit,"__OUT "); }
    flush(&unit);
}
int main(int argc,char **argv) {
    const char *destination=NULL,*grammar=NULL; char *s; int i; FILE *target; char *temporary=NULL;
    for (i=1;i<argc;i++) {
        if (!strcmp(argv[i],"-o") && i+1<argc) destination=argv[++i];
        else if (!strcmp(argv[i],"--grammar") && i+1<argc) grammar=argv[++i];
        else if (argv[i][0]=='-' || source_name) { fprintf(stderr,"usage: zzpy [--grammar file.zz] [-o output.py] source.zzpy\n"); return 2; }
        else source_name=argv[i];
    }
    if (!source_name) die("source file required");
    if (destination) {
        struct stat a,b;
        if (!strcmp(source_name,destination) || (!stat(source_name,&a) && !stat(destination,&b) && a.st_dev==b.st_dev && a.st_ino==b.st_ino)) die("output must differ from source");
    }
    s=read_source(source_name); output=tmpfile(); if (!output) die("cannot create output stream");
    initialize();
    if (grammar && (!zz_parse_file(grammar) || zz_get_error_number())) die("cannot load grammar");
    translate(s); free(s);
    if (fflush(output) || fseek(output,0,SEEK_SET)) die("cannot rewind output");
    target=stdout;
    if (destination) {
        int fd; size_t n=strlen(destination)+12;
        temporary=malloc(n); if (!temporary) die("out of memory");
        snprintf(temporary,n,"%s.XXXXXX",destination); fd=mkstemp(temporary);
        if (fd<0 || !(target=fdopen(fd,"wb"))) die("cannot open output temporary");
    }
    while ((i=fgetc(output))!=EOF) if (fputc(i,target)==EOF) { if (temporary) unlink(temporary); die("output write failed"); }
    if (ferror(output) || fflush(target)) { if (temporary) unlink(temporary); die("output I/O failed"); }
    if (temporary) {
        if (fclose(target) || rename(temporary,destination)) { unlink(temporary); die("cannot publish output"); }
        free(temporary);
    }
    fclose(output);
    while (arena) { struct allocation *a=arena; arena=a->next; free(a->p); free(a); }
    return 0;
}

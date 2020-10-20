#if !defined(VAR_H)
#define VAR_H

#define DEFAULT_LEXICON_SIZE 4
#define DEFAULT_SCOPE_SIZE   4
#define LEXICON_NOTFOUND    -1


typedef struct __define_list_entry {
    char* name;
    char* lbl;
    int nargs;
} def_t;

typedef struct __define_list {
    int len;
    int cap;
    def_t* defs;
} deflist_t;

typedef struct __var {
    int l;
    int c;
    int off;
    char* name;
} var_t;

typedef struct __scope {
    int len;
    int cap;
    volatile int offc;
    var_t* vars;
} scope_t;

typedef struct __lexicon {
    int len;
    int cap;
    volatile int offc;
    scope_t* scopes;
    deflist_t* defs;
} lexicon_t;

lexicon_t* new_lexicon (lexicon_t* table);
void push_scope (lexicon_t* table);
int pop_scope (lexicon_t* table);
int bind (lexicon_t* table, int l, int c, char* name);
int lookup (lexicon_t* table, char* name);

char* add_definition (lexicon_t* table, char* name, char*(*gensym)(char*, char*), int numargs);
deflist_t* rem_deftable (lexicon_t* table);
char* reintroduce_def(lexicon_t* table, deflist_t* def, char* name);
char* get_definition (lexicon_t* table, char* name);
int get_args (lexicon_t* table, char* name);
int lexicon_offset(lexicon_t* table);
void push_scope_a (lexicon_t* table, scope_t* scope, scope_t* mal);

scope_t* scope_a ();
void scope_d (scope_t* s);
int sbind (scope_t* scope, char* name);
int slookup (scope_t* scope, char* name);
void unbind (scope_t* scope, scope_t* mal);
void print_table(lexicon_t* table);

void destroy_lexicon(lexicon_t* table);

#endif
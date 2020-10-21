#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "var.h"

void lexicon_grow (lexicon_t* table, int cap);
void scope_grow (scope_t* scope, int cap);
void deflist_grow (deflist_t* defs, int cap);
void new_scope (scope_t* scope);

lexicon_t* new_lexicon (lexicon_t* table) {
    table->len = 0;
    table->cap = 0;
    table->offc = 1; /* offsets are 1-indexed */
    table->scopes = NULL;
    table->defs = malloc(sizeof(deflist_t));
    table->defs->len = 0;
    table->defs->cap = 0;
    table->defs->defs = NULL;

    /* we actually do grow the lexicon first, for global */
    lexicon_grow(table, DEFAULT_LEXICON_SIZE);
    deflist_grow(table->defs, DEFAULT_SCOPE_SIZE);
    /* then add global scope */
    push_scope(table);
}

void lexicon_grow (lexicon_t* table, int cap) {
    scope_t* new_scopes;
    if (table->len == 0) {
        table->cap = cap;
        table->scopes = malloc(sizeof(scope_t) * cap);
    }
    else {
        cap = 2 * table->cap;
        new_scopes = malloc(sizeof(scope_t) * cap);
        memcpy((uint8_t*) new_scopes, (uint8_t*) table->scopes, sizeof(scope_t) * table->len);
        free(table->scopes);
        table->scopes = new_scopes;
        table->cap = cap;
    }
}

void destroy_lexicon(lexicon_t* table) {
    while (table->len) {
        pop_scope(table);
    }
    free(table->scopes);
}

void scope_grow (scope_t* scope, int cap) {
    var_t* new_vars;
    if (scope->len == 0) {
        scope->cap = cap;
        scope->vars = malloc(sizeof(var_t) * cap);
    }
    else {
        cap = 2 * scope->cap;
        new_vars = malloc(sizeof(var_t) * cap);
        memcpy((uint8_t*) new_vars, (uint8_t*) scope->vars, sizeof(var_t) * scope->len);
        free(scope->vars);
        scope->vars = new_vars;
        scope->cap = cap;
    }
}

void deflist_grow (deflist_t* defs, int cap) {
    def_t* new_defs;
    if (defs->len == 0) {
        defs->cap = cap;
        defs->defs = malloc(sizeof(def_t) * cap);
    }
    else {
        cap = 2 * defs->cap;
        new_defs = malloc(sizeof(def_t) * cap);
        memcpy((uint8_t*) new_defs, (uint8_t*) defs->defs, sizeof(def_t) * defs->len);
        free(defs->defs);
        defs->defs = new_defs;
        defs->cap = cap;
    }
}

void new_scope (scope_t* scope) {
    scope->len = 0;
    scope->cap = 0;
    scope->vars = NULL;

    /* grow scope init */
    scope_grow(scope, DEFAULT_SCOPE_SIZE);
}

void push_scope (lexicon_t* table) {
    scope_t scope;
    if (2 * table->len >= table->cap) {
        lexicon_grow(table, 0);
    }
    new_scope(&scope);
    scope_grow(&scope, DEFAULT_SCOPE_SIZE);
    scope.offc = table->offc;
    table->scopes[table->len++] = scope;
}

void push_scope_a (lexicon_t* table, scope_t* scope, scope_t* mal) {
    for (int i = 0; i < mal->len; i++) {
        sbind(scope, mal->vars[i].name);
    }

    table->offc = scope->offc;
}

void print_table(lexicon_t* table) {
    for (int i = 0; i < table->len; i++) {
        scope_t scope = table->scopes[i];
        for (int j = 0; j < scope.len; j++) {
            printf("%30s %8d\n", scope.vars[j].name, scope.vars[j].off);
        }
    }
}

int pop_scope (lexicon_t* table) {
    scope_t scope = table->scopes[table->len - 1];
    /* free the vars, not the strings */
    int rv = scope.len;
    free(scope.vars);
    /* reduce offc */
    table->offc -= scope.len;
    memset((uint8_t*) &((table->scopes)[table->len - 1]), 0, sizeof(scope_t));
    table->len--;
    return rv;
}

int bind (lexicon_t* table, int l, int c, char* name) {
    if (name == NULL) return -1;
    /* binds variable to most recent scope */
    scope_t* scope = &(table->scopes[table->len - 1]);
    if (2 * scope->len >= scope->cap) {
        scope_grow(scope, 0);
    }
    var_t var;
    var.l = l;
    var.c = c;
    var.off = scope->offc++;
    var.name = name;
    scope->vars[scope->len++] = var;
    table->offc = scope->offc;
    return var.off;
}

scope_t* scope_a () {
    scope_t* s = malloc(sizeof(scope_t));
    new_scope(s);
    s->offc = 1;
    return s;
}

void scope_d (scope_t* s) {
    free(s->vars);
    free(s);
}


int sbind (scope_t* scope, char* name) {
    if (name == NULL) return -1;
    if (2 * scope->len >= scope->cap) {
        scope_grow(scope, 0);
    }
    if (slookup(scope, name) != LEXICON_NOTFOUND) {
        return -1;
    }
    var_t var;
    var.off = (scope->offc)++;
    var.name = name;
    scope->vars[scope->len++] = var;
    return var.off;
}

int slookup (scope_t* scope, char* name) {
    if (name == NULL) return -1;
    int rv_ = LEXICON_NOTFOUND;
    for (int j = scope->len - 1; j >= 0; j--) {
        if (!strcmp(scope->vars[j].name, name)) {
            rv_ = scope->vars[j].off;
            break;
        }
    }
    return rv_;
}

void unbind (scope_t* scope, scope_t* mal) {
    for (int i = 0; i < scope->len; i++) {
        for (int j = 0; j < mal->len; j++) {
             if (!strcmp(scope->vars[i].name, mal->vars[j].name)) {
                 scope->vars[i].off = -1;
             }
        }
    }
    scope_t* better = scope_a();
    for (int i = 0; i < scope->len; i++) {
        if (scope->vars[i].off != -1) {
            sbind(better, scope->vars[i].name);
        }
    }
    free(scope->vars);
    scope->vars = better->vars;
    scope->len = better->len;
    scope->cap = better->cap;
    scope->offc = better->offc;
    free(better);
}

int lookup (lexicon_t* table, char* name) {
    if (name == NULL) return -1;
    scope_t scope;
    int rv_ = LEXICON_NOTFOUND;
    for (int i = table->len - 1; i >= 0; i--) {
        scope = table->scopes[i];
        for (int j = scope.len - 1; j >= 0; j--) {
            if (!strcmp(scope.vars[j].name, name)) {
                rv_ = scope.vars[j].off;
                break;
            }
        }
        if (rv_ != LEXICON_NOTFOUND) {
            break;
        }
    }
    return rv_;
}

char* add_definition (lexicon_t* table, char* name, char*(*gensym)(char*, char*), int numargs) {
    if (name == NULL) return NULL;
    def_t def;
    char* label = malloc(50);
    (*gensym)(label, "define_label");
    def.lbl = label;
    def.name = name;
    def.nargs = numargs;
    
    if (2 * table->defs->len >= table->defs->cap) {
        deflist_grow(table->defs, 0);
    }
    table->defs->defs[table->defs->len++] = def;
    return label;
}

deflist_t* rem_deftable (lexicon_t* table) {
    deflist_t* rv = table->defs;
    table->defs = malloc(sizeof(deflist_t));
    table->defs->len = 0;
    table->defs->cap = 0;
    table->defs->defs = NULL;
    deflist_grow(table->defs, DEFAULT_SCOPE_SIZE);
    return rv;
}


char* get_defraw (deflist_t* defs, char* name) {
    if (name == NULL) return NULL;
    char* rv_ = NULL;
    for (int i = 0; i < defs->len; i++) {
        if (!strcmp(defs->defs[i].name, name)) {
            rv_ = defs->defs[i].lbl;
            break;
        }
    }
    return rv_;
}

char* reintroduce_def(lexicon_t* table, deflist_t* defs, char* name) {
    if (name == NULL) return NULL;
    def_t def;
    char* label = get_defraw(defs, name);
    def.lbl = label;
    def.name = name;
    def.nargs = 0;
    
    if (2 * table->defs->len >= table->defs->cap) {
        deflist_grow(table->defs, 0);
    }
    table->defs->defs[table->defs->len++] = def;
    return label;
}

char* get_definition (lexicon_t* table, char* name) {
    if (name == NULL) return NULL;
    char* rv_ = NULL;
    for (int i = 0; i < table->defs->len; i++) {
        if (!strcmp(table->defs->defs[i].name, name)) {
            rv_ = table->defs->defs[i].lbl;
            break;
        }
    }
    return rv_;
}

int get_args (lexicon_t* table, char* name) {
    if (name == NULL) return -1;
    int rv_ = -1;
    for (int i = 0; i < table->defs->len; i++) {
        if (!strcmp(table->defs->defs[i].name, name)) {
            rv_ = table->defs->defs[i].nargs;
            break;
        }
    }
    return rv_;
}

int lexicon_offset (lexicon_t* table) {
    return table->offc;
}

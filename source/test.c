#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include "util.h"
#include "arch/x86_64/arch.h"

#define WLANG_TARGET_DARWIN 0
#define WLANG_TARGET_LINUX 1

#ifndef WLANG_TARGET
#  ifdef __APPLE__
#    define WLANG_TARGET WLANG_TARGET_DARWIN
#  else
#    define WLANG_TARGET WLANG_TARGET_LINUX
#  endif
#endif

#if WLANG_TARGET == WLANG_TARGET_DARWIN
#  define WLANG_SYSCALL_EXIT WLANG_SYSCALL__DARWIN__EXIT
#elif WLANG_TARGET == WLANG_TARGET_LINUX
#  define WLANG_SYSCALL_EXIT WLANG_SYSCALL__LINUX__EXIT
#endif

#define LIST(T) T##List
#define LISTFN(T, NAME) T##List_##NAME
#define DECLARE_LIST_TYPE(T) \
    typedef struct {\
        T* data; \
        size_t count; \
    } LIST(T); \
    void LISTFN(T, Initialize)(LIST(T)* self) \
    { self->count = 0; self->data = NULL; } \
    void LISTFN(T, _IncrSize)(LIST(T)* self) \
    { if(self->count == 0) self->data = malloc(sizeof(T) * (++self->count)); \
      else self->data = realloc(self->data, sizeof(T) * (++self->count)); } \
    T* LISTFN(T, PushValue)(LIST(T)* self, T value) \
    { LISTFN(T, _IncrSize)(self); self->data[self->count - 1] = value; return &self->data[self->count - 1]; } \
    T* LISTFN(T, PushRef)(LIST(T)* self, T* value) \
    { LISTFN(T, _IncrSize)(self); self->data[self->count - 1] = *value; return &self->data[self->count - 1]; } \
    void LISTFN(T, Free)(LIST(T)* self) \
    { free(self->data); self->count = 0; self->data = NULL; } \
    void LISTFN(T, ForEachRef)(LIST(T)* self, void (*func)(T*)) \
    { for(size_t i = 0; i < self->count; ++i) func(&self->data[i]); } \
    void LISTFN(T, ForEach)(LIST(T)* self, void (*func)(T)) \
    { for(size_t i = 0; i < self->count; ++i) func(self->data[i]); }

// TODO: This is not a Hash Map, just a list with a linear search!
#define HASHMAP(T) T##HashMap
#define HASHMAPFN(T, NAME) T##HashMap_##NAME
#define DECLARE_HASHMAP_TYPE(T, K) \
    typedef struct {\
        T* data; \
        size_t count; \
    } HASHMAP(T); \
    void HASHMAPFN(T, Initialize)(HASHMAP(T)* self) \
    { self->count = 0; self->data = NULL; } \
    void HASHMAPFN(T, _IncrSize)(HASHMAP(T)* self) \
    { if(self->count == 0) self->data = malloc(sizeof(T) * (++self->count)); \
      else self->data = realloc(self->data, sizeof(T) * (++self->count)); } \
    T* HASHMAPFN(T, PushValue)(HASHMAP(T)* self, T value) \
    { HASHMAPFN(T, _IncrSize)(self); self->data[self->count - 1] = value; return &self->data[self->count - 1]; } \
    T* HASHMAPFN(T, PushRef)(HASHMAP(T)* self, T* value) \
    { HASHMAPFN(T, _IncrSize)(self); self->data[self->count - 1] = *value; return &self->data[self->count - 1]; } \
    void HASHMAPFN(T, Free)(HASHMAP(T)* self) \
    { free(self->data); self->count = 0; self->data = NULL; } \
    void HASHMAPFN(T, ForEachRef)(HASHMAP(T)* self, void (*func)(T*)) \
    { for(size_t i = 0; i < self->count; ++i) func(&self->data[i]); } \
    void HASHMAPFN(T, ForEach)(HASHMAP(T)* self, void (*func)(T)) \
    { for(size_t i = 0; i < self->count; ++i) func(self->data[i]); } \
    T* HASHMAPFN(T, Find)(HASHMAP(T)* self, K key, bool (*cmp)(T*, K)) \
    { for(size_t i = 0; i < self->count; ++i) if(cmp(&self->data[i], key)) \
      return &self->data[i]; return NULL; }


typedef char* CStr;
void CStr_Free(char* s) { free(s); }
DECLARE_LIST_TYPE(CStr);

typedef struct {
    char *data;
    size_t size;
} DynamicString;


void DynamicString_Initialize(DynamicString* self)
{ self->data = NULL; self->size = 0; }

void DynamicString__IncrSize(DynamicString* self)
{
    if(self->size == 0) self->data = malloc(++self->size + 1);
    else self->data = realloc(self->data, ++self->size + 1);
    self->data[self->size] = '\0';
}
void DynamicString_Add(DynamicString* self, char value) { DynamicString__IncrSize(self); self->data[self->size - 1] = value; }
void DynamicString_Free(DynamicString* self) { free(self->data); self->size = 0; self->data = NULL; }
void DynamicString_CopyFrom(DynamicString* self, const char* source, size_t size) {
    self->size = size ? size : strlen(source);
    if(self->data != NULL) free(self->data);
    self->data = malloc(self->size + 1);
    memcpy(self->data, source, self->size + 1);
}


enum TokenType
{
    TokenType_Eof = -1,
    TokenType_Other,
    TokenType_Iden,
    TokenType_Int,
    TokenType_Float,
    TokenType_String,
    TokenType_DoubleEqual,
    TokenType_NotEqual,
    TokenType_LessEqual,
    TokenType_GreaterEqual,
    TokenType__Last
};

const char* TokenType_ToString(enum TokenType t)
{
    switch(t)
    {
    case TokenType_Other: return "Other";
    case TokenType_Iden: return "Identifier";
    case TokenType_Int: return "Integer Literal";
    case TokenType_Float: return "Float Literal";
    case TokenType_String: return "String Literal";
    default: return "<UNKNOWN>";
    }
}

typedef struct { int type; char* value; } Token;
DECLARE_LIST_TYPE(Token);

typedef struct { TokenList tokens; FILE* source; size_t current; } Lexer;

void Lexer_Initialize(Lexer* self, const char* filepath)
{
    self->source = fopen(filepath, "r");
    if(!self->source) perror("Failed to open file");
    TokenList_Initialize(&self->tokens);
    self->current = 0;
}
void Lexer_Free(Lexer* self)
{
    for(size_t i = 0; i < self->tokens.count; ++i)
    {
        Token* t = &self->tokens.data[i];
        if(t->type == TokenType_Iden) free(t->value);
    }
    TokenList_Free(&self->tokens);
    fclose(self->source);
}
void Lexer_LexFile(Lexer* l)
{
    while(fpeekc(l->source) != EOF)
    {
        while(isspace(fpeekc(l->source))) fgetc(l->source);
        if(fpeekc(l->source) == EOF) break;

        /**/ if(isalpha(fpeekc(l->source)) || fpeekc(l->source) == '_')
        {
            DynamicString s;
            DynamicString_Initialize(&s);

            while(isalnum(fpeekc(l->source)) || fpeekc(l->source) == '_')
                DynamicString_Add(&s, fgetc(l->source));

            TokenList_PushValue(&l->tokens, (Token){ TokenType_Iden, s.data });
        }
        else if(isdigit(fpeekc(l->source)))
        {
            DynamicString s;
            DynamicString_Initialize(&s);
            enum TokenType tt = TokenType_Int;

            while(isdigit(fpeekc(l->source)) || fpeekc(l->source) == '_')
                DynamicString_Add(&s, fgetc(l->source));

            if(fpeekc(l->source) == '.')
            {
                tt = TokenType_Float;
                fgetc(l->source);
                while(isdigit(fpeekc(l->source)) || fpeekc(l->source) == '_')
                    DynamicString_Add(&s, fgetc(l->source));
            }

            TokenList_PushValue(&l->tokens, (Token){ tt, s.data });
        }
        else if(fpeekc(l->source) == '=')
        {
            fgetc(l->source);
            if(fpeekc(l->source) == '=')
            {
                fgetc(l->source);
                TokenList_PushValue(&l->tokens, (Token){ TokenType_DoubleEqual, NULL });
            }
            else TokenList_PushValue(&l->tokens, (Token){ '=', NULL });
        }
        else if(fpeekc(l->source) == '>')
        {
            fgetc(l->source);
            if(fpeekc(l->source) == '=')
            {
                fgetc(l->source);
                TokenList_PushValue(&l->tokens, (Token){ TokenType_GreaterEqual, NULL });
            }
            else TokenList_PushValue(&l->tokens, (Token){ '=', NULL });
        }
        else if(fpeekc(l->source) == '<')
        {
            fgetc(l->source);
            if(fpeekc(l->source) == '=')
            {
                fgetc(l->source);
                TokenList_PushValue(&l->tokens, (Token){ TokenType_LessEqual, NULL });
            }
            else TokenList_PushValue(&l->tokens, (Token){ '=', NULL });
        }
        else if(fpeekc(l->source) == '!')
        {
            fgetc(l->source);
            if(fpeekc(l->source) == '=')
            {
                fgetc(l->source);
                TokenList_PushValue(&l->tokens, (Token){ TokenType_NotEqual, NULL });
            }
            else TokenList_PushValue(&l->tokens, (Token){ '=', NULL });
        }
        else TokenList_PushValue(&l->tokens, (Token){ fgetc(l->source), NULL });
    }
}

Token* Lexer_Next(Lexer* self)
{
    static Token eof_token = { TokenType_Eof, "<EOF>" };
    if(self->current >= self->tokens.count) return &eof_token;
    return &self->tokens.data[self->current++];
}

Token* Lexer_PeekN(Lexer* self, int n)
{
    static Token eof_token = { TokenType_Eof, "<EOF>" };
    if(self->current + n - 1 >= self->tokens.count) return &eof_token;
    return &self->tokens.data[self->current + n - 1];
}

Token* Lexer_Peek(Lexer* self) { return Lexer_PeekN(self, 1); }

enum NodeType
{
    NodeType_Error,
    NodeType_Proc,
    NodeType_Iden,
    NodeType_Int,
    NodeType_Float,
    NodeType_StringLit,
    NodeType_Block,
    NodeType_Return,
    NodeType_BinOp,
    NodeType_Decl,
};

const char* NodeType_ToString(enum NodeType t)
{
    switch(t)
    {
    case NodeType_Error: return "Error";
    case NodeType_Proc: return "Procedure";
    case NodeType_Iden: return "Identifier";
    case NodeType_Int: return "Integer Literal";
    case NodeType_Float: return "Float Literal";
    case NodeType_StringLit: return "String Literal";
    case NodeType_Block: return "Block";
    case NodeType_Return: return "Return";
    case NodeType_BinOp: return "Binary Operation";
    case NodeType_Decl: return "Variable Declaration";
    default: return "<UNKNOWN>";
    }
}

const char* NodeType_ToString2(enum NodeType t)
{
    switch(t)
    {
    case NodeType_Error: return "Error";
    case NodeType_Proc: return "Proc";
    case NodeType_Iden: return "Iden";
    case NodeType_Int: return "Int";
    case NodeType_Float: return "Float";
    case NodeType_StringLit: return "StringLit";
    case NodeType_Block: return "Block";
    case NodeType_Return: return "Return";
    case NodeType_BinOp: return "BinOp";
    case NodeType_Decl: return "Decl";
    default: return "<UNKNOWN>";
    }
}

typedef struct { enum NodeType type; } AstNode;
typedef AstNode* AstNodePtr;
DECLARE_LIST_TYPE(AstNode);
DECLARE_LIST_TYPE(AstNodePtr);

#define ASTNODE_ALLOC(VAR, T, TYPE) T* VAR = malloc(sizeof(T)); VAR->node.type = TYPE
#define ASTNODE_FREE(E) if(E) free(E)

void AstNode_FreeAny(AstNode* base);

typedef struct { AstNode node; char* name; CStrList args; AstNode* body; } AstNode_Proc;
void AstNode_Proc_Free(AstNode_Proc* n)
{
    free(n->name);
    CStrList_ForEach(&n->args, &CStr_Free);
    CStrList_Free(&n->args);
    AstNode_FreeAny(n->body);
    ASTNODE_FREE(n);
}
AstNode_Proc *AstNode_Proc_Create(char* name, CStrList* args, AstNode* body)
{
    ASTNODE_ALLOC(n, AstNode_Proc, NodeType_Proc);
    n->name = AllocStringCopy(name);
    n->args.data = args->data;
    n->args.count = args->count;
    n->body = body;
    return n;
}

typedef struct { AstNode node; char* iden; } AstNode_Iden;
void AstNode_Iden_Free(AstNode_Iden* n) { free(n->iden); ASTNODE_FREE(n); }
AstNode_Iden *AstNode_Iden_Create(char* iden)
{
    ASTNODE_ALLOC(n, AstNode_Iden, NodeType_Iden);
    n->iden = AllocStringCopy(iden);
    return n;
}

typedef struct { AstNode node; char* name; AstNode* value; } AstNode_Decl;
void AstNode_Decl_Free(AstNode_Decl* n) { free(n->name); ASTNODE_FREE(n); }
AstNode_Decl *AstNode_Decl_Create(char* name, AstNode* default_value)
{
    ASTNODE_ALLOC(n, AstNode_Decl, NodeType_Decl);
    n->name = AllocStringCopy(name);
    n->value = default_value;
    return n;
}

typedef struct { AstNode node; long value; } AstNode_Int;
void AstNode_Int_Free(AstNode_Int* n) { ASTNODE_FREE(n); }
AstNode_Int *AstNode_Int_Create(long value)
{
    ASTNODE_ALLOC(n, AstNode_Int, NodeType_Int);
    n->value = value;
    return n;
}

enum BinOpType
{
    BinOpType_Add, // +
    BinOpType_Sub, // -
    BinOpType_Mul, // *
    BinOpType_Div, // /
    BinOpType_Eql, // ==
    BinOpType_Neq, // !=
    BinOpType_Grt, // >
    BinOpType_Lst, // <
    BinOpType_Geq, // >=
    BinOpType_Leq, // <=
    BinOpType_And, // &&
    BinOpType_Cor, // ||
    BinOpType_Bnd, // &
    BinOpType_Bor, // |
    BinOpType_Mod, // %
    BinOpType_Xor, // ^
    BinOpType_Set, // =
};

const char* BinOpType_ToString(enum BinOpType t)
{
    switch(t)
    {
    case BinOpType_Add: return "+";
    case BinOpType_Sub: return "-";
    case BinOpType_Mul: return "*";
    case BinOpType_Div: return "/";
    case BinOpType_Eql: return "==";
    case BinOpType_Neq: return "!=";
    case BinOpType_Grt: return ">";
    case BinOpType_Lst: return "<";
    case BinOpType_Geq: return ">=";
    case BinOpType_Leq: return "<=";
    case BinOpType_And: return "&&";
    case BinOpType_Cor: return "||";
    case BinOpType_Bnd: return "&";
    case BinOpType_Bor: return "|";
    case BinOpType_Mod: return "%";
    case BinOpType_Xor: return "^";
    case BinOpType_Set: return "=";
    default: return "<UNKNOWN>";
    }
}
const char* BinOpType_ToString2(enum BinOpType t)
{
    switch(t)
    {
    case BinOpType_Add: return "Add";
    case BinOpType_Sub: return "Substract";
    case BinOpType_Mul: return "Multiply";
    case BinOpType_Div: return "Divide";
    case BinOpType_Eql: return "Equals";
    case BinOpType_Neq: return "Not Equal";
    case BinOpType_Grt: return "Greater Then";
    case BinOpType_Lst: return "Less Than";
    case BinOpType_Geq: return "Greater or Equal";
    case BinOpType_Leq: return "Less or Equal";
    case BinOpType_And: return "And";
    case BinOpType_Cor: return "Rr";
    case BinOpType_Bnd: return "Binary And";
    case BinOpType_Bor: return "Binary Or";
    case BinOpType_Mod: return "Modulus";
    case BinOpType_Xor: return "Xor";
    case BinOpType_Set: return "Set";
    default: return "<UNKNOWN>";
    }
}

typedef struct { AstNode node; enum BinOpType type; AstNode* left; AstNode* right; } AstNode_BinOp;
void AstNode_BinOp_Free(AstNode_BinOp* n) { AstNode_FreeAny(n->left); AstNode_FreeAny(n->right); ASTNODE_FREE(n); }
AstNode_BinOp *AstNode_BinOp_Create(enum BinOpType type, AstNode* left, AstNode* right)
{
    ASTNODE_ALLOC(n, AstNode_BinOp, NodeType_BinOp);
    n->type = type;
    n->left = left;
    n->right = right;
    return n;
}

typedef struct { AstNode node; float value; } AstNode_Float;
typedef struct { AstNode node; char* value; } AstNode_StringLit;
typedef struct { AstNode node; AstNodePtrList nodes; } AstNode_Block;
void AstNode_Block_Free(AstNode_Block* n) { AstNodePtrList_ForEach(&n->nodes, &AstNode_FreeAny); ASTNODE_FREE(n); }
AstNode_Block *AstNode_Block_Create(AstNodePtrList* nodes) // NOTE: This list is moved to the node.
{
    ASTNODE_ALLOC(n, AstNode_Block, NodeType_Block);
    n->nodes.data = nodes->data;
    n->nodes.count = nodes->count;
    return n;
}

typedef struct { AstNode node; AstNode* value; } AstNode_Return;
void AstNode_Return_Free(AstNode_Return* n) { AstNode_FreeAny(n->value); ASTNODE_FREE(n); }
AstNode_Return *AstNode_Return_Create(AstNode* value)
{
    ASTNODE_ALLOC(n, AstNode_Return, NodeType_Return);
    n->value = value;
    return n;
}

void AstNode_FreeAny(AstNode* node)
{
    if(!node) return;
    switch(node->type)
    {
    // case NodeType_Error: AstNode_Error_Free(node); break;
    case NodeType_Proc: AstNode_Proc_Free((AstNode_Proc*)node); break;
    case NodeType_Iden: AstNode_Iden_Free((AstNode_Iden*)node); break;
    case NodeType_Int: AstNode_Int_Free((AstNode_Int*)node); break;
    // case NodeType_Float: AstNode_Float_Free(node); break;
    // case NodeType_StringLit: AstNode_StringLit_Free(node); break;
    case NodeType_Return: AstNode_Return_Free((AstNode_Return*)node); break;
    case NodeType_Block: AstNode_Block_Free((AstNode_Block*)node); break;
    case NodeType_Decl: AstNode_Decl_Free((AstNode_Decl*)node); break;
    case NodeType_BinOp: AstNode_BinOp_Free((AstNode_BinOp*)node); break;
    default: break;
    }
}

typedef struct { Lexer* l; AstNode* root; } Parser;

void Parser_Initialize(Parser* parser, Lexer* l)
{
    parser->l = l;
    parser->root = NULL;
}

void Parser_Free(Parser* self)
{
    AstNode_FreeAny(self->root);
}

void Parser__Error(Parser* self, const char* msg)
{
    fprintf(stderr, "\033[0;31mError:\033[0;0m %s\n", msg);
}

void Parser__ErrorFormat(Parser* self, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, 1024, format, args);
    Parser__Error(self, buffer);
    va_end(args);
}

void Parser__Error_Expected(Parser* self, const char* expected, const char* actual)
{
    Parser__ErrorFormat(self, "Expected \"%s\", but got \"%s\"", expected, actual);
}

void Parser__Error_ExpectedMulti(Parser* self, const char* expected, const char* actual)
{
    Parser__ErrorFormat(self, "Expected %s, but got \"%s\"", expected, actual);
}

void Parser__FormatToken(Parser* self, char* dest, size_t max_size, Token* token)
{
    if(token->type < TokenType__Last)
        snprintf(dest, max_size, "%s", TokenType_ToString(token->type)); // TODO: for iden, num, str printf value too
    else
        snprintf(dest, max_size, "'%c'", token->type);
}

Token* Parser__ExpectKeyword(Parser* self, const char* kw, bool* correct)
{
    Token* t = Lexer_Peek(self->l);
    if(t->type != TokenType_Iden || strcmp(t->value, kw) != 0)
    {
        *correct = false;
        char *s = malloc(128);
        Parser__FormatToken(self, s, 128, t);
        Parser__Error_Expected(self, kw, s);
        free(s);
    }

    *correct = true;
    return t;
}

Token* Parser__ExpectKeywordAndMove(Parser* self, const char* kw)
{
    bool correct;
    Token* t = Parser__ExpectKeyword(self, kw, &correct);
    if(correct) Lexer_Next(self->l);
    return t;
}

Token* Parser__Expect(Parser* self, int tt, bool* correct)
{
    Token* t = Lexer_Peek(self->l);
    if(t->type != tt)
    {
        if(correct) *correct = false;
        char* s1 = malloc(128); char* s2 = malloc(2);
        Token fake_token = { tt, NULL };
        Parser__FormatToken(self, s1, 128, t);
        Parser__FormatToken(self, s2, 128, &fake_token);
        Parser__Error_Expected(self, s1, s2);
        free(s1); free(s2);
    }
    if(correct) *correct = true;
    return t;
}

Token* Parser__ExpectAndMove(Parser* self, int tt)
{
    bool correct;
    Token* t = Parser__Expect(self, tt, &correct);
    if(correct) Lexer_Next(self->l);
    return t;
}

bool Parser__Is(Parser* self, int tt)
{ return Lexer_Peek(self->l)->type == tt; }

bool Parser__IsKeyword(Parser* self, const char* kw)
{
    return Lexer_Peek(self->l)->type == TokenType_Iden
        && strcmp(Lexer_Peek(self->l)->value, kw) == 0;
}

AstNode* Parser_ParseExpression(Parser* self);
AstNode* Parser_ParseEx_Atom(Parser* self)
{
    Token* t = Lexer_Peek(self->l);
    /**/ if(t->type == TokenType_Int)
        return (AstNode*)AstNode_Int_Create(atol(Lexer_Next(self->l)->value));
    else if(t->type == TokenType_Iden)
        return (AstNode*)AstNode_Iden_Create(Lexer_Next(self->l)->value);
    else if(t->type == '(')
    {
        Lexer_Next(self->l);
        AstNode* n = Parser_ParseExpression(self);
        Parser__ExpectAndMove(self, ')');
        return n;
    }
    else
    {
        const char* actual;
        char chars[] = { t->type, '\0' };
        if(t->type < TokenType__Last) actual = TokenType_ToString(t->type);
        else actual = chars;
        Parser__Error_ExpectedMulti(self, "one of integer, identifier, \"(\"", actual);
        return NULL;
    }
}

#define DECLARE_PARSER_BIN_EXPR_FN(NAME, BEFORE, TYPE, TEST) \
    AstNode* Parser_ParseEx_##NAME(Parser* self) \
    { AstNode* n = Parser_ParseEx_##BEFORE(self); Token* t = Lexer_Peek(self->l); \
      for(int tt = t->type; TEST; (t = Lexer_Peek(self->l)), (tt = t->type)) \
      { Lexer_Next(self->l); n = (AstNode*)AstNode_BinOp_Create(TYPE, n, Parser_ParseEx_##BEFORE(self)); } \
      return n; }

DECLARE_PARSER_BIN_EXPR_FN(Mul, Atom, tt == '*' ? BinOpType_Mul : BinOpType_Div, tt == '*' || tt == '/')
DECLARE_PARSER_BIN_EXPR_FN(Add, Mul, tt == '+' ? BinOpType_Add : BinOpType_Sub, tt == '+' || tt == '-')
DECLARE_PARSER_BIN_EXPR_FN(Cmp, Add, tt == '>' ? BinOpType_Grt : BinOpType_Lst, tt == '>' || tt == '<')
DECLARE_PARSER_BIN_EXPR_FN(Ceq, Cmp,
    tt == TokenType_GreaterEqual ? BinOpType_Geq : BinOpType_Leq,
    tt == TokenType_GreaterEqual || tt == TokenType_LessEqual)
DECLARE_PARSER_BIN_EXPR_FN(Eql, Ceq,
    tt == TokenType_DoubleEqual ? BinOpType_Eql : BinOpType_Neq,
    tt == TokenType_DoubleEqual || tt == TokenType_NotEqual)
DECLARE_PARSER_BIN_EXPR_FN(Set, Eql, BinOpType_Set, tt == '=')

AstNode* Parser_ParseExpression(Parser* self)
{
    return Parser_ParseEx_Set(self);
    // char* s = malloc(128);
    // Parser__FormatToken(self, s, 128, Lexer_Peek(self->l));
    // Parser__Error_ExpectedMulti(self, "an expression", s);
    // free(s);
}

AstNode* Parser_ParseStatement(Parser* self)
{
    /**/ if(Parser__IsKeyword(self, "return"))
    {
        Lexer_Next(self->l);
        AstNode* n = (AstNode*)AstNode_Return_Create(Parser_ParseExpression(self));
        Parser__ExpectAndMove(self, ';');
        return n;
    }
    else if(Parser__Is(self, '{'))
    {
        Lexer_Next(self->l);
        AstNodePtrList stmts; AstNodePtrList_Initialize(&stmts);
        while(!Parser__Is(self, '}'))
            AstNodePtrList_PushValue(&stmts, Parser_ParseStatement(self));
        Lexer_Next(self->l);
        return (AstNode*)AstNode_Block_Create(&stmts);
    }
    else if(Lexer_Peek(self->l)->type == TokenType_Iden && Lexer_PeekN(self->l, 2)->type == TokenType_Iden)
    {
        /* Token* type_token = */ Lexer_Next(self->l);
        Token* name_token = Lexer_Next(self->l);

        AstNode* default_value = NULL;
        if(Parser__Is(self, '='))
        {
            Lexer_Next(self->l);
            default_value = Parser_ParseExpression(self);
        }
        Parser__ExpectAndMove(self, ';');

        return (AstNode*)AstNode_Decl_Create(name_token->value, default_value);
    }
    else
    {
        AstNode* n = Parser_ParseExpression(self);
        Parser__ExpectAndMove(self, ';');
        return n;
    }
}

AstNode_Proc* Parser_ParseProc(Parser* self)
{
    Parser__ExpectKeywordAndMove(self, "proc");
    char* name = Lexer_Next(self->l)->value;
    CStrList args; CStrList_Initialize(&args);

    Parser__ExpectAndMove(self, '(');
    while(!Parser__Is(self, ')'))
    {
        bool correct;
        Token* t = Parser__Expect(self, TokenType_Iden, &correct);
        if(correct) CStrList_PushValue(&args, AllocStringCopy(t->value));
        Lexer_Next(self->l);

        if(!Parser__Is(self, ',')) break;
        else Lexer_Next(self->l);
    }
    Parser__ExpectAndMove(self, ')');
    AstNode* body = Parser_ParseStatement(self);
    return AstNode_Proc_Create(name, &args, body);
}

void AstNode_Show(AstNode* node, int indent);
void AstNode_Proc_Show(AstNode_Proc* node, int indent) {
    printf("Procedure: \"%s\", %ld args.\n", node->name, node->args.count);
    AstNode_Show(node->body, indent + 1);
}
void AstNode_Int_Show(AstNode_Int* node, int indent) {
    printf("Integer: %ld\n", node->value);
}
void AstNode_Iden_Show(AstNode_Iden* node, int indent) {
    printf("Identifier: '%s'\n", node->iden);
}
void AstNode_Return_Show(AstNode_Return* node, int indent) {
    printf("Return:\n");
    AstNode_Show(node->value, indent + 1);
}
void AstNode_BinOp_Show(AstNode_BinOp* node, int indent) {
    printf("BinOp: %s\n", BinOpType_ToString2(node->type));
    AstNode_Show(node->left, indent + 1);
    AstNode_Show(node->right, indent + 1);
}
void AstNode_Block_Show(AstNode_Block* node, int indent) {
    printf("Block: %ld statements:\n", node->nodes.count);
    for(size_t i = 0; i < node->nodes.count; ++i)
        AstNode_Show(node->nodes.data[i], indent + 1);
}

void AstNode_Show(AstNode* node, int indent)
{
    for(int i = 0; i < indent * 2; ++i) fputc(' ', stdout);
    if(!node) { printf("NULL\n"); return; }
    switch(node->type)
    {
    // case NodeType_Error: AstNode_Error_Free(node); break;
    case NodeType_Proc: AstNode_Proc_Show((AstNode_Proc*)node, indent); break;
    case NodeType_Iden: AstNode_Iden_Show((AstNode_Iden*)node, indent); break;
    case NodeType_Int: AstNode_Int_Show((AstNode_Int*)node, indent); break;
    // case NodeType_Float: AstNode_Float_Free(node); break;
    // case NodeType_StringLit: AstNode_StringLit_Free(node); break;
    case NodeType_Return: AstNode_Return_Show((AstNode_Return*)node, indent); break;
    case NodeType_Block: AstNode_Block_Show((AstNode_Block*)node, indent); break;
    case NodeType_BinOp: AstNode_BinOp_Show((AstNode_BinOp*)node, indent); break;
    default: printf("Node: %s\n", NodeType_ToString(node->type));break;
    }
}

enum IR_OpType
{
    IR_OpType_Add, // +
    IR_OpType_Sub, // -
    IR_OpType_Mul, // *
    IR_OpType_Div, // /
    IR_OpType_Eql, // ==
    IR_OpType_Neq, // !=
    IR_OpType_Grt, // >
    IR_OpType_Lst, // <
    IR_OpType_Geq, // >=
    IR_OpType_Leq, // <=
    IR_OpType_And, // &&
    IR_OpType_Cor, // ||
    IR_OpType_Bnd, // &
    IR_OpType_Bor, // |
    IR_OpType_Mod, // %
    IR_OpType_Xor, // ^
    IR_OpType_Neg, // -
    IR_OpType_Not, // !
    IR_OpType_Bno, // ~
    IR_OpType_Cpy, // cpy X <- Y
    IR_OpType_SetMem, // setmem ADDR <- X
    IR_OpType_SetOff, // setoff OFFSET <- X
    IR_OpType_Ret, // ret X
    IR_OpType__Last,
};

const char* IR_OpType_ToString(enum IR_OpType t)
{
    static const char* strs[] =
    {
        "Add",
        "Sub",
        "Mul",
        "Div",
        "Eql",
        "Neq",
        "Grt",
        "Lst",
        "Geq",
        "Leq",
        "And",
        "Cor",
        "Bnd",
        "Bor",
        "Mod",
        "Xor",
        "Neg",
        "Not",
        "Bno",
        "Cpy",
        "SetMem",
        "SetOff",
        "Ret",
    };
    return t < IR_OpType__Last ? strs[t] : "<UNKNOWN>";
}
const char* IR_OpType_ToString2(enum IR_OpType t)
{
    static const char* strs[] =
    {
        "Add",
        "Substract",
        "Multiply",
        "Divide",
        "Equals",
        "Not Equal",
        "Greater Then",
        "Less Than",
        "Greater or Equal",
        "Less or Equal",
        "And",
        "Rr",
        "Binary And",
        "Binary Rr",
        "Modulus",
        "Xor",
        "Negation",
        "Not",
        "Binary Not",
        "Copy",
        "Set Memory",
        "Set Offset",
        "Return",
    };
    return t < IR_OpType__Last ? strs[t] : "<UNKNOWN>";
}

typedef unsigned long IR_V;
typedef unsigned long IR_L;
typedef char IR_O;
typedef struct { char* name; IR_V var; } IR_Var; DECLARE_HASHMAP_TYPE(IR_Var, const char*);
typedef struct { IR_O type; IR_V res, lhs, rhs; IR_L lbl; } IR_Instr;
DECLARE_LIST_TYPE(IR_Instr);
typedef struct
{
    char* name;
    IR_VarHashMap locals;
    IR_V next_local;
    IR_InstrList code;
} IR_Proc;
DECLARE_HASHMAP_TYPE(IR_Proc, const char*);

bool IR_ProcHashMap_CompareKey(IR_Proc* proc, const char* key) { return StringEqual(proc->name, key); }
bool IR_VarHashMap_CompareKey(IR_Var* var, const char* key) { return StringEqual(var->name, key); }

IR_Var IR_Var_Create(const char* name, IR_V var) { return (IR_Var){ name ? AllocStringCopy(name) : NULL, var }; }
void IR_Var_Free(IR_Var* self) { free(self->name); }

void IR_Proc_Initialize(IR_Proc* proc, const char* name)
{
    proc->name = AllocStringCopy(name);
    proc->next_local = 0;
    IR_VarHashMap_Initialize(&proc->locals);
    IR_InstrList_Initialize(&proc->code);
}

IR_Var IR_Proc_NextVar(IR_Proc* self, const char* name)
{ return IR_Var_Create(name, self->next_local++); }

IR_Var* IR_Proc_GetVar(IR_Proc* self, const char* name)
{ return IR_VarHashMap_Find(&self->locals, name, &IR_VarHashMap_CompareKey); }

void IR_Proc_Free(IR_Proc* proc)
{
    free(proc->name);
    IR_VarHashMap_ForEachRef(&proc->locals, &IR_Var_Free);
    IR_InstrList_Free(&proc->code);
}

typedef struct { IR_ProcHashMap procs; } IR_Builder;

void IR_Builder_Initialize(IR_Builder* self)
{
    IR_ProcHashMap_Initialize(&self->procs);
}

void IR_Builder_Free(IR_Builder* self)
{
    IR_ProcHashMap_ForEachRef(&self->procs, &IR_Proc_Free);
    IR_ProcHashMap_Free(&self->procs);
}

void IR_Proc__EmitV(IR_Proc* self, enum IR_OpType op, IR_V val)
{
    IR_Instr instr = { op, 0, val, 0, 0 };
    IR_InstrList_PushRef(&self->code, &instr);
}
void IR_Proc__Emit1(IR_Proc* self, enum IR_OpType op, IR_V res, IR_V val)
{
    IR_Instr instr = { op, res, val, 0, 0 };
    IR_InstrList_PushRef(&self->code, &instr);
}
void IR_Proc__Emit2(IR_Proc* self, enum IR_OpType op, IR_V res, IR_V lhs, IR_V rhs)
{
    IR_Instr instr = { op, res, lhs, rhs, 0 };
    IR_InstrList_PushRef(&self->code, &instr);
}
void IR_Proc__EmitL(IR_Proc* self, enum IR_OpType op, IR_L lbl, IR_V lhs, IR_V rhs)
{
    IR_Instr instr = { op, 0, lhs, rhs, lbl };
    IR_InstrList_PushRef(&self->code, &instr);
}

void IR_Builder__Error(IR_Builder* self, const char* msg)
{
    fprintf(stderr, "\033[0;31mError:\033[0;0m %s\n", msg);
}
void IR_Builder__ErrorFormat(IR_Builder* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    fprintf(stderr, "\033[0;31mError:\033[0;0m\n");
    vfprintf(stderr, fmt, va);
    fputc('\n', stderr);
    va_end(va);
}

void IR_Builder__Fatal(IR_Builder* self)
{
    fprintf(stderr, "\033[0;31mFatal Error![0;0m\n");
    exit(1);
}

IR_V IR_Proc__Target(IR_Proc* self, IR_V target)
{
    if(target != (IR_V)-1) return target;
    return self->next_local++;
}

IR_V IR_Builder_BuildNode(IR_Builder* self, AstNode* node, IR_Proc* proc, IR_V target) // TODO: IR_Locals
{
    switch(node->type)
    {
        case NodeType_Proc:
        {
            IR_Proc initproc;
            IR_Proc_Initialize(&initproc, ((AstNode_Proc*)node)->name);
            IR_Proc* newproc = IR_ProcHashMap_PushRef(&self->procs, &initproc);
            IR_Builder_BuildNode(self, ((AstNode_Proc*)node)->body, newproc, (IR_V)-1);
        } break;
        case NodeType_Return:
        {
            IR_V o = IR_Builder_BuildNode(self, ((AstNode_Return*)node)->value, proc, (IR_V)-1);
            IR_Proc__EmitV(proc, IR_OpType_Ret, o);
        } break;
        case NodeType_Block:
        {
            AstNodePtrList* l = &((AstNode_Block*)node)->nodes;
            for(size_t i = 0; i < l->count; ++i)
                IR_Builder_BuildNode(self, l->data[i], proc, i == l->count - 1 ? (target = IR_Proc__Target(proc, target)) : (IR_V)-1);
        } break;
        case NodeType_Int: IR_Proc__Emit1(proc, IR_OpType_Cpy, target = IR_Proc__Target(proc, target), ((AstNode_Int*)node)->value); break;
        case NodeType_Iden:
        {
            IR_Var* var = NULL;
            /* var = IR_Build__GetGlobal(((AstNode_Iden*)node)->iden) */
            if(!var) // Not a global.
            {
                if(!proc)
                {
                    IR_Builder__Error(self, "Internal: `proc` is NULL on NodeType_Iden and no global var found.");
                    IR_Builder__Fatal(self);
                }

                var = IR_Proc_GetVar(proc, ((AstNode_Iden*)node)->iden);
                if(!var)
                {
                    IR_Builder__ErrorFormat(self, "No such variable: '%s'", ((AstNode_Iden*)node)->iden);
                }
            }
            IR_Proc__Emit1(proc, IR_OpType_Cpy, target = IR_Proc__Target(proc, target), ((AstNode_Int*)node)->value);
        } break;
        default: IR_Builder__ErrorFormat(self, "Failed to build IR from '%s'", NodeType_ToString2(node->type)); break;
    }
    return target;
}

void IR_Builder__DumpInstr(IR_Builder* self, FILE* output, IR_Instr* instr)
{
    fprintf(output, "\t%lu = %lu %s %lu (->%lu)\n",
        instr->res, instr->lhs, IR_OpType_ToString(instr->type), instr->rhs, instr->lbl);
}

void IR_Builder_Dump(IR_Builder* self, FILE* output)
{
    int line = 1;
    for(size_t pi = 0; pi < self->procs.count; ++pi)
    {
        IR_Proc* proc = &self->procs.data[pi];

        fprintf(output, "\nproc %s: \t\t## {%zu}\n", proc->name, proc->code.count);
        line += 2;

        for(size_t i = 0; i < proc->code.count; ++i)
        {
            IR_Builder__DumpInstr(self, output, &proc->code.data[i]);
            line += 1;
        }
    }
    fprintf(output, "\n -- END --\n");
}

int System_Format(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    char* s = malloc(256);

    vsnprintf(s, 256, format, va);
    int ret = system(s);

    va_end(va);
    free(s);
    return ret;
}

bool Compiler_SaveDebugData = false;

int Command_Assembler(const char* input_name, const char* output_name)
{
    return System_Format("as %s %s -o %s", Compiler_SaveDebugData ? "-g" : "", input_name, output_name);
}

const char* MACOS_LINKER_COMMAND =
    "ld %s -o %s -e _start -static -macosx_version_min 10.13";
const char* LINUX_LINKER_COMMAND =
    "ld %s -o %s -e _start -static";

#ifdef __APPLE__
#define LINKER_COMMAND_FMT MACOS_LINKER_COMMAND
#else
#define LINKER_COMMAND_FMT LINUX_LINKER_COMMAND
#endif

int Command_Linker(const char* input_name, const char* output_name)
{
    return System_Format(LINKER_COMMAND_FMT, input_name, output_name);
}

int Build(const char* input_name, const char* output_name)
{
    char o_file[] = "/tmp/test_assem_out_XXXXXX";
    mkstemp(o_file);

    int ret = 0;
    if((ret = Command_Assembler(input_name, o_file)) != 0)
    {
        fprintf(stderr, "\033[0;31mError:\033[0;0m Assembler command failed with exit code %d!\n", ret);
        return ret;
    }

    if((ret = Command_Linker(o_file, output_name)) != 0)
    {
        fprintf(stderr, "\033[0;31mError:\033[0;0m Linker command failed with exit code %d!\n", ret);
        return ret;
    }

    return 0;
}

bool Compiler_IsDebug = false;
char* Compile_OutputAssemblyFile = NULL;

int Compile(const char* input_file, const char* output_file)
{

    Lexer lexer;
    Lexer_Initialize(&lexer, input_file);
    Lexer_LexFile(&lexer);

    // for(size_t i = 0; i < lexer.tokens.count; ++i)
    // {
    //     Token* t = &lexer.tokens.data[i];
    //     printf("Token: ");
    //     if(t->value == NULL) printf("'%c'", t->type);
    //     else printf("\"%s\" - %s", t->value, TokenType_ToString(t->type));
    //     printf("\n");
    // }

    Parser parser;
    Parser_Initialize(&parser, &lexer);

    parser.root = (AstNode*)Parser_ParseProc(&parser);
    Lexer_Free(&lexer);

    if(Compiler_IsDebug)
    {
        AstNode_Show(parser.root, 0);
        fputc('\n', stdout);
    }

    char temp_filename[] = "/tmp/test_compl_out_XXXXXX";
    mkstemp(temp_filename);
    if(!Compile_OutputAssemblyFile)
        Compile_OutputAssemblyFile = temp_filename;

    IR_Builder ir_builder;
    IR_Builder_Initialize(&ir_builder);

    IR_Builder_BuildNode(&ir_builder, parser.root, NULL, (IR_V)-1);
    IR_Builder_Dump(&ir_builder, stdout);

    IR_Builder_Free(&ir_builder);

    // Compiler compiler;
    // Compiler_Initialize(&compiler, Compile_OutputAssemblyFile);

    // Compiler_WriteHeaders(&compiler);
    // char* tmp = Compiler_CompileNode(&compiler, parser.root);
    // if(compiler.alloc_ret_reg) free(tmp);

    // Compiler_Free(&compiler);

    Parser_Free(&parser);
    if(Compiler_IsDebug) return 0;

    int ret = Build(Compile_OutputAssemblyFile, output_file);
    return ret == 0 ? 0 : 1;
}

int CLI(int argc, char* argv[])
{
    char input_file[256];
    char output_file[256];

    char last_opt = 0;
    for(int i = 1; i < argc; ++i)
    {
        if(argv[i][0] == '-')
        {
            last_opt = argv[i][1];
            switch(last_opt) {
                case 'd': Compiler_IsDebug = true; break;
                case 'g': Compiler_SaveDebugData = true; break;
                default: break;
            }
        }
        else
        {
            switch(last_opt) {
                case 'o': strncpy(output_file, argv[i], 256); break;
                case 's': Compile_OutputAssemblyFile = argv[i]; break;
                case   0: strncpy(input_file, argv[i], 256); break;
                default:
                {
                    fprintf(stderr, "\033[0;31mError:\033[0;0m Bad option: '%c'!\n", last_opt);
                } break;
            }
            last_opt = 0;
        }
    }

    return Compile(input_file, output_file);
}

int main(int argc, char* argv[])
{
    return CLI(argc, argv);
}

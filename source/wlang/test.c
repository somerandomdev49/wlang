#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <wlang/util.h>
#include <wlang/pim.h>
#include <wlang/type.h>
#include <wlang/ir.h>
#include <wlang/arch/gen.h> // Unused?
#include <wlang/arch/x86_64/arch.h> // Unused?

//:==========----------- Target-Specific Macros -----------==========://

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

//:==========----------- Lexer -----------==========://

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

typedef struct { int type; PIM_OWN char* value; } Token;
DECLARE_LIST_TYPE(Token)
DEFINE_LIST_TYPE(Token)

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
            else TokenList_PushValue(&l->tokens, (Token){ '>', NULL });
        }
        else if(fpeekc(l->source) == '<')
        {
            fgetc(l->source);
            if(fpeekc(l->source) == '=')
            {
                fgetc(l->source);
                TokenList_PushValue(&l->tokens, (Token){ TokenType_LessEqual, NULL });
            }
            else TokenList_PushValue(&l->tokens, (Token){ '<', NULL });
        }
        else if(fpeekc(l->source) == '!')
        {
            fgetc(l->source);
            if(fpeekc(l->source) == '=')
            {
                fgetc(l->source);
                TokenList_PushValue(&l->tokens, (Token){ TokenType_NotEqual, NULL });
            }
            else TokenList_PushValue(&l->tokens, (Token){ '!', NULL });
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

//:==========----------- AST -----------==========://

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
    NodeType_If,
    NodeType_While,
    NodeType_FCall,
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
    case NodeType_If: return "If";
    case NodeType_While: return "While";
    case NodeType_FCall: return "Function Call";
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
    case NodeType_If: return "If";
    case NodeType_While: return "While";
    case NodeType_FCall: return "FCall";
    default: return "<UNKNOWN>";
    }
}

typedef struct { enum NodeType type; } AstNode;
typedef AstNode* AstNodePtr;
DECLARE_LIST_TYPE(AstNode)
DEFINE_LIST_TYPE(AstNode)
DECLARE_LIST_TYPE(AstNodePtr)
DEFINE_LIST_TYPE(AstNodePtr)

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
AstNode_Proc* AstNode_Proc_Create(char* name, CStrList* args, AstNode* body)
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
AstNode_Iden* AstNode_Iden_Create(char* iden)
{
    ASTNODE_ALLOC(n, AstNode_Iden, NodeType_Iden);
    n->iden = AllocStringCopy(iden);
    return n;
}

typedef struct { AstNode node; char* name; AstNode* value; } AstNode_Decl;
void AstNode_Decl_Free(AstNode_Decl* n) { free(n->name); ASTNODE_FREE(n); }
AstNode_Decl* AstNode_Decl_Create(char* name, AstNode* default_value)
{
    ASTNODE_ALLOC(n, AstNode_Decl, NodeType_Decl);
    n->name = AllocStringCopy(name);
    n->value = default_value;
    return n;
}

typedef struct { AstNode node; long value; } AstNode_Int;
void AstNode_Int_Free(AstNode_Int* n) { ASTNODE_FREE(n); }
AstNode_Int* AstNode_Int_Create(long value)
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
    case BinOpType_Grt: return "Greater Than";
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
AstNode_BinOp* AstNode_BinOp_Create(enum BinOpType type, AstNode* left, AstNode* right)
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
void AstNode_Block_Free(AstNode_Block* n)
{
    AstNodePtrList_ForEach(&n->nodes, &AstNode_FreeAny);
    AstNodePtrList_Free(&n->nodes);
    ASTNODE_FREE(n);
}
AstNode_Block* AstNode_Block_Create(AstNodePtrList* nodes) // NOTE: This list is moved to the node.
{
    ASTNODE_ALLOC(n, AstNode_Block, NodeType_Block);
    n->nodes.data = nodes->data;
    n->nodes.count = nodes->count;
    return n;
}

typedef struct { AstNode node; AstNode* value; } AstNode_Return;
void AstNode_Return_Free(AstNode_Return* n) { AstNode_FreeAny(n->value); ASTNODE_FREE(n); }
AstNode_Return* AstNode_Return_Create(AstNode* value)
{
    ASTNODE_ALLOC(n, AstNode_Return, NodeType_Return);
    n->value = value;
    return n;
}

typedef struct { AstNode node; PIM_OWN AstNode* cond, * body, * othr; } AstNode_If;
void AstNode_If_Free(AstNode_If* n)
{
    AstNode_FreeAny(n->cond);
    AstNode_FreeAny(n->body);
    if(n->othr) AstNode_FreeAny(n->othr);
    ASTNODE_FREE(n);
}

AstNode_If* AstNode_If_Create(AstNode* cond, AstNode* body, AstNode* othr)
{
    ASTNODE_ALLOC(n, AstNode_If, NodeType_If);
    n->cond = cond;
    n->body = body;
    n->othr = othr;
    return n;
}

typedef struct { AstNode node; AstNode* func; PIM_OWN AstNodePtrList args; } AstNode_FCall;
void AstNode_FCall_Free(AstNode_FCall* n)
{
    AstNode_FreeAny(n->func);
    AstNodePtrList_ForEach(&n->args, &AstNode_FreeAny);
    AstNodePtrList_Free(&n->args);
    ASTNODE_FREE(n);
}

AstNode_FCall* AstNode_FCall_Create(AstNode* func, AstNodePtrList* args)
{
    ASTNODE_ALLOC(n, AstNode_FCall, NodeType_FCall);
    n->func = func;
    n->args.count = args->count;
    n->args.data = args->data;
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
    case NodeType_If: AstNode_If_Free((AstNode_If*)node); break;
    case NodeType_FCall: AstNode_FCall_Free((AstNode_FCall*)node); break;
    // TODO: case NodeType_While: AstNode_While_Free((AstNode_While*)node); break;
    // TODO: case NodeType_FCall: AstNode_FCall_Free((AstNode_FCall*)node); break;
    default: break;
    }
}

//:==========----------- Recursive-Descent Parser -----------==========://

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

AstNode* Parser_ParseEx_Unr(Parser* self)
{
    int unop_type = 0;
    /**/ if(Lexer_Peek(self->l)->type == '-')
    {
        unop_type = '-';
        fprintf(stderr, "Unary operators are not yet implemented.");
        return NULL;
    }
    else if(Lexer_Peek(self->l)->type == '!')
    {
        unop_type = '!';
        fprintf(stderr, "Unary operators are not yet implemented.");
        return NULL;
    }
    
    (void)unop_type;

    AstNode* node = Parser_ParseEx_Atom(self);

    if(Lexer_Peek(self->l)->type == '(')
    {
        AstNodePtrList nodes;
        AstNodePtrList_Initialize(&nodes);
        do
        {
            Lexer_Next(self->l);
            if(Lexer_Peek(self->l)->type == ')') break;
            AstNodePtrList_PushValue(&nodes, Parser_ParseExpression(self));
        }
        while(Lexer_Peek(self->l)->type != ')' && Lexer_Peek(self->l)->type == ',');
        Lexer_Next(self->l);
        return (AstNode*)AstNode_FCall_Create(node, PIM_MOVE(&nodes));
    }

    return node;
}

#define DEFINE_PARSER_BIN_EXPR_FN(NAME, BEFORE, TYPE, TEST) \
    AstNode* Parser_ParseEx_##NAME(Parser* self) \
    { AstNode* n = Parser_ParseEx_##BEFORE(self); Token* t = Lexer_Peek(self->l); \
      for(int tt = t->type; TEST; (t = Lexer_Peek(self->l)), (tt = t->type)) \
      { Lexer_Next(self->l); n = (AstNode*)AstNode_BinOp_Create(TYPE, n, Parser_ParseEx_##BEFORE(self)); } \
      return n; }

DEFINE_PARSER_BIN_EXPR_FN(Mul, Unr, tt == '*' ? BinOpType_Mul : BinOpType_Div, tt == '*' || tt == '/')
DEFINE_PARSER_BIN_EXPR_FN(Add, Mul, tt == '+' ? BinOpType_Add : BinOpType_Sub, tt == '+' || tt == '-')
DEFINE_PARSER_BIN_EXPR_FN(Cmp, Add, tt == '>' ? BinOpType_Grt : BinOpType_Lst, tt == '>' || tt == '<')
DEFINE_PARSER_BIN_EXPR_FN(Ceq, Cmp,
    tt == TokenType_GreaterEqual ? BinOpType_Geq : BinOpType_Leq,
    tt == TokenType_GreaterEqual || tt == TokenType_LessEqual)
DEFINE_PARSER_BIN_EXPR_FN(Eql, Ceq,
    tt == TokenType_DoubleEqual ? BinOpType_Eql : BinOpType_Neq,
    tt == TokenType_DoubleEqual || tt == TokenType_NotEqual)
DEFINE_PARSER_BIN_EXPR_FN(Set, Eql, BinOpType_Set, tt == '=')

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
        return (AstNode*)AstNode_Block_Create(PIM_MOVE(&stmts));
    }
    else if(Parser__IsKeyword(self, "if"))
    {
        Lexer_Next(self->l);
        Parser__ExpectAndMove(self, '(');
        AstNode* cond = Parser_ParseExpression(self);
        Parser__ExpectAndMove(self, ')');
        AstNode* body = Parser_ParseStatement(self);
        AstNode* othr = NULL;
        if(Parser__IsKeyword(self, "else"))
        {
            Lexer_Next(self->l);
            othr = Parser_ParseStatement(self);
        }
        return (AstNode*)AstNode_If_Create(cond, body, othr);
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

void AstNode_Show(const AstNode* node, int indent);
void AstNode_Proc_Show(const AstNode_Proc* node, int indent) {
    printf("Procedure: \"%s\", %ld args.\n", node->name, node->args.count);
    AstNode_Show(node->body, indent + 1);
}
void AstNode_Int_Show(const AstNode_Int* node, int indent) {
    printf("Integer: %ld\n", node->value);
}
void AstNode_Iden_Show(const AstNode_Iden* node, int indent) {
    printf("Identifier: '%s'\n", node->iden);
}
void AstNode_Return_Show(const AstNode_Return* node, int indent) {
    printf("Return:\n");
    AstNode_Show(node->value, indent + 1);
}
void AstNode_BinOp_Show(const AstNode_BinOp* node, int indent) {
    printf("BinOp: %s\n", BinOpType_ToString2(node->type));
    AstNode_Show(node->left, indent + 1);
    AstNode_Show(node->right, indent + 1);
}
void AstNode_Block_Show(const AstNode_Block* node, int indent) {
    printf("Block: %ld statements:\n", node->nodes.count);
    for(size_t i = 0; i < node->nodes.count; ++i)
        AstNode_Show(node->nodes.data[i], indent + 1);
}
void AstNode_FCall_Show(const AstNode_FCall* node, int indent) {
    printf("FCall: %ld arguments\n", node->args.count);
    AstNode_Show(node->func, indent + 1);
    for(int i = -2; i < indent * 2; ++i) fputc(' ', stdout);
    printf("Arguments:\n");
    for(size_t i = 0; i < node->args.count; ++i)
        AstNode_Show(node->args.data[i], indent + 1);
}
void AstNode_Decl_Show(const AstNode_Decl* node, int indent)
{
    printf("Variable %s", node->name);
    if(node->value) printf(" =");
    putc('\n', stdout);
    if(node->value) AstNode_Show(node->value, indent + 1);
}
void AstNode_If_Show(const AstNode_If* node, int indent)
{
    printf("If:\n");
    for(int i = -2; i < indent * 2; ++i) fputc(' ', stdout);
    printf("Cond:\n");
    AstNode_Show(node->cond, indent + 2);
    for(int i = -2; i < indent * 2; ++i) fputc(' ', stdout);
    printf("Body:\n");
    AstNode_Show(node->body, indent + 2);
    if(node->othr)
    {
        for(int i = -2; i < indent * 2; ++i) fputc(' ', stdout);
        printf("Else:\n");
        AstNode_Show(node->othr, indent + 2);
    }
}

void AstNode_Show(const AstNode* node, int indent)
{
    for(int i = 0; i < indent * 2; ++i) fputc(' ', stdout);
    if(!node) { printf("NULL\n"); return; }
    switch(node->type)
    {
    // case NodeType_Error: AstNode_Error_Free(node); break;
    case NodeType_Proc: AstNode_Proc_Show((const AstNode_Proc*)node, indent); break;
    case NodeType_Iden: AstNode_Iden_Show((const AstNode_Iden*)node, indent); break;
    case NodeType_Int: AstNode_Int_Show((const AstNode_Int*)node, indent); break;
    // case NodeType_Float: AstNode_Float_Free(node); break;
    // case NodeType_StringLit: AstNode_StringLit_Free(node); break;
    case NodeType_Return: AstNode_Return_Show((const AstNode_Return*)node, indent); break;
    case NodeType_Block: AstNode_Block_Show((const AstNode_Block*)node, indent); break;
    case NodeType_BinOp: AstNode_BinOp_Show((const AstNode_BinOp*)node, indent); break;
    case NodeType_If: AstNode_If_Show((const AstNode_If*)node, indent); break;
    case NodeType_Decl: AstNode_Decl_Show((const AstNode_Decl*)node, indent); break;
    default: printf("Node: %s\n", NodeType_ToString(node->type));break;
    }
}

typedef struct
{
    const char* name;
    size_t stack_offset;
} Variable;

DECLARE_HASHMAP_TYPE(Variable, const char*)
DEFINE_HASHMAP_TYPE(Variable, const char*)

typedef struct
{
    const char* name;
    size_t locals;
    VariableHashMap vars;
} Procedure;

DECLARE_HASHMAP_TYPE(Procedure, const char*)
DEFINE_HASHMAP_TYPE(Procedure, const char*)

bool CompareVariableKey(Variable* var, const char* key) { return var->name == key || strcmp(var->name, key) == 0; }
bool CompareProcedureKey(Procedure* proc, const char* key) { return proc->name == key || strcmp(proc->name, key) == 0; }

void Procedure_Initialize(Procedure* self, const char* name)
{
    VariableHashMap_Initialize(&self->vars);
    self->name = name;
}

// char* Register_ToString[] = { "<error>", "rax", "rbx", "rcx", "rdx" };
// enum Register
// {
//     Register_None,
//     Register_Rax,
//     Register_Rbx,
//     Register_Rcx,
//     Register_Rdx,
//     Register__Last
// };

// const char* Register__32(const char* regstr)
// {
//     if(!regstr) return regstr;
//     int l = strlen(regstr);
//     if(l < 3) return NULL; // x86, no >=32bit register exists with a name less than 3 chars.

//     /**/ if(regstr[0] == 'r') // 64 bit register.
//     {
//         switch(regstr[1])
//         {
//         case 'a': return "eax";
//         case 'b': return "ebx";
//         case 'c': return "ecx";
//         case 'd': return "edx";
//         default: return NULL;
//         }
//     }
//     else if(regstr[0] == 'e') // 32-bit register, do nothing.
//         return regstr;

//     return NULL;
// }


typedef struct
{
    IRGenerator gen;
    char* pref_out_reg;
    bool alloc_ret_reg;
    size_t last_stack_offset;
    bool ret_written;
    int label_no;

    ProcedureHashMap procs;
    VariableHashMap globals;
    Procedure* current_proc; // TODO: VariableContext stack
} Compiler;
bool Compiler_IsDebug = false;

void Compiler_Initialize(Compiler* self, FILE* output)
{
    // printf("Compiler_IsDebug = %s\n", Compiler_IsDebug ? "yes" : "no");
    IR_Output_Initialize(&self->gen);
    self->current_proc = NULL;
    self->alloc_ret_reg = false;
    self->last_stack_offset = 8;
    self->ret_written = false;
    self->label_no = 0;
    ProcedureHashMap_Initialize(&self->procs);
}

void Compiler_Free(Compiler* self)
{
    ProcedureHashMap_Free(&self->procs);
    IR_Output_Free(&self->gen);
}


void Compiler__Error(Compiler* self, const char* msg)
{
    fprintf(stderr, "\033[0;31mError:\033[0;0m %s\n", msg);
}



bool Compiler__IsAssignable(Compiler* self, const AstNode* node) { return true; }

static char* Compiler__saved_regs[] = { "rbp", "rbx", /* not used: "r12", "r13", "r14", "r15" */ };

// In future, this will be all instructions.
void Compiler__GenerateReturn(Compiler* self)
{
    // for(size_t i = sizeof(Compiler__saved_regs) / sizeof(char*); i --> 0;)
    //     Compiler__Write(self, "pop %s", Compiler__saved_regs[i]);
    Compiler__Write(self, "ret");
}


// TODO: Omit the frame pointer
// TODO: Write something like Compiler__GetLocalSize()
void Compiler_CompileNode(Compiler* self, const AstNode* node)
{
    // printf("# Compiling %s\n", NodeType_ToString(node->type));
    // AstNode_Show(node, 0);
    switch(node->type)
    {
        case NodeType_Proc:
        {
            Procedure proc;
            Procedure_Initialize(&proc, ((AstNode_Proc*)node)->name);
            ProcedureHashMap_PushRef(&self->procs, &proc);
            self->current_proc = &self->procs.data[self->procs.count - 1];

            Compiler__WriteNoIndent(self, ".global %s", ((AstNode_Proc*)node)->name);
            Compiler__Begin(self, "%s:", ((AstNode_Proc*)node)->name);

            // TODO: Arguments.

            for(size_t i = 0; i < sizeof(Compiler__saved_regs) / sizeof(char*); ++i)
                Compiler__Write(self, "push %s", Compiler__saved_regs[i]);

            Compiler__Write(self, "mov rbp, rsp");

            
            Compiler_CompileNode(self, ((AstNode_Proc*)node)->body);
            if(!self->ret_written) Compiler__GenerateReturn(self);
            Compiler__End(self);
        } break;
        case NodeType_Block:
        {
            for(size_t i = 0; i < ((AstNode_Block*)node)->nodes.count; ++i)
            {
                Compiler_CompileNode(self, ((AstNode_Block*)node)->nodes.data[i]);
                // self->ret_written = false;
                // will this work? if(self->ret_written) break;
            }
        } break;
        case NodeType_FCall:
        {
            AstNode_FCall* n = (AstNode_FCall*)node;

            // Since we do not use any of those registers in code, we don't need to save/restore them.
            static char* registers[] = { "rdi", "rsi", "rdx", "rcx", "r8", "r9" };
            size_t reg_count = sizeof(registers) / sizeof(registers[0]);
            
            if(n->args.count > reg_count)
            {
                fprintf(stderr, "Passing more than %ld arguments to a function "
                        "is currently not supported. Arguments will be truncated.\n", reg_count);
            }

            size_t actual_arg_count = n->args.count > reg_count ? reg_count : n->args.count;

            for(size_t i = 0; i < actual_arg_count; ++i)
            {
                Compiler_CompileNode(self, n->args.data[i]);
                Compiler__Write(self, "mov %s, rax", registers[i]);
            }

            if(n->func->type != NodeType_Iden)
            {
                fprintf(stderr, "Calling a function pointer is not currently supported.");
            }
            
            Compiler__Write(self, "call %s", ((AstNode_Iden*)n->func)->iden);
        } break;
        case NodeType_If:
        {
            AstNode_If* n = (AstNode_If*)node;

            // TODO: Check if the the condition is one of the '>', '<', '==', '!=' or
            //       similar nodes and instead of computing the node as a binary operation,
            //       use the inverse of the corresponding jmp instr.
            Compiler_CompileNode(self, n->cond);
            Compiler__Write(self, "cmp rax, 0");

            int label_exit = self->label_no++;
            int label_else = n->othr ? self->label_no++ : label_exit;

            Compiler__Write(self, "je .L%d", label_else);
            Compiler_CompileNode(self, n->body);

            if(n->othr)
            {
                // We don't need to jump if there is no else branch
                Compiler__Write(self, "jmp .L%d", label_exit);

                Compiler__WriteNoIndent(self, ".L%d:", label_else);
                Compiler_CompileNode(self, n->othr);
            }
            
            Compiler__WriteNoIndent(self, ".L%d:", label_exit);
        } break;
        case NodeType_Int:
        {
            Compiler__Write(self, "mov rax, %ld", ((AstNode_Int*)node)->value);
        } break;
        case NodeType_Decl:
        {
            if(self->current_proc == NULL) { Compiler__Error(self, "Global variables are not supported for now."); break; }
            size_t v_stack_offset = self->last_stack_offset;
            self->last_stack_offset += 8; // TODO: Variable size

            VariableHashMap_PushValue(&self->current_proc->vars, (Variable){
                /* name:   */ ((AstNode_Decl*)node)->name, // TODO: AllocStringCopy & Free
                /* offset: */ v_stack_offset
            });


            if(((AstNode_Decl*)node)->value != NULL)
            {
                AstNode_Iden* i = AstNode_Iden_Create(((AstNode_Decl*)node)->name);
                AstNode_BinOp* s = AstNode_BinOp_Create(BinOpType_Set, (AstNode*)i, ((AstNode_Decl*)node)->value);
                char* offset_string = Compiler__StackOffsetString(v_stack_offset);
                Compiler_CompileNode(self, (AstNode*)s);
                Compiler__Write(self, "sub rsp, 8"); // Because and 'push' instruction will overwrite.
                Compiler__Write(self, "mov %s, rax", offset_string);

                free(offset_string);
                AstNode_BinOp_Free(s);
            }
        } break;
        case NodeType_Iden:
        {
            if(self->current_proc == NULL)
            {
                Compiler__Error(self, "Global variables are not supported.");
                break;
            }
            Variable* v = VariableHashMap_Find(&self->current_proc->vars, ((AstNode_Iden*)node)->iden, &CompareVariableKey);
            char* loc = Compiler__StackOffsetString(v->stack_offset);
            Compiler__Write(self, "mov rax, %s", loc);
            // Compiler__WriteBinInstr(self, "mov", loc);
            free(loc);
        } break;
        case NodeType_BinOp:
        {
            AstNode* lhs = ((AstNode_BinOp*)node)->left;
            AstNode* rhs = ((AstNode_BinOp*)node)->right;
            switch(((AstNode_BinOp*)node)->type)
            {
                case BinOpType_Set:
                {
                    if(!Compiler__IsAssignable(self, lhs)) { Compiler__Error(self, "Cannot assign!"); break; }
                    Variable* v = VariableHashMap_Find(&self->current_proc->vars, ((AstNode_Iden*)lhs)->iden, &CompareVariableKey);

                    char* var_loc = Compiler__StackOffsetString(v->stack_offset);
                    self->pref_out_reg = var_loc;
                    Compiler_CompileNode(self, rhs);
                } break;

#define BIN_IMPL_BASE Compiler_CompileNode(self, rhs); \
                      Compiler__Write(self, "push rax"); \
                      Compiler_CompileNode(self, lhs); \
                      Compiler__Write(self, "pop rbx");

                case BinOpType_Add: { BIN_IMPL_BASE; Compiler__Write(self, "add rax, rbx"); } break;
                case BinOpType_Sub: { BIN_IMPL_BASE; Compiler__Write(self, "sub rax, rbx"); } break;
#define BIN_IMPL_BASE_CMP(NAME_STR)  BIN_IMPL_BASE; \
                                     Compiler__Write(self, "cmp rax, rbx"); \
                                     Compiler__Write(self, "set" NAME_STR " al"); \
                                     Compiler__Write(self, "movzx rax, al");
                                     // ^^ Could have "xor rax, rax" before but GCC uses this so...

                case BinOpType_Lst: { BIN_IMPL_BASE_CMP("l"); } break;
                case BinOpType_Leq: { BIN_IMPL_BASE_CMP("le"); } break;
                case BinOpType_Grt: { BIN_IMPL_BASE_CMP("g"); } break;
                case BinOpType_Geq: { BIN_IMPL_BASE_CMP("ge"); } break;
#undef BIN_IMPL_BASE_CMP
#undef BIN_IMPL_BASE
                default: Compiler__Error(self, "This binary operation is not yet implemented!");
            }
        } break;
        case NodeType_Return:
        {
            // Value already in RAX.
            Compiler_CompileNode(self, ((AstNode_Return*)node)->value);
            Compiler__GenerateReturn(self);
            self->ret_written = true;
        } break;
        default: Compiler__Write(self, "# Did not compile node of type %s.", NodeType_ToString2(node->type)); break;
    }
}

int System(const char *cmd)
{
    printf("system: %s\n", cmd);
    return system(cmd);
}

int System_Format(const char* format, ...)
{
    va_list va;
    va_start(va, format);
    char* s = malloc(256);

    vsnprintf(s, 256, format, va);
    int ret = System(s);

    va_end(va);
    free(s);
    return ret;
}

bool Compiler_SaveDebugData = false;

int Command_Assembler(const char* input_name, const char* output_name)
{
    return System_Format("as %s %s -o %s", Compiler_SaveDebugData ? "-g" : "", input_name, output_name);
}

#ifdef __APPLE__
#define LINKER_COMMAND_FMT "ld %s -o %s -e _start -static -macosx_version_min 10.13"
#else
#define LINKER_COMMAND_FMT "ld %s -o %s -e _start -static"
#endif

int Command_Linker(const char* input_name, const char* output_name)
{
    return System_Format(LINKER_COMMAND_FMT, input_name, output_name);
}

bool Compiler_DontLink = false;
bool Compiler_DontCompile = false;

int Build(const char* input_name, const char* output_name)
{
    char tmp_file[] = "/tmp/test_assem_out_XXXXXX";
    mkstemp(tmp_file);

    const char* o_file = Compiler_DontLink ? output_name : tmp_file;

    int ret = 0;
    if((ret = Command_Assembler(input_name, o_file)) != 0)
    {
        fprintf(stderr, "\033[0;31mError:\033[0;0m Assembler command failed with exit code %d!\n", ret);
        return ret;
    }

    if(!Compiler_DontLink)
    {
        if((ret = Command_Linker(o_file, output_name)) != 0)
        {
            fprintf(stderr, "\033[0;31mError:\033[0;0m Linker command failed with exit code %d!\n", ret);
            return ret;
        }
    }

    return 0;
}

// bool Compiler_IsDebug = false;
char* Compile_OutputAssemblyFile = NULL;

int Compile(const char* input_file, const char* output_file)
{
    if(!Compiler_DontCompile)
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

        char temp_filename[] = "/tmp/test_compl_out_XXXXXX";
        mkstemp(temp_filename);
        if(!Compile_OutputAssemblyFile)
            Compile_OutputAssemblyFile = temp_filename;

        FILE* f = fopen(Compile_OutputAssemblyFile, "w");

        Compiler compiler;
        Compiler_Initialize(&compiler, f);
        Compiler_WriteHeaders(&compiler);
        
        while(Lexer_Peek(&lexer)->type != TokenType_Eof)
        {
            AstNode_Proc* node = Parser_ParseProc(&parser);
            Compiler_CompileNode(&compiler, (AstNode*)node);
            AssemblyOutput_Flush(&compiler.output);

            if(Compiler_IsDebug)
            {
                AstNode_Show((AstNode*)node, 0);
                fputc('\n', stdout);
            }
        }

        Lexer_Free(&lexer);


        Compiler_Free(&compiler);

        fclose(f);

        Parser_Free(&parser);
        if(Compiler_IsDebug) return 0;
    }
    int ret = Build(Compile_OutputAssemblyFile, output_file);
    return ret == 0 ? 0 : 1;
}

int CLI(int argc, char* argv[])
{
    if(argc < 2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "\t%s <input files> [flags]\n", argv[0]);
        fprintf(stderr, "Flags:\n");
        fprintf(stderr, "\t-o <file>\tset output file\n");
        fprintf(stderr, "\t-s <file>\tset output assembly file\n");
        fprintf(stderr, "\t-d\t\tset debug mode\n");
        fprintf(stderr, "\t-g\t\tassemble and link with debug data\n");
        fprintf(stderr, "\t-c\t\tdo not link, only compile\n");
        fprintf(stderr, "\t-z\t\tdo not compile, just repeat the assembler and linker commands\n");
        return 1;
    }

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
                case 'c': Compiler_DontLink = true; break;
                case 'z': Compiler_DontCompile = true; break;
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

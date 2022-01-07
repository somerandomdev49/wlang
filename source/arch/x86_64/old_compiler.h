/// DO NOT USE, OLD (PRE IR, REPLACED WITH NEW COMPILER) COMPILER CODE ///

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

typedef struct { const char* name; size_t stack_offset; } Variable; DECLARE_HASHMAP_TYPE(Variable, const char*)
typedef struct { const char* name; VariableHashMap vars; } Procedure; DECLARE_HASHMAP_TYPE(Procedure, const char*)

bool CompareVariableKey(Variable* var, const char* key) { return var->name == key || strcmp(var->name, key) == 0; }
bool CompareProcedureKey(Procedure* proc, const char* key) { return proc->name == key || strcmp(proc->name, key) == 0; }

void Procedure_Initialize(Procedure* self, const char* name)
{
    VariableHashMap_Intialize(&self->vars);
    self->name = name;
}

char* Register_ToString[] = { "<error>", "rax", "rbx", "rcx", "rdx" };
enum Register
{
    Register_None,
    Register_Rax,
    Register_Rbx,
    Register_Rcx,
    Register_Rdx,
    Register__Last
};

const char* Register__32(const char* regstr)
{
    if(!regstr) return regstr;
    int l = strlen(regstr);
    if(l < 3) return NULL; // x86, no >=32bit register exists with a name less than 3 chars.

    /**/ if(regstr[0] == 'r') // 64 bit register.
    {
        switch(regstr[1])
        {
        case 'a': return "eax";
        case 'b': return "ebx";
        case 'c': return "ecx";
        case 'd': return "edx";
        default: return NULL;
        }
    }
    else if(regstr[0] == 'e') // 32-bit register, do nothing.
        return regstr;

    return NULL;
}

// TODO: IR

typedef struct
{
    FILE* output;
    int indent, tab_width;

    char* pref_out_reg;
    bool alloc_ret_reg;
    size_t last_stack_offset;
    bool used_regs[Register__Last];
    bool ret_written;

    ProcedureHashMap procs;
    Procedure* current_proc; // TODO: VariableContext stack
} Compiler;
bool Compiler_IsDebug = false;

void Compiler_Initialize(Compiler* self, const char* filename)
{
    // printf("Compiler_IsDebug = %s\n", Compiler_IsDebug ? "yes" : "no");
    self->output = Compiler_IsDebug ? stderr : fopen(filename, "w");
    self->indent = 0;
    self->tab_width = 4;
    self->pref_out_reg = Register_ToString[Register_None];
    self->current_proc = NULL;
    self->alloc_ret_reg = false;
    self->last_stack_offset = 4;
    self->ret_written = false;
    memset(self->used_regs, 0, sizeof(bool) * Register__Last);
    ProcedureHashMap_Intialize(&self->procs);
}

void Compiler__Error(Compiler* self, const char* msg)
{
    fprintf(stderr, "\033[0;31mError:\033[0;0m %s\n", msg);
}

char* Compiler__StackOffsetString(size_t offset)
{
    char* m = malloc(32);
    snprintf(m, 32, "qword ptr [rbp - %zu]", offset);
    return m;
}

void Compiler__Indent(Compiler* self)
{
    for(int i = 0; i < self->indent * self->tab_width; ++i)
        fputc(' ', self->output);
}

void Compiler__End(Compiler* self) { self->indent--; fputc('\n', self->output); }
void Compiler__Begin(Compiler* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    Compiler__Indent(self);
    vfprintf(self->output, fmt, va);
    fputc('\n', self->output);
    self->indent++;

    va_end(va);
}
void Compiler__Write(Compiler* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    Compiler__Indent(self);
    vfprintf(self->output, fmt, va);
    fputc('\n', self->output);

    va_end(va);
}
void Compiler__WriteNoIndent(Compiler* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);

    vfprintf(self->output, fmt, va);
    fputc('\n', self->output);

    va_end(va);
}

enum Register Compiler__FirstFreeReg(Compiler* self)
{
    for(int i = 1; i < Register__Last; ++i)
        if(!self->used_regs[i]) return i;
    return 0;
}

enum Register Compiler__PrefOutRegEnum(Compiler* self)
{
    return StringEqual(self->pref_out_reg, Register_ToString[Register_None])
        ? Compiler__FirstFreeReg(self)
        : Register_None;
}


char* Compiler__PrefOutReg(Compiler* self)
{
    return StringEqual(self->pref_out_reg, Register_ToString[Register_None])
        ? Register_ToString[Compiler__FirstFreeReg(self)]
        : self->pref_out_reg;
}


void Compiler__WriteBinInstr(Compiler* self, const char* ins, const char* dst, const char* src)
{
    if(StringEqual(dst, src)) return;
    if(StringStartsWith(dst, "qword ptr") && StringStartsWith(src, "qword ptr"))
    {
        enum Register r = Compiler__FirstFreeReg(self);
        Compiler__Write(self, "%s %s, %s", ins, Register_ToString[r], src);
        Compiler__Write(self, "%s %s, %s", ins, dst, Register_ToString[r]);
    }
    else
    {
        Compiler__Write(self, "%s %s, %s", ins, dst, src);
    }
}

// ??
void Compiler__WriteSyscall_Exit(Compiler* self, char* retcode)
{
#if WLANG_TARGET == WLANG_TARGET_DARWIN
    // if(!StringEqual(retcode, "ebx") && !StringEqual(retcode, "rbx")) Compiler__Write(self, "mov ebx, %s", Register__32(retcode));
    Compiler__Write(self, "mov eax, 0x%x", WLANG_SYSCALL__DARWIN__EXIT);
    Compiler__Write(self, "syscall");
#elif WLANG_TARGET == WLANG_TARGET_LINUX
    Compiler__WriteBinInstr(self, "mov", "rdi", retcode);
    Compiler__Write(self, "mov eax, 0x%x", WLANG_SYSCALL__LINUX__EXIT);
    Compiler__Write(self, "syscall");
#endif
}

void Compiler_WriteHeaders(Compiler* self)
{
    Compiler__WriteNoIndent(self, ".intel_syntax noprefix");
    Compiler__WriteNoIndent(self, ".global _start");
    Compiler__Begin(self, "_start:");
        Compiler__Write(self, "call _main");
        Compiler__Write(self, "mov rdi, rax");
        Compiler__WriteSyscall_Exit(self, "rdi");
    Compiler__End(self);
}

bool Compiler__IsAssignable(Compiler* self, const AstNode* node) { return true; }

char* Compiler_CompileNode(Compiler* self, const AstNode* node)
{
    // printf("\033[0;32mCompiling %s...\033[0;0m\n", NodeType_ToString(node->type));
    switch(node->type)
    {
        case NodeType_Proc:
        {
            Procedure proc; Procedure_Initialize(&proc, ((AstNode_Proc*)node)->name);
            ProcedureHashMap_PushRef(&self->procs, &proc);
            self->current_proc = &self->procs.data[self->procs.count - 1];

            Compiler__WriteNoIndent(self, ".global _%s", ((AstNode_Proc*)node)->name);
            Compiler__Begin(self, "_%s:", ((AstNode_Proc*)node)->name);
            // TODO: Arguments.
            Compiler__Write(self, "push rbp");
            Compiler__Write(self, "mov rbp, rsp");
            self->pref_out_reg = Register_ToString[Register_None];
            char* tmp = Compiler_CompileNode(self, ((AstNode_Proc*)node)->body);
            if(self->alloc_ret_reg) free(tmp);
            if(!self->ret_written)
            {
                Compiler__Write(self, "pop rbp");
                Compiler__Write(self, "ret");
            }
            Compiler__End(self);
        } break;
        case NodeType_Block:
        {
            char* p = Register_ToString[Register_None];
            self->alloc_ret_reg = false;
            for(size_t i = 0; i < ((AstNode_Block*)node)->nodes.count; ++i)
            {
                self->ret_written = false;
                self->pref_out_reg = Register_ToString[Register_None];
                p = Compiler_CompileNode(self, ((AstNode_Block*)node)->nodes.data[i]);
                if(i != ((AstNode_Block*)node)->nodes.count - 1 && self->alloc_ret_reg)
                { self->alloc_ret_reg = false; free(p); }
            }

            self->alloc_ret_reg = self->alloc_ret_reg;
            return p;
        } break;
        case NodeType_Int:
        {
            char* p = Compiler__PrefOutReg(self);
            Compiler__Write(self, "mov %s, %ld", p, ((AstNode_Int*)node)->value);
            self->alloc_ret_reg = false;
            return p;
        } break;
        case NodeType_Decl:
        {
            if(self->current_proc == NULL) { Compiler__Error(self, "Global variables are not supported."); break; }
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

                self->pref_out_reg = offset_string;
                char* tmp = Compiler_CompileNode(self, (AstNode*)s);
                if(self->alloc_ret_reg) free(tmp);

                free(offset_string);
                AstNode_BinOp_Free(s);
            }

            self->alloc_ret_reg = true;
            return Compiler__StackOffsetString(v_stack_offset);
        } break;
        case NodeType_Iden:
        {
            if(self->current_proc == NULL) { Compiler__Error(self, "Global variables are not supported."); break; }
            Variable* v = VariableHashMap_Find(&self->current_proc->vars, ((AstNode_Iden*)node)->iden, &CompareVariableKey);
            char* p = Compiler__PrefOutReg(self);
            char* loc = Compiler__StackOffsetString(v->stack_offset);
            Compiler__WriteBinInstr(self, "mov", p, loc);
            self->alloc_ret_reg = false;
            return p;
        } break;
        case NodeType_BinOp:
        {
            char* p = Compiler__PrefOutReg(self); (void)p;
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
                    char* val = Compiler_CompileNode(self, rhs);
                    if(self->alloc_ret_reg) free(val);

                    self->alloc_ret_reg = true;
                    return var_loc;
                } break;
                case BinOpType_Add:
                {
                    enum Register r1 = Compiler__FirstFreeReg(self);
                    self->pref_out_reg = Register_ToString[r1];
                    char* n1 = Compiler_CompileNode(self, lhs);
                    bool al_n1 = self->alloc_ret_reg;
                    self->used_regs[r1] = true; // TODO!!!

                    enum Register r2 = Compiler__FirstFreeReg(self);
                    self->pref_out_reg = Register_ToString[r2];
                    char* n2 = Compiler_CompileNode(self, rhs);
                    bool al_n2 = self->alloc_ret_reg;
                    self->used_regs[r2] = true;

                    // printf("Writing add instr %s, %s\n", n1, n2);
                    Compiler__WriteBinInstr(self, "add", n1, n2);

                    self->used_regs[r1] = false;
                    self->used_regs[r2] = false;
                    if(al_n2) free(n2);
                    self->alloc_ret_reg = al_n1;
                    return n1;
                }
                default: Compiler__Error(self, "Binary operation is not yet implemented!");
            }
        } break;
        case NodeType_Return:
        {
            self->pref_out_reg = Register_ToString[Register_Rax];
            char* p = Compiler_CompileNode(self, ((AstNode_Return*)node)->value);
            Compiler__WriteBinInstr(self, "mov", Register_ToString[Register_Rax], p);
            if(self->alloc_ret_reg) free(p);
            Compiler__Write(self, "pop rbp");
            Compiler__Write(self, "ret");
            self->ret_written = true;
        } break;
        default: Compiler__Write(self, "# Did not compile node of type %s.", NodeType_ToString2(node->type)); break;
    }

    self->alloc_ret_reg = false;
    return Register_ToString[Register_None];
}

void Compiler_Free(Compiler* self)
{
    fclose(self->output);
}

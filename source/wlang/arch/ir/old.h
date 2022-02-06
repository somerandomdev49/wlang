
//:==========----------- Tree-Walking Iterpreter -----------==========://


// void AstNode_Show(AstNode* node, int indent)
// {
//     for(int i = 0; i < indent * 2; ++i) fputc(' ', stdout);
//     if(!node) { printf("Nil Node!\n"); return; }
//     switch(node->type)
//     {
//     // case NodeType_Error: AstNode_Error_Free(node); break;
//     case NodeType_Proc: AstNode_Proc_Show((AstNode_Proc*)node, indent); break;
//     case NodeType_Iden: AstNode_Iden_Show((AstNode_Iden*)node, indent); break;
//     case NodeType_Int: AstNode_Int_Show((AstNode_Int*)node, indent); break;
//     // case NodeType_Float: AstNode_Float_Free(node); break;
//     // case NodeType_StringLit: AstNode_StringLit_Free(node); break;
//     case NodeType_Return: AstNode_Return_Show((AstNode_Return*)node, indent); break;
//     case NodeType_Block: AstNode_Block_Show((AstNode_Block*)node, indent); break;
//     case NodeType_BinOp: AstNode_BinOp_Show((AstNode_BinOp*)node, indent); break;
//     default: printf("Node: %s\n", NodeType_ToString(node->type));break;
//     }
// }

//:==========----------- IR Code Generation -----------==========://


// typedef struct { char* name; IR_V var; } IR_Var;
// DECLARE_HASHMAP_TYPE(IR_Var, const char*);
// typedef struct
// {
//     char* name;
//     IR_VarHashMap locals;
//     IR_V next_local;
//     IR_InstrList code;
// } IR_Proc;
// DECLARE_HASHMAP_TYPE(IR_Proc, const char*);

// bool IR_ProcHashMap_CompareKey(IR_Proc* proc, const char* key) { return StringEqual(proc->name, key); }
// bool IR_VarHashMap_CompareKey(IR_Var* var, const char* key) { return StringEqual(var->name, key); }

// IR_Var IR_Var_Create(const char* name, IR_V var) { return (IR_Var){ name ? AllocStringCopy(name) : NULL, var }; }
// void IR_Var_Free(IR_Var* self) { free(self->name); }

// void IR_Proc_Initialize(IR_Proc* proc, const char* name)
// {
//     proc->name = AllocStringCopy(name);
//     proc->next_local = 0;
//     IR_VarHashMap_Initialize(&proc->locals);
//     IR_InstrList_Initialize(&proc->code);
// }

// IR_Var IR_Proc_NextVar(IR_Proc* self, const char* name)
// { return IR_Var_Create(name, self->next_local++); }

// IR_Var* IR_Proc_GetVar(IR_Proc* self, const char* name)
// { return IR_VarHashMap_Find(&self->locals, name, &IR_VarHashMap_CompareKey); }

// void IR_Proc_Free(IR_Proc* proc)
// {
//     free(proc->name);
//     IR_VarHashMap_ForEachRef(&proc->locals, &IR_Var_Free);
//     IR_InstrList_Free(&proc->code);
// }

// typedef struct { IR_ProcHashMap procs; } IR_Builder;

// void IR_Builder_Initialize(IR_Builder* self)
// {
//     IR_ProcHashMap_Initialize(&self->procs);
// }

// void IR_Builder_Free(IR_Builder* self)
// {
//     IR_ProcHashMap_ForEachRef(&self->procs, &IR_Proc_Free);
//     IR_ProcHashMap_Free(&self->procs);
// }

// void IR_Proc__EmitV(IR_Proc* self, enum IR_OpType op, IR_V val)
// {
//     IR_Instr instr = { op, 0, val, 0, 0 };
//     IR_InstrList_PushRef(&self->code, &instr);
// }
// void IR_Proc__Emit1(IR_Proc* self, enum IR_OpType op, IR_V res, IR_V val)
// {
//     IR_Instr instr = { op, res, val, 0, 0 };
//     IR_InstrList_PushRef(&self->code, &instr);
// }
// void IR_Proc__Emit2(IR_Proc* self, enum IR_OpType op, IR_V res, IR_V lhs, IR_V rhs)
// {
//     IR_Instr instr = { op, res, lhs, rhs, 0 };
//     IR_InstrList_PushRef(&self->code, &instr);
// }
// void IR_Proc__EmitL(IR_Proc* self, enum IR_OpType op, IR_L lbl, IR_V lhs, IR_V rhs)
// {
//     IR_Instr instr = { op, 0, lhs, rhs, lbl };
//     IR_InstrList_PushRef(&self->code, &instr);
// }

// void IR_Builder__Error(IR_Builder* self, const char* msg)
// {
//     fprintf(stderr, "\033[0;31mError:\033[0;0m %s\n", msg);
// }
// void IR_Builder__ErrorFormat(IR_Builder* self, const char* fmt, ...)
// {
//     va_list va;
//     va_start(va, fmt);
//     fprintf(stderr, "\033[0;31mError:\033[0;0m\n");
//     vfprintf(stderr, fmt, va);
//     fputc('\n', stderr);
//     va_end(va);
// }

// void IR_Builder__Fatal(IR_Builder* self)
// {
//     fprintf(stderr, "\033[0;31mFatal Error![0;0m\n");
//     exit(1);
// }

// IR_V IR_Proc__Target(IR_Proc* self, IR_V target)
// {
//     if(target != (IR_V)-1) return target;
//     return self->next_local++;
// }

// IR_Value IR_Builder_BuildNode(IR_Builder* self, AstNode* node, IR_Proc* proc, IR_V target) // TODO: IR_Locals
// {
//     switch(node->type)
//     {
//         case NodeType_Return:
//         {
//             IR_V o = IR_Builder_BuildNode(self, ((AstNode_Return*)node)->value, proc, (IR_V)-1);
//             IR_Proc__EmitV(proc, IR_OpType_Ret, o);
//         } break;
//         case NodeType_Block:
//         {
//             AstNodePtrList* l = &((AstNode_Block*)node)->nodes;
//             for(size_t i = 0; i < l->count; ++i)
//                 IR_Builder_BuildNode(self, l->data[i], proc, i == l->count - 1 ? (target = IR_Proc__Target(proc, target)) : (IR_V)-1);
//         } break;
//         case NodeType_Int: IR_Proc__Emit1(proc, IR_OpType_Cpy, target = IR_Proc__Target(proc, target), ((AstNode_Int*)node)->value); break;
//         case NodeType_Iden:
//         {
//             IR_Var* var = NULL;
//             /* var = IR_Build__GetGlobal(((AstNode_Iden*)node)->iden) */
//             if(!var) // Not a global.
//             {
//                 if(!proc)
//                 {
//                     IR_Builder__Error(self, "Internal: `proc` is NULL on NodeType_Iden and no global var found.");
//                     IR_Builder__Fatal(self);
//                 }

//                 var = IR_Proc_GetVar(proc, ((AstNode_Iden*)node)->iden);
//                 if(!var)
//                 {
//                     IR_Builder__ErrorFormat(self, "No such variable: '%s'", ((AstNode_Iden*)node)->iden);
//                 }
//             }
//             IR_Proc__Emit1(proc, IR_OpType_Cpy, target = IR_Proc__Target(proc, target), ((AstNode_Int*)node)->value);
//         } break;
//         default: IR_Builder__ErrorFormat(self, "Failed to build IR from '%s'", NodeType_ToString2(node->type)); break;
//     }
//     return target;
// }

// const char *IR_Builder__FormatValue(IR_Builder* self, IR_Value* value)
// {

// }

// void IR_Builder__DumpInstr(IR_Builder* self, FILE* output, IR_Instr* instr)
// {
//     switch(instr->type)
//     {
//     case IR_OpType_Add: // +
//     case IR_OpType_Sub: // -
//     case IR_OpType_Mul: // *
//     case IR_OpType_Div: // /
//     case IR_OpType_Eql: // ==
//     case IR_OpType_Neq: // !=
//     case IR_OpType_Grt: // >
//     case IR_OpType_Lst: // <
//     case IR_OpType_Geq: // >=
//     case IR_OpType_Leq: // <=
//     case IR_OpType_And: // &&
//     case IR_OpType_Cor: // ||
//     case IR_OpType_Bnd: // &
//     case IR_OpType_Bor: // |
//     case IR_OpType_Mod: // %
//     case IR_OpType_Xor: // ^
//         fprintf(output, "\t%lu =  %s%lu\n",
//             instr->res,
//             IR_OpType_ToString2(instr->type),
//             instr->lhs);
//         break;

//     case IR_OpType_Neg: // -
//     case IR_OpType_Not: // !
//     case IR_OpType_Bno: // ~
//         fprintf(output, "\t%lu = %s %lu\n",
//             instr->res,
//             IR_OpType_ToString2(instr->type),
//             instr->lhs);
//         break;

//     case IR_OpType_Cpy:  // cpy X <- Y
//         fprintf(output, "\t%lu = %lu\n", instr->res, instr->lhs);
//         break;

//     case IR_OpType_SetMem: // setmem ADDR <- X
//         fprintf(output, "\t[%lu] = %lu\n", instr->res, instr->lhs);
//         break;

//     case IR_OpType_SetOff: // setoff OFFSET <- X
//         fprintf(output, "\t[-%lu] = %lu\n", instr->res, instr->lhs);
//         break;

//     case IR_OpType_Ret: // ret X
//         fprintf(output, "\tret %lu\n", instr->lhs);
//         break;

//     default:
//         fprintf(output, "\t%lu = %lu %s %lu (->%lu)\n",
//             instr->res, instr->lhs, IR_OpType_ToString(instr->type), instr->rhs, instr->lbl);
//         break;
//     }
// }

// void IR_Builder_Dump(IR_Builder* self, FILE* output)
// {
//     int line = 1;
//     for(size_t pi = 0; pi < self->procs.count; ++pi)
//     {
//         IR_Proc* proc = &self->procs.data[pi];

//         fprintf(output, "\nproc %s: \t\t## {%zu}\n", proc->name, proc->code.count);
//         line += 2;

//         for(size_t i = 0; i < proc->code.count; ++i)
//         {
//             IR_Builder__DumpInstr(self, output, &proc->code.data[i]);
//             line += 1;
//         }
//     }
//     fprintf(output, "\n -- END --\n");
// }

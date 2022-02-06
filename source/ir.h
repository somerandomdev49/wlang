#ifndef WLANG_HEADER_IR_
#define WLANG_HEADER_IR_
#include "type.h"

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
    IR_OpType_Mov, // cpy X <- Y
    IR_OpType_SetMem, // setmem ADDR <- X
    IR_OpType_SetOff, // setoff OFFSET <- X
    IR_OpType_Call, // ret X
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
enum IR_ValueType
{
    IR_ValueType_Imm,
    IR_ValueType_Reg,
    IR_ValueType_Mem,
};

typedef struct { enum IR_ValueType type; IR_V value; } IR_Value;
typedef struct { IR_OpType type; IR_Value lhs, rhs; size_t label; } IR_Instr;
DECLARE_LIST_TYPE(IR_Instr);

#define IR_VALUE_MACRO_(V, TYP) ((IR_Value){ .type = IR_ValueType_##TYP, (V) })
#define IR_VALUE_REG(REG) IR_VALUE_MACRO_(Reg, REG)
#define IR_VALUE_IMM(IMM) IR_VALUE_MACRO_(Imm, IMM)
#define IR_VALUE_MEM(MEM) IR_VALUE_MACRO_(Mem, MEM)

#endif

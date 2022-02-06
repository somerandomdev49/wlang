#include <wlang/ir.h>

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

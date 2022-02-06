#ifndef WLANG_HEADER_GEN_
#define WLANG_HEADER_GEN_
#include <wlang/util.h>
#include <wlang/type.h>
#include <wlang/common.h>

typedef struct { char* line; int indent; } AssemblyLine;
inline void AssemblyLine_Initialize(AssemblyLine* self, int indent)
{
    self->line = NULL;
    self->indent = indent;
}
inline void AssemblyLine_Free(AssemblyLine* self)
{
    if(self->line) free(self->line);
}

DECLARE_LIST_TYPE(AssemblyLine)

typedef struct
{
    AssemblyLineList lines;
    int tab_width;
} AssemblyOutput;

inline void AssemblyOutput_Initialize(AssemblyOutput* self, int tab_width)
{
    self->tab_width = tab_width;
    AssemblyLineList_Initialize(&self->lines);
}

inline void AssemblyOutput_Free(AssemblyOutput* self)
{
    AssemblyLineList_ForEachRef(&self->lines, &AssemblyLine_Free);
    AssemblyLineList_Free(&self->lines);
}

void AssemblyOutput_NewLine(AssemblyOutput* self, int indent);
AssemblyLine* AssemblyOutput_Line(AssemblyOutput* self);
void AssemblyOutput_Flush(AssemblyOutput* self, FILE* target);

typedef struct
{
    int indent, tab_width;
    AssemblyOutput output;
} AssemblyGenerator;

inline void AssemblyGenerator_Initialize(AssemblyGenerator* self)
{
    self->indent = 0;
    self->tab_width = 4;
    AssemblyOutput_Initialize(&self->output, self->tab_width);
}

inline void AssemblyGenerator_Free(AssemblyGenerator* self)
{
    AssemblyOutput_Free(&self->output);
}

inline void AssemblyGenerator_End(AssemblyGenerator* self) { self->indent--; }

AssemblyLine* AssemblyGenerator_WriteIndentV(AssemblyGenerator* self, int indent, const char *fmt, va_list va);
void AssemblyGenerator_Write(AssemblyGenerator* self, const char* fmt, ...);
void AssemblyGenerator_WriteNoIndent(AssemblyGenerator* self, const char* fmt, ...);
void AssemblyGenerator_Begin(AssemblyGenerator* self, const char* fmt, ...);

#endif

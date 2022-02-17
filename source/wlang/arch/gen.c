#include <wlang/arch/gen.h>
#include <wlang/pim.h>

DEFINE_LIST_TYPE(AssemblyLine)

void AssemblyOutput_NewLine(AssemblyOutput* self, int indent)
{
    AssemblyLine nl;
    AssemblyLine_Initialize(&nl, indent);
    AssemblyLineList_PushRef(&self->lines, PIM_MOVE(&nl));
}

AssemblyLine* AssemblyOutput_Line(AssemblyOutput* self)
{
    return &self->lines.data[self->lines.count - 1];
}

static void AssemblyOutput__FlushLine(AssemblyOutput* self, AssemblyLine* line, FILE* target)
{
    if(!line) return;

    for(int i = 0; i < line->indent * self->tab_width; ++i)
        fputc(' ', target);
    
    if(line->line)
        fprintf(target, "%s\n", line->line);
}

void AssemblyOutput_Flush(AssemblyOutput* self, FILE* target)
{
    for(size_t i = 0; i < self->lines.count; ++i)
        AssemblyOutput__FlushLine(self, &self->lines.data[i], target);
    
    AssemblyLineList_Free(&self->lines);
}

char* AssemblyGenerator__StackOffsetString(size_t offset)
{
    char* m = malloc(32);
    snprintf(m, 32, "qword ptr [rbp - %zu]", offset);
    return m;
}

AssemblyLine* AssemblyGenerator_WriteIndentV(AssemblyGenerator* self, int indent, const char *fmt, va_list va)
{
    va_list va2;
    va_copy(va2, va);

    AssemblyOutput_NewLine(&self->output, indent);
    size_t nsz = vsnprintf(NULL, 0, fmt, va);
    AssemblyOutput_Line(&self->output)->line = malloc(nsz + 1);
    vsnprintf(AssemblyOutput_Line(&self->output)->line, nsz + 1, fmt, va2);

    va_end(va2);
    return AssemblyOutput_Line(&self->output);
}
void AssemblyGenerator_Write(AssemblyGenerator* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    AssemblyGenerator__WriteIndentV(self, self->indent, fmt, va);
    va_end(va);
}
void AssemblyGenerator_WriteNoIndent(AssemblyGenerator* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    AssemblyGenerator_WriteIndentV(self, 0, fmt, va);
    va_end(va);
}

void AssemblyGenerator_Begin(AssemblyGenerator* self, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    AssemblyGenerator_WriteIndentV(self, self->indent, fmt, va);
    self->indent++;

    va_end(va);
}

#include <stdio.h>
#include <stddef.h>

size_t strcspn(const char *src, const char *delim)
{
    size_t n = 0;
    for(const char *d = delim; *src; ++src, d = delim)
    {
        for(; *d; ++d)
            if(*src == *d)
                return n;
        ++n;
    }
    
    return n;
}

char *strtok(char *s, const char *delim)
{
    static char *p;
    if(!delim) return NULL; // error
    p = s ? s : p + 1; // could optimize?
    s = p;
    p += strcspn(p, delim);
    *p = 0;
    return s;
}

const char original[] = "echo abcdef gh";

void test(char *s, char *actual)
{
    printf("'%s'\nbytes: ", strtok(s, " "));
    for(int i = 0; i < sizeof(original); ++i)
        printf("%xh ", actual[i]);
    putc('\n', stdout);
    putc('\n', stdout);
}

int main()
{   
    char copy[sizeof(original)];
    for(int i = 0; i < sizeof(original); ++i) copy[i] = original[i];
    test(copy, copy);
    test(NULL, copy);
    test(NULL, copy);
}



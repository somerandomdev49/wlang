# To Do:

## Things that must be implemented

Three-Address Code IR.
Register Allocation for assembly.
Rewrite the compiler.

## Things that aren't working, but should

bad output with
```c
proc main()
{
    int a = 5;
    int b = a + 1;
    return b + a;
}
```

<!-- ```sh
# Doesn't work!
mov dword ptr [rbp - A], dword ptr [rbp - B]

# Should instead be:
mov TMPREG, dword ptr [rbp - B]
mov dword ptr [rbp - A], TMPREG
``` -->

<!-- Also when assembler/linker fails to find a file, no error. (-d flag) -->

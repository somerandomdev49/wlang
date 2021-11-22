# To Do:

## Things that must be implemented

Three-Address Code IR.
Register Allocation for assembly.
Rewrite the compiler.

## Things that aren't working, but should

<!-- ```sh
# Doesn't work!
mov dword ptr [rbp - A], dword ptr [rbp - B]

# Should instead be:
mov TMPREG, dword ptr [rbp - B]
mov dword ptr [rbp - A], TMPREG
``` -->

<!-- Also when assembler/linker fails to find a file, no error. (-d flag) -->

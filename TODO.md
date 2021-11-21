# Things that aren't working, but should

```sh
# Doesn't work!
mov dword ptr [rbp - A], dword ptr [rbp - B]

# Should instead be:
mov TMPREG, dword ptr [rbp - B]
mov dword ptr [rbp - A], TMPREG
```

Also when assembler/linker fail to find a file, no error. (-d flag)

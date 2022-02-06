#ifndef WLANG_PIM_HEADER_
#define WLANG_PIM_HEADER_
//:==========----------- Pointer Interface Macros -----------==========://

// PIM: Pointer is moved to the target and is owned by it.
#define PIM_MOVE(X) (X)

// PIM: Pointer is owned by the structure that contains it.
#define PIM_OWN

// PIM: Pointer is a reference to somewhere.
#define PIM_REF

#endif


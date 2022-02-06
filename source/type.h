#ifndef WLANG_HEADER_TYPE_
#define WLANG_HEADER_TYPE_
#include <stdlib.h>
#include <string.h>

//:==========----------- Generic Types -----------==========://

#define LIST(T) T##List
#define LISTFN(T, NAME) T##List_##NAME
#define DECLARE_LIST_TYPE(T) \
    typedef struct {\
        T* data; \
        size_t count; \
    } LIST(T); \
    void LISTFN(T, Initialize)(LIST(T)* self); \
    void LISTFN(T, _IncrSize)(LIST(T)* self); \
    T* LISTFN(T, PushValue)(LIST(T)* self, T value); \
    T* LISTFN(T, PushRef)(LIST(T)* self, T* value); \
    void LISTFN(T, Free)(LIST(T)* self); \
    void LISTFN(T, ForEachRef)(LIST(T)* self, void (*func)(T*)); \
    void LISTFN(T, ForEach)(LIST(T)* self, void (*func)(T));

#define DEFINE_LIST_TYPE(T) \
    void LISTFN(T, Initialize)(LIST(T)* self) \
    { self->count = 0; self->data = NULL; } \
    void LISTFN(T, _IncrSize)(LIST(T)* self) \
    { if(self->count == 0) self->data = malloc(sizeof(T) * (++self->count)); \
      else self->data = realloc(self->data, sizeof(T) * (++self->count)); } \
    T* LISTFN(T, PushValue)(LIST(T)* self, T value) \
    { LISTFN(T, _IncrSize)(self); self->data[self->count - 1] = value; return &self->data[self->count - 1]; } \
    T* LISTFN(T, PushRef)(LIST(T)* self, T* value) \
    { LISTFN(T, _IncrSize)(self); self->data[self->count - 1] = *value; return &self->data[self->count - 1]; } \
    void LISTFN(T, Free)(LIST(T)* self) \
    { free(self->data); self->count = 0; self->data = NULL; } \
    void LISTFN(T, ForEachRef)(LIST(T)* self, void (*func)(T*)) \
    { for(size_t i = 0; i < self->count; ++i) func(&self->data[i]); } \
    void LISTFN(T, ForEach)(LIST(T)* self, void (*func)(T)) \
    { for(size_t i = 0; i < self->count; ++i) func(self->data[i]); }

// TODO: This is not a Hash Map, just a list with a linear search!
#define HASHMAP(T) T##HashMap
#define HASHMAPFN(T, NAME) T##HashMap_##NAME
#define DECLARE_HASHMAP_TYPE(T, K) \
    typedef struct {\
        PIM_OWN T* data; \
        size_t count; \
    } HASHMAP(T); \
    void HASHMAPFN(T, Initialize)(HASHMAP(T)* self); \
    void HASHMAPFN(T, _IncrSize)(HASHMAP(T)* self); \
    T* HASHMAPFN(T, PushValue)(HASHMAP(T)* self, T value); \
    T* HASHMAPFN(T, PushRef)(HASHMAP(T)* self, T* value); \
    void HASHMAPFN(T, Free)(HASHMAP(T)* self); \
    void HASHMAPFN(T, ForEachRef)(HASHMAP(T)* self, void (*func)(T*)); \
    void HASHMAPFN(T, ForEach)(HASHMAP(T)* self, void (*func)(T)); \
    T* HASHMAPFN(T, Find)(HASHMAP(T)* self, K key, bool (*cmp)(T*, K));

#define DEFINE_HASHMAP_TYPE(T, K) \
    void HASHMAPFN(T, Initialize)(HASHMAP(T)* self) \
    { self->count = 0; self->data = NULL; } \
    void HASHMAPFN(T, _IncrSize)(HASHMAP(T)* self) \
    { if(self->count == 0) self->data = malloc(sizeof(T) * (++self->count)); \
      else self->data = realloc(self->data, sizeof(T) * (++self->count)); } \
    T* HASHMAPFN(T, PushValue)(HASHMAP(T)* self, T value) \
    { HASHMAPFN(T, _IncrSize)(self); self->data[self->count - 1] = value; return &self->data[self->count - 1]; } \
    T* HASHMAPFN(T, PushRef)(HASHMAP(T)* self, T* value) \
    { HASHMAPFN(T, _IncrSize)(self); self->data[self->count - 1] = *value; return &self->data[self->count - 1]; } \
    void HASHMAPFN(T, Free)(HASHMAP(T)* self) \
    { free(self->data); self->count = 0; self->data = NULL; } \
    void HASHMAPFN(T, ForEachRef)(HASHMAP(T)* self, void (*func)(T*)) \
    { for(size_t i = 0; i < self->count; ++i) func(&self->data[i]); } \
    void HASHMAPFN(T, ForEach)(HASHMAP(T)* self, void (*func)(T)) \
    { for(size_t i = 0; i < self->count; ++i) func(self->data[i]); } \
    T* HASHMAPFN(T, Find)(HASHMAP(T)* self, K key, bool (*cmp)(T*, K)) \
    { for(size_t i = 0; i < self->count; ++i) if(cmp(&self->data[i], key)) \
      return &self->data[i]; return NULL; }


typedef char* CStr;
void CStr_Free(char* s) { free(s); }
DECLARE_LIST_TYPE(CStr);

typedef struct {
    char* data;
    size_t size;
} DynamicString;


void DynamicString_Initialize(DynamicString* self)
{ self->data = NULL; self->size = 0; }

void DynamicString__IncrSize(DynamicString* self)
{
    if(self->size == 0) self->data = malloc(++self->size + 1);
    else self->data = realloc(self->data, ++self->size + 1);
    self->data[self->size] = '\0';
}
void DynamicString_Add(DynamicString* self, char value) { DynamicString__IncrSize(self); self->data[self->size - 1] = value; }
void DynamicString_Free(DynamicString* self) { free(self->data); self->size = 0; self->data = NULL; }
void DynamicString_CopyFrom(DynamicString* self, const char* source, size_t size) {
    self->size = size ? size : strlen(source);
    if(self->data != NULL) free(self->data);
    self->data = malloc(self->size + 1);
    memcpy(self->data, source, self->size + 1);
}

#endif


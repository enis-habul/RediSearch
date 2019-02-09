#ifndef SRC_REDISEARCH_API_H_
#define SRC_REDISEARCH_API_H_

#include "redismodule.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REDISEARCH_CAPI_VERSION 1

#define MODULE_API_FUNC(T, N) extern T(*N)

typedef struct IndexSpec RSIndex;
typedef struct FieldSpec RSField;
typedef struct Document RSDoc;
typedef struct RSQueryNode RSQNode;
typedef struct indexIterator RSResultsIterator;

#define RSVALTYPE_NOTFOUND 0
#define RSVALTYPE_STRING 1
#define RSVALTYPE_DOUBLE 2
typedef int (*RSGetValueCallback)(void* ctx, const char* fieldName, const void* id, char** strVal,
                                  double* doubleVal);

MODULE_API_FUNC(int, RediSearch_GetCApiVersion)();

MODULE_API_FUNC(RSIndex*, RediSearch_CreateSpec)
(const char* name, RSGetValueCallback getValue, void* getValueCtx);

MODULE_API_FUNC(RSField*, RediSearch_CreateTextField)(RSIndex* sp, const char* name);

MODULE_API_FUNC(void, RediSearch_TextFieldSetWeight)(RSField* fs, double w);

MODULE_API_FUNC(void, RediSearch_TextFieldNoStemming)(RSField* fs);

MODULE_API_FUNC(void, RediSearch_TextFieldPhonetic)(RSField* fs, RSIndex* sp);

MODULE_API_FUNC(RSField*, RediSearch_CreateGeoField)(RSIndex* sp, const char* name);

MODULE_API_FUNC(RSField*, RediSearch_CreateNumericField)(RSIndex* sp, const char* name);

MODULE_API_FUNC(RSField*, RediSearch_CreateTagField)(RSIndex* sp, const char* name);

MODULE_API_FUNC(void, RediSearch_TagSetSeparator)(RSField* fs, char sep);

MODULE_API_FUNC(void, RediSearch_FieldSetSortable)(RSField* fs, RSIndex* sp);

MODULE_API_FUNC(void, RediSearch_FieldSetNoIndex)(RSField* fs);

MODULE_API_FUNC(RSDoc*, RediSearch_CreateDocument)
(const void* docKey, size_t len, double score, const char* lang);

MODULE_API_FUNC(int, RediSearch_DropDocument)(RSIndex* sp, const void* docKey, size_t len);

MODULE_API_FUNC(void, RediSearch_DocumentAddTextField)
(RSDoc* d, const char* fieldName, const char* val);

MODULE_API_FUNC(void, RediSearch_DocumentAddNumericField)
(RSDoc* d, const char* fieldName, double num);

MODULE_API_FUNC(void, RediSearch_SpecAddDocument)(RSIndex* sp, RSDoc* d);

MODULE_API_FUNC(RSQNode*, RediSearch_CreateTokenNode)
(RSIndex* sp, const char* fieldName, const char* token);

MODULE_API_FUNC(RSQNode*, RediSearch_CreateNumericNode)
(RSIndex* sp, const char* field, double max, double min, int includeMax, int includeMin);

MODULE_API_FUNC(RSQNode*, RediSearch_CreatePrefixNode)
(RSIndex* sp, const char* fieldName, const char* s);

MODULE_API_FUNC(RSQNode*, RediSearch_CreateLexRangeNode)
(RSIndex* sp, const char* fieldName, const char* begin, const char* end);

MODULE_API_FUNC(RSQNode*, RediSearch_CreateTagNode)(RSIndex* sp, const char* field);

MODULE_API_FUNC(void, RediSearch_TagNodeAddChild)(RSQNode* qn, RSQNode* child);

MODULE_API_FUNC(RSQNode*, RediSearch_CreateIntersectNode)(RSIndex* sp, int exact);

MODULE_API_FUNC(void, RediSearch_IntersectNodeAddChild)(RSQNode* qn, RSQNode* child);

MODULE_API_FUNC(RSQNode*, RediSearch_CreateUnionNode)(RSIndex* sp);

MODULE_API_FUNC(void, RediSearch_UnionNodeAddChild)(RSQNode* qn, RSQNode* child);

MODULE_API_FUNC(RSResultsIterator*, RediSearch_GetResultsIterator)(RSQNode* qn, RSIndex* sp);

const MODULE_API_FUNC(void*, RediSearch_ResultsIteratorNext)(RSResultsIterator* iter, RSIndex* sp,
                                                             size_t* len);

MODULE_API_FUNC(void, RediSearch_ResultsIteratorFree)(RSResultsIterator* iter);

MODULE_API_FUNC(void, RediSearch_ResultsIteratorReset)(RSResultsIterator* iter);

#define REDISEARCH_MODULE_INIT_FUNCTION(name)                                  \
  if (RedisModule_GetApi("RediSearch_" #name, ((void**)&RediSearch_##name))) { \
    printf("could not initialize RediSearch_" #name "\r\n");                   \
    return REDISMODULE_ERR;                                                    \
  }

#define RS_XAPIFUNC(X)       \
  X(GetCApiVersion)          \
  X(CreateSpec)              \
  X(CreateTextField)         \
  X(TextFieldSetWeight)      \
  X(TextFieldNoStemming)     \
  X(TextFieldPhonetic)       \
  X(CreateGeoField)          \
  X(CreateNumericField)      \
  X(CreateTagField)          \
  X(TagSetSeparator)         \
  X(FieldSetSortable)        \
  X(FieldSetNoIndex)         \
  X(CreateDocument)          \
  X(DropDocument)            \
  X(DocumentAddTextField)    \
  X(DocumentAddNumericField) \
  X(SpecAddDocument)         \
  X(CreateTokenNode)         \
  X(CreateNumericNode)       \
  X(CreatePrefixNode)        \
  X(CreateLexRangeNode)      \
  X(CreateTagNode)           \
  X(TagNodeAddChild)         \
  X(CreateIntersectNode)     \
  X(IntersectNodeAddChild)   \
  X(CreateUnionNode)         \
  X(UnionNodeAddChild)       \
  X(GetResultsIterator)      \
  X(ResultsIteratorNext)     \
  X(ResultsIteratorFree)     \
  X(ResultsIteratorReset)

static bool RediSearch_Initialize() {
  RS_XAPIFUNC(REDISEARCH_MODULE_INIT_FUNCTION);
  if (RediSearch_GetCApiVersion() > REDISEARCH_CAPI_VERSION) {
    return REDISMODULE_ERR;
  }
  return REDISMODULE_OK;
}

#define REDISEARCH__API_INIT_NULL(s) __typeof__(RediSearch_##s) RediSearch_##s = NULL;
#define REDISEARCH_API_INIT_SYMBOLS() RS_XAPIFUNC(REDISEARCH__API_INIT_NULL)

#ifdef __cplusplus
}
#endif
#endif /* SRC_REDISEARCH_API_H_ */

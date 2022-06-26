#pragma once

#include "redismodule.h"
#include "rmutil/sds.h"
#include "query_error.h"

enum RSTimeoutPolicy {
  TimeoutPolicy_Default = 0,  // Defer to global config
  TimeoutPolicy_Return,       // Return what we have on timeout
  TimeoutPolicy_Fail,         // Just fail without returning anything
  TimeoutPolicy_Invalid       // Not a real value
};

enum GCPolicy { GCPolicy_Fork = 0, GCPolicy_Sync };

const char *TimeoutPolicy_ToString(RSTimeoutPolicy);

/**
 * Returns TimeoutPolicy_Invalid if the string could not be parsed
 */
RSTimeoutPolicy TimeoutPolicy_Parse(const char *s, size_t n);

static inline const char *GCPolicy_ToString(GCPolicy policy) {
  switch (policy) {
    case GCPolicy_Sync:
      return "sync";
    case GCPolicy_Fork:
      return "fork";
    default:          // LCOV_EXCL_LINE cannot be reached
      return "huh?";  // LCOV_EXCL_LINE cannot be reached
  }
}

#define RS_MAX_CONFIG_VARS 255
struct RSConfigOptions {
  RSConfigVar vars[RS_MAX_CONFIG_VARS];
  struct RSConfigOptions *next;

  void AddConfigs(RSConfigOptions *dst);
};

/* RSConfig is a global configuration struct for the module, it can be included from each file,
 * and is initialized with user config options during module statrtup */
struct RSConfig{
  // Use concurrent serach (default: 1, disable with SAFEMODE)
  int concurrentMode;
  // If not null, this points at a .so file of an extension we try to load (default: NULL)
  const char *extLoad;
  // If this is set, GC is enabled on all indexes (default: 1, disable with NOGC)
  int enableGC;

  // The minimal number of characters we allow expansion for in a prefix search. Default: 2
  long long minTermPrefix;

  // The maximal number of expansions we allow for a prefix. Default: 200
  long long maxPrefixExpansions;

  // The maximal amount of time a single query can take before timing out, in milliseconds.
  // 0 means unlimited
  long long queryTimeoutMS;

  long long timeoutPolicy;

  // Number of rows to read from a cursor if not specified
  long long cursorReadSize;

  // Maximum idle time for a cursor. Users can use shorter lifespans, but never
  // longer ones
  long long cursorMaxIdle;

  size_t maxDocTableSize;

  size_t searchPoolSize;
  size_t indexPoolSize;
  int poolSizeNoAuto;  // Don't auto-detect pool size

  size_t gcScanSize;
  GCPolicy gcPolicy;

  size_t forkGcRunIntervalSec;
  size_t forkGcRetryInterval;
  size_t forkGcSleepBeforeExit;
  size_t forkGcCleanThreshold;

  size_t minPhoneticTermLen;

  long long maxResultsToUnsortedMode;

  int noMemPool;

  // Chained configuration data
  void *chainedConfig;

  // Path to friso.ini for chinese dictionary file
  const char *frisoIni;

  void DumpProto(const RSConfigOptions *options, const char *name,
                 RedisModuleCtx *ctx, int isHelp) const;

  int SetOption(RSConfigOptions *options, const char *name, RedisModuleString **argv,
                int argc, size_t *offset, QueryError *status);

  sds GetInfoString() const;
};

enum RSConfigVarFlags{
  RSCONFIGVAR_F_IMMUTABLE = 0x01,
  RSCONFIGVAR_F_MODIFIED = 0x02,
  RSCONFIGVAR_F_FLAG = 0x04,
  RSCONFIGVAR_F_SHORTHAND = 0x08
};

struct RSConfigVar {
  const char *name;
  const char *helpText;
  // Whether this configuration option can be modified after initial loading
  int (*setValue)(RSConfig *, ArgsCursor *, QueryError *);
  sds (*getValue)(const RSConfig *);
  uint32_t flags;
};

// global config extern references
extern RSConfig RSGlobalConfig;
extern RSConfigOptions RSGlobalConfigOptions;

/* Read configuration from redis module arguments into the global config object. Return
 * REDISMODULE_ERR and sets an error message if something is invalid */
int ReadConfig(RedisModuleString **argv, int argc, char **err);

#define DEFAULT_DOC_TABLE_SIZE 1000000
#define MAX_DOC_TABLE_SIZE 100000000
#define CONCURRENT_SEARCH_POOL_DEFAULT_SIZE 20
#define CONCURRENT_INDEX_POOL_DEFAULT_SIZE 8
#define CONCURRENT_INDEX_MAX_POOL_SIZE 200  // Maximum number of threads to create
#define GC_SCANSIZE 100
#define DEFAULT_MIN_PHONETIC_TERM_LEN 3
#define DEFAULT_FORK_GC_RUN_INTERVAL 10
#define DEFAULT_MAX_RESULTS_TO_UNSORTED_MODE 1000

// default configuration
#define RS_DEFAULT_CONFIG  \
  {  \
    concurrentMode: 0,  \
    extLoad: NULL,  \
    enableGC: 1,  \
    minTermPrefix: 2,  \
    maxPrefixExpansions: 200,  \
    queryTimeoutMS: 500,  \
    timeoutPolicy: TimeoutPolicy_Return,  \
    cursorReadSize: 1000,  \
    cursorMaxIdle: 300000,  \
    maxDocTableSize: DEFAULT_DOC_TABLE_SIZE,  \
    searchPoolSize: CONCURRENT_SEARCH_POOL_DEFAULT_SIZE,  \
    indexPoolSize: CONCURRENT_INDEX_POOL_DEFAULT_SIZE,  \
    poolSizeNoAuto: 0,  \
    gcScanSize: GC_SCANSIZE,  \
    gcPolicy: GCPolicy_Fork,  \
    forkGcRunIntervalSec: DEFAULT_FORK_GC_RUN_INTERVAL,  \
    forkGcRetryInterval: 5,  \
    forkGcSleepBeforeExit: 0,  \
    forkGcCleanThreshold: 0,  \
    minPhoneticTermLen: DEFAULT_MIN_PHONETIC_TERM_LEN,  \
    maxResultsToUnsortedMode: DEFAULT_MAX_RESULTS_TO_UNSORTED_MODE,  \
    noMemPool: 0,  \
  }

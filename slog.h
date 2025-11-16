#ifndef SLOG_H
#define SLOG_H

#include <stdio.h>
#include <time.h>

#ifndef SLOG_BUFFER_SIZE
#define SLOG_BUFFER_SIZE 4096
#endif

#define SLOG_DEBUG "debug"
#define SLOG_INFO "info"
#define SLOG_WARN "warn"
#define SLOG_ERROR "error"

#define SLOG(l, ...) Slog(__FILE__, __LINE__, __func__, l, __VA_ARGS__, 0)

// Basic
#define SLOG_OBJECT(k, ...) __SlogMakeNode(SlogTypeObject, k, __VA_ARGS__, 0)
#define SLOG_ARRAY(k, ...) __SlogMakeNode(SlogTypeArray, k, __VA_ARGS__, 0)
#define SLOG_INTEGER(k, v) __SlogMakeNode(SlogTypeInteger, k, v)
#define SLOG_BOOL(k, v) __SlogMakeNode(SlogTypeBool, k, v)
#define SLOG_STRING(k, v) __SlogMakeNode(SlogTypeString, k, v)
#define SLOG_NULL(k) __SlogMakeNode(SlogTypeNull, k)
// Extended
#define SLOG_NUMBER(k, v) __SlogMakeNode(SlogTypeNumber, k, v)
#define SLOG_TIMESPEC(k, v) __SlogMakeNode(SlogTypeTimeSpec, k, v)

typedef enum SlogType SlogType;
enum SlogType {
  SlogTypeNull = 0,
  SlogTypeNumber = 1,
  SlogTypeBool = 2,
  SlogTypeString = 3,
  SlogTypeArray = 4,
  SlogTypeObject = 5,
  SlogTypeInteger = 6,
  SlogTypeTimeSpec = 7,
};

typedef struct SlogNode SlogNode;
struct SlogNode {
  SlogNode *next;
  SlogType type;
  const char *key;
  union {
    long integer;
    double number;
    char *string;
    SlogNode *array;
    SlogNode *object;
    struct timespec ts;
  } value;
};

typedef void (*SlogHandler)(SlogNode *);

void SlogSetHandler(SlogHandler h);
void SlogSetFile(FILE *fp);
void Slog(const char *file, int line, const char *func, const char *level,
          const char *msg, ...);

SlogNode *__SlogMakeNode(int type, const char *key, ...);

#endif // SLOG_H

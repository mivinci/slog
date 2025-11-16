#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "slog.h"

typedef struct Fmt Fmt;
struct Fmt {
  char *buf;
  size_t cap;
  size_t pos;
};

__thread SlogHandler threadLocalHandler = NULL;
__thread SlogNode *threadLocalSlogNode = NULL;
__thread FILE *threadLocalSlogFile = NULL;
__thread char threadLocalBuffer[SLOG_BUFFER_SIZE];

static int Format(Fmt *f, const char *fmt, ...) {
  int n;
  size_t size;
  va_list ap;
  va_start(ap, fmt);
  n = vsnprintf(f->buf + f->pos, f->cap - f->pos, fmt, ap);
  va_end(ap);
  size = f->pos + n;
  if (size >= f->cap) {
    return -1;
  }
  f->pos = size;
  f->buf[size] = '\0';
  return n;
}

static SlogNode *GetNode() {
  if (threadLocalSlogNode) {
    SlogNode *node = threadLocalSlogNode;
    threadLocalSlogNode = node->next;
    return node;
  }
  return malloc(sizeof(SlogNode));
}

static void PutNode(SlogNode *node) {
  if (node == NULL)
    return;
  node->next = threadLocalSlogNode;
  threadLocalSlogNode = node;
}

SlogNode *MakeNode(const int type, const char *key, va_list ap) {
  int n;
  SlogNode *node, *p, **pp;

  node = GetNode();
  node->type = type;
  node->key = key;
  node->next = NULL;
  node->value.number = 0;

  switch (type) {
  case SlogTypeNull:
    break;
  case SlogTypeBool:
  case SlogTypeInteger:
    n = va_arg(ap, long);
    node->value.integer = n;
    break;
  case SlogTypeNumber:
    n = va_arg(ap, double);
    node->value.number = n;
    break;
  case SlogTypeString:
    node->value.string = va_arg(ap, char *);
    break;
  case SlogTypeArray:
  case SlogTypeObject:
    pp = type == SlogTypeArray ? &node->value.array : &node->value.object;
    while ((p = va_arg(ap, SlogNode *)) != NULL) {
      *pp = p;
      pp = &p->next;
    }
    break;
  case SlogTypeTimeSpec:
    node->value.ts = va_arg(ap, struct timespec);
    break;
  default:
    break;
  }
  return node;
}

SlogNode *__SlogMakeNode(const int type, const char *key, ...) {
  SlogNode *node;
  va_list ap;
  va_start(ap, key);
  node = MakeNode(type, key, ap);
  va_end(ap);
  return node;
}

static int FormatEscaped(Fmt *f, const char *s) {
  int n = 0;
  while (*s) {
    switch (*s) {
    case '\b':
      n += Format(f, "\\b");
      break;
    case '\f':
      n += Format(f, "\\f");
      break;
    case '\n':
      n += Format(f, "\\n");
      break;
    case '\r':
      n += Format(f, "\\r");
      break;
    case '\t':
      n += Format(f, "\\t");
      break;
    case '"':
      n += Format(f, "\\\"");
      break;
    case '\\':
      n += Format(f, "\\\\");
      break;
    default:
      if (*s < 0x20) {
        n += Format(f, "\\u%04x", *s);
      } else {
        n += Format(f, "%c", *s);
      }
      break;
    }
    s++;
  }
  return n;
}

static void FormatTimeSpec(Fmt *f, const struct timespec *pts) {
  struct tm tm;
  char buf[64];
  localtime_r(&pts->tv_sec, &tm);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  Format(f, "%s.%09ld", buf, pts->tv_nsec);
}

void WriteNode(Fmt *f, SlogNode *node) {
  // 深度遍历 root，以 JSON 格式输出
  Format(f, "{");
  SlogNode *p = node;
  while (p) {
    Format(f, "\"");
    FormatEscaped(f, p->key);
    Format(f, "\":");
    switch (p->type) {
    case SlogTypeNull:
      Format(f, "null");
      break;
    case SlogTypeInteger:
      Format(f, "%ld", p->value.integer);
      break;
    case SlogTypeNumber:
      Format(f, "%lf", p->value.number);
      break;
    case SlogTypeBool:
      Format(f, "%s", p->value.number ? "true" : "false");
      break;
    case SlogTypeString:
      Format(f, "\"");
      FormatEscaped(f, p->value.string);
      Format(f, "\"");
      break;
    case SlogTypeArray:
      Format(f, "[");
      SlogNode *q = p->value.array;
      while (q) {
        WriteNode(f, q);
        q = q->next;
        if (q) {
          Format(f, ",");
        }
      }
      Format(f, "]");
      break;
    case SlogTypeObject:
      WriteNode(f, p->value.object);
      break;
    case SlogTypeTimeSpec:
      Format(f, "\"");
      FormatTimeSpec(f, &p->value.ts);
      Format(f, "\"");
      break;
    default:
      break;
    }
    p = p->next;
    if (p) {
      Format(f, ",");
    }
  }
  Format(f, "}");
  PutNode(node);
}

void defaultHandler(SlogNode *node) {
  FILE *fp;
  Fmt f;

  f.buf = threadLocalBuffer;
  f.cap = SLOG_BUFFER_SIZE;
  f.pos = 0;

  fp = threadLocalSlogFile;
  if (!fp) {
    fp = stdout;
  }

  WriteNode(&f, node);
  fputs(f.buf, fp);
}

void Slog(const char *file, const int line, const char *func, const char *level,
          const char *msg, ...) {
  SlogNode *root, *extra;
  SlogHandler h;
  va_list ap;

  va_start(ap, msg);
  extra = MakeNode(SlogTypeObject, "extra", ap);
  va_end(ap);

  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);

  root =
      SLOG_OBJECT("", SLOG_TIMESPEC("time", &ts), SLOG_STRING("file", file),
                  SLOG_INTEGER("line", line), SLOG_STRING("func", func),
                  SLOG_STRING("level", level), SLOG_STRING("msg", msg), extra);

  h = threadLocalHandler;
  if (!h) {
    h = defaultHandler;
  }
  h(root->value.object);
}

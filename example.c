#include "slog.h"

int main(void) {
  SLOG(
    SLOG_INFO,
    "this is a message",
    SLOG_INTEGER("a\"ge", 18),
    SLOG_STRING("foo", "bar"),
    SLOG_BOOL("baz", 1),
    SLOG_ARRAY("array",
      SLOG_OBJECT("empty", 0)
    ),
    SLOG_OBJECT("obj",
      SLOG_NUMBER("age", 18),
      SLOG_STRING("name", "qaq")
    )
  );
  return 0;
}

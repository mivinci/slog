/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SLOG_H
#define SLOG_H

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum slog_level {
	SLOG_ERROR = 0,
	SLOG_WARN = 1,
	SLOG_INFO = 2,
	SLOG_DEBUG = 3,
	SLOG_LAST
};

enum slog_type {
	SLOG_TYPE_NULL = 0,
	SLOG_TYPE_STRING,
	SLOG_TYPE_INT,
	SLOG_TYPE_FLOAT,
	SLOG_TYPE_BOOL,
	SLOG_TYPE_TIME,
	// SLOG_ARRAY,··
	SLOG_TYPE_OBJECT,
	SLOG_TYPE_PLAIN,
};

struct slog_field {
	enum slog_type type;

	const char *key;
	union {
		const char *string;
		double number;
		long long integer;
		bool boolean;
		enum slog_level level;
		struct timespec time;
		// struct slog_field *array;
		struct slog_field *object;
	} value;

	struct slog_field *next;
};

const struct slog_field slog_field_default;

struct slog_alloc {
	struct {
		struct slog_field *pool;
		size_t length, capacity;
	} field;

	struct {
		char *buffer;
		size_t size, capacity;
	} output;
};

void slog_alloc_init(struct slog_alloc *alloc, size_t capacity) {
	if (capacity == 0) {
		capacity = 32;
	}

	alloc->field.pool = calloc(capacity, sizeof(struct slog_field));
	alloc->field.length = 0;
	alloc->field.capacity = capacity;

	alloc->output.buffer = calloc(capacity * 32, sizeof(char));
	alloc->output.size = 1; // it's '\0'
	alloc->output.capacity = capacity * 32;
}

void slog_alloc_fini(struct slog_alloc *alloc) {
	free(alloc->field.pool);
	free(alloc->output.buffer);
}

struct slog_field *slog_alloc_field(struct slog_alloc *alloc) {
	assert(alloc);
	assert(alloc->field.capacity);

	if (alloc->field.capacity == alloc->field.length) {
		size_t new_capacity = alloc->field.capacity + 16;
		alloc->field.pool =
			reallocarray(alloc->field.pool, new_capacity,
				     sizeof(struct slog_field));
		alloc->field.capacity = new_capacity;
	}
	struct slog_field *field = &alloc->field.pool[alloc->field.length++];
	*field = slog_field_default;
	return field;
}

// output.size = strlen(string) + 1
void slog_alloc_vfmt(struct slog_alloc *alloc, const char *fmt, ...) {
	if (!fmt) {
		return;
	}

	va_list args;
	va_start(args, fmt);
	int count = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (count <= 0) {
		return;
	}

	if (alloc->output.size + count > alloc->output.capacity) {
		size_t new_capacity = count > alloc->output.capacity
					      ? alloc->output.capacity + count
					      : alloc->output.capacity * 2;
		alloc->output.buffer =
			realloc(alloc->output.buffer, new_capacity);
		alloc->output.capacity = new_capacity;
	}

	va_start(args, fmt);
	int written = vsnprintf(alloc->output.buffer + alloc->output.size - 1,
				alloc->output.capacity - alloc->output.size + 1,
				fmt, args);
	va_end(args);

	if (written > 0) {
		alloc->output.size += written;
		alloc->output.buffer[alloc->output.size - 1] = '\0';
	}
}

struct slog_field *slog_new_vfield(struct slog_alloc *alloc,
				   enum slog_type type, const char *key,
				   va_list ap) {
	struct slog_field *field = slog_alloc_field(alloc);
	field->key = key;
	field->type = type;

	struct slog_field *p, **pp;

	switch (type) {
	case SLOG_TYPE_NULL:
		break;
	case SLOG_TYPE_STRING:
		field->value.string = va_arg(ap, const char *);
		break;
	case SLOG_TYPE_INT:
		field->value.integer = va_arg(ap, long long);
		break;
	case SLOG_TYPE_FLOAT:
		field->value.number = va_arg(ap, double);
		break;
	case SLOG_TYPE_BOOL:
		field->value.boolean = va_arg(ap, int);
		break;
	case SLOG_TYPE_TIME:
		clock_gettime(CLOCK_REALTIME, &field->value.time);
		break;
	// case SLOG_ARRAY:
	case SLOG_TYPE_PLAIN:
		assert(!key);
	case SLOG_TYPE_OBJECT:
		pp = &field->value.object;
		while ((p = va_arg(ap, struct slog_field *)) != NULL) {
			*pp = p;
			pp = &p->next;
		}
		break;
	}
	return field;
}

struct slog_field *slog_new_field(struct slog_alloc *alloc, enum slog_type type,
				  const char *key, ...) {
	va_list ap;
	va_start(ap, key);
	struct slog_field *field = slog_new_vfield(alloc, type, key, ap);
	va_end(ap);
	return field;
}

void slog_fmt_escape(struct slog_alloc *alloc, const char *str) {
	assert(str);

	slog_alloc_vfmt(alloc, "\"");

	for (const char *p = str; *p; p++) {
		unsigned char c = *p;

		switch (c) {
		case '"':
			slog_alloc_vfmt(alloc, "\\\"");
			break;
		case '\\':
			slog_alloc_vfmt(alloc, "\\\\");
			break;
		case '\b':
			slog_alloc_vfmt(alloc, "\\b");
			break;
		case '\f':
			slog_alloc_vfmt(alloc, "\\f");
			break;
		case '\n':
			slog_alloc_vfmt(alloc, "\\n");
			break;
		case '\r':
			slog_alloc_vfmt(alloc, "\\r");
			break;
		case '\t':
			slog_alloc_vfmt(alloc, "\\t");
			break;
		default:
			if (c < 0x20) {
				slog_alloc_vfmt(alloc, "\\u%04x", c);
			} else {
				slog_alloc_vfmt(alloc, "%c", c);
			}
			break;
		}
	}
	slog_alloc_vfmt(alloc, "\"");
}

void slog_fmt_time(struct slog_alloc *alloc, struct slog_field *field) {
	assert(field->type == SLOG_TYPE_TIME);

	struct tm *t = localtime(&field->value.time.tv_sec);

	// YYYY-MM-DD HH:MM:SS.mmm
	// 2025-11-16 11:15:25.123

	slog_alloc_vfmt(
		alloc, "\"time\": \"%04d-%02d-%02d %02d:%02d:%02d.%03ld\"",
		t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour,
		t->tm_min, t->tm_sec, field->value.time.tv_nsec / 1000000);
}

void slog_fmt_field(struct slog_alloc *alloc, struct slog_field *field) {
	while (field) {
		if (field->key) {
			slog_fmt_escape(alloc, field->key);
			slog_alloc_vfmt(alloc, ": ");
		}
		switch (field->type) {
		case SLOG_TYPE_STRING:
			slog_fmt_escape(alloc, field->value.string);
			break;
		case SLOG_TYPE_BOOL:
			slog_alloc_vfmt(alloc, "%s",
					field->value.boolean ? "true"
							     : "false");
			break;
		case SLOG_TYPE_INT:
			slog_alloc_vfmt(alloc, "%lld", field->value.integer);
			break;
		case SLOG_TYPE_FLOAT:
			slog_alloc_vfmt(alloc, "%f", field->value.number);
			break;
		case SLOG_TYPE_OBJECT:
			slog_alloc_vfmt(alloc, "{");
			slog_fmt_field(alloc, field->value.object);
			slog_alloc_vfmt(alloc, "}");
			break;
		case SLOG_TYPE_PLAIN:
			assert(!field->key);
			slog_fmt_field(alloc, field->value.object);
			break;
		case SLOG_TYPE_TIME:
			slog_fmt_time(alloc, field);
			break;
		default:
			break;
		}
		field = field->next;
		if (field) {
			slog_alloc_vfmt(alloc, ", ");
		}
	}
}

static void slog_main(struct slog_alloc *alloc, const char *file,
		      const int line, const char *func, const char *level,
		      const char *msg, ...) {
	va_list fields;
	va_start(fields, msg);
	struct slog_field *extra =
		slog_new_vfield(alloc, SLOG_TYPE_PLAIN, NULL, fields);
	va_end(fields);

	if (strncmp(level, "SLOG_", 5) == 0) {
		level = level + 5;
	}

	struct slog_field *root = slog_new_field(
		alloc, SLOG_TYPE_OBJECT, NULL,
		slog_new_field(alloc, SLOG_TYPE_STRING, "file", file),
		slog_new_field(alloc, SLOG_TYPE_INT, "line", line),
		slog_new_field(alloc, SLOG_TYPE_STRING, "func", func),
		slog_new_field(alloc, SLOG_TYPE_STRING, "level", level),
		slog_new_field(alloc, SLOG_TYPE_STRING, "msg", msg),
		slog_new_field(alloc, SLOG_TYPE_TIME, NULL), extra, NULL);

	slog_fmt_field(alloc, root);

	fprintf(stdout, "%s\n", alloc->output.buffer);
	fflush(stdout);
}

#define SLOG_ALLOC slog_alloc
#define SLOG_BOOL(K, V) slog_new_field(&SLOG_ALLOC, SLOG_TYPE_BOOL, K, V)
#define SLOG_FLOAT(K, V) slog_new_field(&SLOG_ALLOC, SLOG_TYPE_FLOAT, K, V)
#define SLOG_STRING(K, V) slog_new_field(&SLOG_ALLOC, SLOG_TYPE_STRING, K, V)
#define SLOG_INT(K, V) slog_new_field(&SLOG_ALLOC, SLOG_TYPE_INT, K, V)
#define SLOG_OBJECT(K, ...)                                                    \
	slog_new_field(&SLOG_ALLOC, SLOG_TYPE_OBJECT,                          \
		       K __VA_OPT__(, ) __VA_ARGS__, NULL)

#define SLOG(LEVEL, MSG, ...)                                                  \
	do {                                                                   \
		struct slog_alloc SLOG_ALLOC = {0};                            \
		slog_alloc_init(&SLOG_ALLOC, 0);                               \
		slog_main(&SLOG_ALLOC, __FILE__, __LINE__, __func__, #LEVEL,   \
			  MSG, ##__VA_ARGS__, NULL);                           \
		slog_alloc_fini(&SLOG_ALLOC);                                  \
	} while (0)

#endif // SLOG_H

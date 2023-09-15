#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "mem.h"
#include "buffer.h"

/* prob I need some kind of stats */

struct buffer *mk_buffer(void) {
	struct buffer	*b;
	b	  = xmalloc(sizeof(struct buffer));
	b->p	  = NULL;
	b->size	  = 0;
	b->cursor = 0;
	return b;
}


void buffer_init_buffer(struct buffer *b) {
	b->p	  = NULL;
	b->cursor = 0;
	b->size	  = 0;
}

void buffer_alloc_buffer(struct buffer *b) {
	b->p	  = xmalloc(sizeof(char) * BUFFER_SIZE);
	b->size	  = BUFFER_SIZE;
	b->cursor = 0;
}

void buffer_truncate_buffer(struct buffer *b) {
	if (b->p != NULL)
		free(b->p);
	buffer_init_buffer(b);
}

void buffer_zero_buffer(struct buffer *b) {
	if (b->p != NULL)
		memset(b->p, 0, sizeof(char) * b->size);
	b->cursor = 0;
}

/* I dont think I ever used this */
int buffer_ncmp_string(struct buffer *b, const char *s, int n) {
	int k = strlen(s);
	if (n > 0) {
		if (k < n && b->cursor < n) {
			if (k < b->cursor)
				return 1;
			else if (k == b->cursor)
				return 0;
			else return -1;
		}

		if (k < n) return 1;
		if (b->cursor < n) return -1;
		return strncmp(b->p, s, n);
	}

	return strcmp(b->p, s);
}

/* i want k*BUFFER_SIZE where k is a positive int */
int buffer_align_buffer(struct buffer *b, size_t len) {
	size_t	 need = (len/BUFFER_SIZE + 1) * BUFFER_SIZE;
	int	 room = (b->size - b->cursor) - need;
	char	*new;
	if (room <= 0) {
		/*prob. i could just use realloc */
		new	 = realloc(b->p, sizeof(char) * (b->size + need));
		if (!new) {
			perror("realloc: ");
			exit(1);
		}
		/* if i'm not here then i just crashed */
		/* memset(new+b->size, 0, sizeof(char) * need); */
		b->p	 = new;
		b->size += need;
	}
	return 0;
}

/* overwrite buffer */
int buffer_write_to_buffer(struct buffer *b, char *data, size_t len) {
	if (len <= 0 )
		return -1;
	buffer_align_buffer(b, len);
	memcpy(b->p, data, sizeof(char) * len);
	b->cursor = len;
	return 0;
}

int buffer_append_string_to_buffer(struct buffer *b, const char *c) {
	int len = strlen(c);
	if (buffer_align_buffer(b, len+1) == 0) {
		memmove(b->p+b->cursor, c, len);
		b->cursor	+= len;
		b->p[b->cursor] = '\0';
		return len;
	}
	return -1;
}

int buffer_append_to_buffer(struct buffer *b, struct buffer *c) {
	if (c == NULL || b == NULL)
		return -1;
	if (buffer_align_buffer(b, c->cursor) == 0) {
		// this is wrong
		memmove(b->p+b->cursor, c->p, c->cursor);
		b->cursor += c->cursor;
		return 0;
	}
	/* i should never be here */
	return -1;
}

void buffer_print_string_buffer(struct buffer *b) {
	printf("%s\n", b->p);
}

size_t buffer_len(struct buffer *b) {
	return b->cursor;
}

void buffer_free_buffer(struct buffer *b) {
	if (b->p != NULL)
		free(b->p);
	free(b);
	/* please don't use b anymore */
}

void buffer_print_serialize_buffer(struct buffer *b) {
	printf("b->p=%p\nb->cursor=%d\nb->size=%zu\n", b->p, b->cursor, b->size);
}

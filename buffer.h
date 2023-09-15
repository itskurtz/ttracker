#ifndef H_BUFFER
#define H_BUFFER
#define BUFFER_SIZE (64)

struct buffer {
	char	*p;
	size_t	 size;
	int	 cursor;
};

struct buffer *mk_buffer(void);
void buffer_init_buffer(struct buffer *b);
void buffer_alloc_buffer(struct buffer *b);
void buffer_truncate_buffer(struct buffer *b);
int buffer_align_buffer(struct buffer *b, size_t len);
int buffer_write_to_buffer(struct buffer *b, char *data, size_t len);
int buffer_append_to_buffer(struct buffer *b, struct buffer *c);
int buffer_append_string_to_buffer(struct buffer *b, const char *c);
void buffer_print_string_buffer(struct buffer *b);
void buffer_free_buffer(struct buffer *b);
void buffer_print_serialize_buffer(struct buffer *b);
void buffer_zero_buffer(struct buffer *b);
size_t buffer_len(struct buffer *b);
int buffer_cur_active_buffers(void);
#endif

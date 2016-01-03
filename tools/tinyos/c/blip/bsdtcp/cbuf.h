#ifndef CBUF_H_
#define CBUF_H_

/* CIRCULAR BUFFER
   The circular buffer can be treated either as a buffer of bytes, or a buffer
   of TCP segments. Don't mix and match the functions unless you know what
   you're doing! */
   

int cbuf_init(uint8_t* buf, size_t len);

size_t cbuf_write(uint8_t* buf, uint8_t* data, size_t data_len);
size_t cbuf_read(uint8_t* buf, uint8_t* data, size_t numbytes, int pop);
size_t cbuf_read_offset(uint8_t* buf, uint8_t* data, size_t numbytes, size_t offset);
size_t cbuf_pop(uint8_t* buf, size_t numbytes);
size_t cbuf_used_space(uint8_t* buf);
size_t cbuf_free_space(uint8_t* buf);
size_t cbuf_size(uint8_t* buf);

size_t cbuf_reass_write(uint8_t* buf, size_t offset, uint8_t* data, size_t numbytes, uint8_t* bitmap, size_t* firstindex);
size_t cbuf_reass_merge(uint8_t* buf, size_t numbytes, uint8_t* bitmap);
size_t cbuf_reass_count_set(uint8_t* buf, size_t offset, uint8_t* bitmap, size_t limit);
int cbuf_reass_within_offset(uint8_t* buf, size_t offset, size_t index);

/*
int cbuf_write_segment(uint8_t* buf, uint8_t* segment, size_t seglen);
size_t cbuf_pop_segment(uint8_t* buf, size_t segsize);
size_t cbuf_peek_segment_size(uint8_t* buf);
size_t cbuf_peek_segment(uint8_t* buf, uint8_t* data, size_t numbytes);
*/

#endif

#ifndef BITMAP_H_
#define BITMAP_H_

void bmp_init(uint8_t* buf, size_t len);
void bmp_setrange(uint8_t* buf, size_t start, size_t len);
void bmp_clrrange(uint8_t* buf, size_t start, size_t len);
size_t bmp_countset(uint8_t* buf, size_t buflen, size_t start, size_t limit);
int bmp_isempty(uint8_t* buf, size_t buflen);
void bmp_print(uint8_t* buf, size_t buflen);

#endif

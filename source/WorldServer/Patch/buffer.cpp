#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "buffer.h"

Buffer::Buffer() {
    buf = NULL;
    capacity = 0;
    len = 0;
}

Buffer::~Buffer() {
    if (buf != NULL)
        free(buf);
}

unsigned int Buffer::GetLength() {
    return len;
}

int Buffer::IndexOf(char c) {
    unsigned int i;

    for (i = 0; i < len; i++) {
        if (buf[i] == c)
            return (int)i;
    }

    return -1;
}

void Buffer::Clear() {
    if (buf != NULL) {
        free(buf);
        buf = NULL;
    }
    capacity = 0;
    len = 0;
}

const char * Buffer::Peek() {
    return buf;
}

void Buffer::PeekReadPrimitive(void *buf, unsigned int size, bool peek) {
    if (len >= size) {
        memcpy(buf, this->buf, size);
        if (!peek) {
            memmove(this->buf, this->buf + size, len - size);
            len -= size;
        }
    }
}

uint32_t Buffer::PeekUInt32() {
    uint32_t ret = 0;

    PeekReadPrimitive(&ret, sizeof(ret), true);

    return ret;
}

unsigned int Buffer::Read(unsigned int len, char **dst) {
    unsigned int actual = len;

    if (actual > this->len)
        actual = this->len;

    *dst = (char *)malloc(actual);
    if (*dst == NULL)
        return 0;

    return Read(actual, *dst, actual);
}

unsigned int Buffer::Read(unsigned int len, char *dst, unsigned int size) {
    unsigned int actual = len;

    if (actual > size)
        actual = size;
    if (actual > this->len)
        actual = this->len;

    memcpy(dst, buf, actual);
    memmove(buf, buf + actual, this->len - actual);
    this->len -= actual;

    return actual;
}

uint32_t Buffer::ReadUInt32() {
    uint32_t ret = 0;

    PeekReadPrimitive(&ret, sizeof(ret), false);

    return ret;
}

bool Buffer::Write(const char *data, unsigned int len) {
    unsigned int new_capacity;
    char *new_buf;

    while (this->len + len > capacity) {
        new_capacity = capacity == 0 ? 512 : capacity * 2;
        new_buf = (char *)realloc(buf, new_capacity);
        if (new_buf == NULL)
            return false;

        capacity = new_capacity;
        buf = new_buf;
    }

    memcpy(buf + this->len, data, len);
    this->len += len;

    return true;
}

bool Buffer::Write(const char *data) {
    return Write(data, (unsigned int)strlen(data));
}

bool Buffer::Write(const unsigned char *data, unsigned int len) {
    return Write((const char *)data, len);
}

bool Buffer::WriteInt8(int8_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteUInt8(uint8_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteInt16(int16_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteUInt16(uint16_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteInt32(int32_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteUInt32(uint32_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteInt64(int64_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteUInt64(uint64_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteChar(char data) {
    return Write(&data, sizeof(data));
}

bool Buffer::WriteDouble(double data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return Write(buf, sizeof(buf));
}

bool Buffer::WriteAt(unsigned int index, const char *data, unsigned int len) {
    unsigned int new_capacity;
    char *new_buf;

    while (this->len + len > capacity) {
        new_capacity = capacity == 0 ? 512 : capacity * 2;
        new_buf = (char *)realloc(buf, new_capacity);
        if (new_buf == NULL)
            return false;

        capacity = new_capacity;
        buf = new_buf;
    }

    memmove(buf + index + len, buf + index, this->len - index);
    memcpy(buf + index, data, len);
    this->len += len;

    return true;
}

bool Buffer::WriteAtUInt32(unsigned int index, uint32_t data) {
    char buf[sizeof(data)];

    memcpy(buf, &data, sizeof(buf));
    return WriteAt(index, buf, sizeof(buf));
}

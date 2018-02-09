#pragma once

#include <stdint.h>

class Buffer {
public:
    Buffer();
    ~Buffer();

    unsigned int GetLength();

    int IndexOf(char c);

    void Clear();

    const char * Peek();
    uint32_t PeekUInt32();

    unsigned int Read(unsigned int len, char **dst);
    unsigned int Read(unsigned int len, char *dst, unsigned int size);
    uint32_t ReadUInt32();

    bool Write(const char *data, unsigned int len);
    bool Write(const char *data);
    bool Write(const unsigned char *data, unsigned int len);
    bool WriteUInt8(uint8_t data);
    bool WriteInt8(int8_t data);
    bool WriteUInt16(uint16_t data);
    bool WriteInt16(int16_t data);
    bool WriteUInt32(uint32_t data);
    bool WriteInt32(int32_t data);
    bool WriteUInt64(uint64_t data);
    bool WriteInt64(int64_t data);
    bool WriteChar(char data);
    bool WriteDouble(double data);

    bool WriteAt(unsigned int index, const char *data, unsigned int len);
    bool WriteAtUInt32(unsigned int index, uint32_t data);

private:
    char *buf;
    unsigned int capacity;
    unsigned int len;

    void PeekReadPrimitive(void *buf, unsigned int size, bool peek);
};

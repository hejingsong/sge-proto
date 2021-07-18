#include "sge_crc16.h"

static const int POLYNOMIAL = 0x1021;

uint16_t sge_crc16(const char* str, size_t len) {
    unsigned char i, j;
    unsigned int crc16 = 0;
    unsigned int val;

    for(i = 0; i < len; i++) {
        val = str[i] << 8;
        for(j = 0; j < 8; j++) {
            if((crc16 ^ val) & 0x8000) {
                crc16 = (crc16 << 1) ^ POLYNOMIAL;
            } else {
                crc16 <<= 1;
            }
            val <<= 1;
        }
    }

    return crc16;
}

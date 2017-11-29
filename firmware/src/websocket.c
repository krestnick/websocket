/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#include "crypto/crypto.h"
#include "crypto/src/sha.h"
#include "ws_server.h"
#include "websocket.h"

static char ws_rn[] PROGMEM = "\r\n";

static inline size_t ws_base64len(size_t n)
{
    return (n + 2) / 3 * 4;
}


static size_t ws_base64(char *buf, size_t nbuf, const unsigned char *p, size_t n)
{    
    const char ws_base64_t[64] PROGMEM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, m = ws_base64len(n);
    unsigned ws_x;

    //if (nbuf >= m)
    for (i = 0; i < n; ++i)
    {
        ws_x = p[i] << 0x10;
        ws_x |= (++i < n ? p[i] : 0) << 0x08;
        ws_x |= (++i < n ? p[i] : 0) << 0x00;

        *buf++ = ws_base64_t[ws_x >> 3 * 6 & 0x3f];
        *buf++ = ws_base64_t[ws_x >> 2 * 6 & 0x3f];
        *buf++ = (((n - 0 - i) >> 31) & '=') |
                (~((n - 0 - i) >> 31) & ws_base64_t[ws_x >> 1 * 6 & 0x3f]);
        *buf++ = (((n - 1 - i) >> 31) & '=') |
                (~((n - 1 - i) >> 31) & ws_base64_t[ws_x >> 0 * 6 & 0x3f]);
    }

    return m;
}
#define SHA1_SIZE 20

void ws_sha1mix(unsigned *r, unsigned *w)
{
    unsigned ws_a = r[0];
    unsigned ws_b = r[1];
    unsigned ws_c = r[2];
    unsigned ws_d = r[3];
    unsigned ws_e = r[4];
    unsigned ws_t, ws_i = 0;

#define rol(x,s) (  ((x) << (s)) | ((unsigned) ((x) >> (32 - (s))))   )
#define mix(f,v) do { \
		ws_t = (f) + (v) + rol(ws_a, 5) + ws_e + w[ws_i & 0xf]; \
		ws_e = ws_d; \
		ws_d = ws_c; \
		ws_c = rol(ws_b, 30); \
		ws_b = ws_a; \
		ws_a = ws_t; \
	} while (0)

    for (; ws_i < 16; ++ws_i)
        mix(ws_d ^ (ws_b & (ws_c ^ ws_d)), 0x5a827999);

    for (; ws_i < 20; ++ws_i)
    {
        w[ws_i & 0xf] = rol(w[(ws_i + 13) & 0xf] ^ w[(ws_i + 8) & 0xf] ^ w[(ws_i + 2) & 0xf] ^ w[ws_i & 0xf], 1);
        mix(ws_d ^ (ws_b & (ws_c ^ ws_d)), 0x5a827999);
    }

    for (; ws_i < 40; ++ws_i)
    {
        w[ws_i & 0xf] = rol(w[(ws_i + 13) & 0xf] ^ w[(ws_i + 8) & 0xf] ^ w[(ws_i + 2) & 0xf] ^ w[ws_i & 0xf], 1);
        mix(ws_b ^ ws_c ^ ws_d, 0x6ed9eba1);
    }

    for (; ws_i < 60; ++ws_i)
    {
        w[ws_i & 0xf] = rol(w[(ws_i + 13) & 0xf] ^ w[(ws_i + 8) & 0xf] ^ w[(ws_i + 2) & 0xf] ^ w[ws_i & 0xf], 1);
        mix((ws_b & ws_c) | (ws_d & (ws_b | ws_c)), 0x8f1bbcdc);
    }

    for (; ws_i < 80; ++ws_i)
    {
        w[ws_i & 0xf] = rol(w[(ws_i + 13) & 0xf] ^ w[(ws_i + 8) & 0xf] ^ w[(ws_i + 2) & 0xf] ^ w[ws_i & 0xf], 1);
        mix(ws_b ^ ws_c ^ ws_d, 0xca62c1d6);
    }

#undef mix
#undef rol

    r[0] += ws_a;
    r[1] += ws_b;
    r[2] += ws_c;
    r[3] += ws_d;
    r[4] += ws_e;
}

static void ws_sha1(unsigned char *h, const void *p, size_t n)
{
    size_t ws_i = 0;    
    unsigned ws_w[16], ws_r[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};
    
    for (; ws_i < (n & ~0x3f);)
    {
        do ws_w[ws_i >> 2 & 0xf] =
                ((const unsigned char *) p)[ws_i + 3] << 0x00 |
                ((const unsigned char *) p)[ws_i + 2] << 0x08 |
                ((const unsigned char *) p)[ws_i + 1] << 0x10 |
                ((const unsigned char *) p)[ws_i + 0] << 0x18; while ((ws_i += 4) & 0x3f);
        ws_sha1mix(ws_r, ws_w);
    }

    memset(ws_w, 0, sizeof ws_w);

    for (; ws_i < n; ++ws_i)
        ws_w[ws_i >> 2 & 0xf] |= ((const unsigned char *) p)[ws_i] << (((3 ^ ws_i) & 3) << 3);

    ws_w[(ws_i >> 2) & 0xf] |= 0x80 << (((3 ^ ws_i) & 3) << 3);

    if ((n & 0x3f) > 56)
    {
        ws_sha1mix(ws_r, ws_w);
        memset(ws_w, 0, sizeof ws_w);
    }

    ws_w[15] = n << 3;
    ws_sha1mix(ws_r, ws_w);

    for (ws_i = 0; ws_i < 5; ++ws_i)
        h[(ws_i << 2) + 0] = (unsigned char) (ws_r[ws_i] >> 0x18),
        h[(ws_i << 2) + 1] = (unsigned char) (ws_r[ws_i] >> 0x10),
        h[(ws_i << 2) + 2] = (unsigned char) (ws_r[ws_i] >> 0x08),
        h[(ws_i << 2) + 3] = (unsigned char) (ws_r[ws_i] >> 0x00);
}

void ws_nullHandshake(struct handshake *hs)
{
    hs->host = NULL;
    hs->origin = NULL;
    hs->resource = NULL;
    hs->key = NULL;
    hs->frameType = WS_EMPTY_FRAME;
}

void ws_freeHandshake(struct handshake *hs)
{
    if (hs->host)
    {
        free(hs->host);
    }
    if (hs->origin)
    {
        free(hs->origin);
    }
    if (hs->resource)
    {
        free(hs->resource);
    }
    if (hs->key)
    {
        free(hs->key);
    }
    ws_nullHandshake(hs);
}

static char* ws_getUptoLinefeed(const char *startFrom)
{
    char *ws_writeTo = NULL;
    uint8_t ws_newLength = strstr_P(startFrom, ws_rn) - startFrom;
    assert(ws_newLength);
    ws_writeTo = (char *) malloc(ws_newLength + 1); //+1 for '\x00'
    assert(ws_writeTo);
    memcpy(ws_writeTo, startFrom, ws_newLength);
    ws_writeTo[ ws_newLength ] = 0;

    return ws_writeTo;
}

enum ws_FrameType ws_ParseHandshake(const uint8_t *inputFrame, size_t inputLength,
        struct handshake *hs)
{
    const char *ws_inputPtr = (const char *) inputFrame;
    const char *ws_endPtr = (const char *) inputFrame + inputLength;

    if (!strstr((const char *) inputFrame, "\r\n\r\n"))
        return WS_INCOMPLETE_FRAME;

    if (memcmp_P(inputFrame, PSTR("GET "), 4) != 0)
        return WS_ERROR_FRAME;
    // measure resource size
    char *first = strchr((const char *) inputFrame, ' ');
    if (!first)
        return WS_ERROR_FRAME;
    first++;
    char *second = strchr(first, ' ');
    if (!second)
        return WS_ERROR_FRAME;

    if (hs->resource)
    {
        free(hs->resource);
        hs->resource = NULL;
    }
    hs->resource = (char *) malloc(second - first + 1); // +1 is for \x00 symbol
    assert(hs->resource);

    if (sscanf_P(ws_inputPtr, PSTR("GET %s HTTP/1.1\r\n"), hs->resource) != 1)
        return WS_ERROR_FRAME;
    ws_inputPtr = strstr_P(ws_inputPtr, ws_rn) + 2;

    /*
        parse next lines
     */
#define prepare(x) do {if (x) { free(x); x = NULL; }} while(0)
#define strtolower(x) do { int i; for (i = 0; x[i]; i++) x[i] = tolower(x[i]); } while(0)
    uint8_t ws_connectionFlag = FALSE;
    uint8_t ws_upgradeFlag = FALSE;
    uint8_t ws_subprotocolFlag = FALSE;
    uint8_t ws_versionMismatch = FALSE;
    while (ws_inputPtr < ws_endPtr && ws_inputPtr[0] != '\r' && ws_inputPtr[1] != '\n')
    {
        if (memcmp_P(ws_inputPtr, hostField, strlen_P(hostField)) == 0)
        {
            ws_inputPtr += strlen_P(hostField);
            prepare(hs->host);
            hs->host = ws_getUptoLinefeed(ws_inputPtr);
        }
        else
            if (memcmp_P(ws_inputPtr, originField, strlen_P(originField)) == 0)
        {
            ws_inputPtr += strlen_P(originField);
            prepare(hs->origin);
            hs->origin = ws_getUptoLinefeed(ws_inputPtr);
        }
        else
            if (memcmp_P(ws_inputPtr, protocolField, strlen_P(protocolField)) == 0)
        {
            ws_inputPtr += strlen_P(protocolField);
            ws_subprotocolFlag = TRUE;
        }
        else
            if (memcmp_P(ws_inputPtr, keyField, strlen_P(keyField)) == 0)
        {
            ws_inputPtr += strlen_P(keyField);
            prepare(hs->key);
            hs->key = ws_getUptoLinefeed(ws_inputPtr);
        }
        else
            if (memcmp_P(ws_inputPtr, versionField, strlen_P(versionField)) == 0)
        {
            ws_inputPtr += strlen_P(versionField);
            char *versionString = NULL;
            versionString = ws_getUptoLinefeed(ws_inputPtr);
            if (memcmp_P(versionString, version, strlen_P(version)) != 0)
                ws_versionMismatch = TRUE;
            free(versionString);
        }
        else
            if (memcmp_P(ws_inputPtr, connectionField, strlen_P(connectionField)) == 0)
        {
            ws_inputPtr += strlen_P(connectionField);
            char *connectionValue = NULL;
            connectionValue = ws_getUptoLinefeed(ws_inputPtr);
            strtolower(connectionValue);
            assert(connectionValue);
            if (strstr_P(connectionValue, upgrade) != NULL)
                ws_connectionFlag = TRUE;
            free(connectionValue);
        }
        else
            if (memcmp_P(ws_inputPtr, upgradeField, strlen_P(upgradeField)) == 0)
        {
            ws_inputPtr += strlen_P(upgradeField);
            char *compare = NULL;
            compare = ws_getUptoLinefeed(ws_inputPtr);
            strtolower(compare);
            assert(compare);
            if (memcmp_P(compare, websocket, strlen_P(websocket)) == 0)
                ws_upgradeFlag = TRUE;
            free(compare);
        };

        ws_inputPtr = strstr_P(ws_inputPtr, ws_rn) + 2;
    }

    // we have read all data, so check them
    if (!hs->host || !hs->key || !ws_connectionFlag || !ws_upgradeFlag || ws_subprotocolFlag
        || ws_versionMismatch)
    {
        hs->frameType = WS_ERROR_FRAME;
    }
    else
    {
        hs->frameType = WS_OPENING_FRAME;
    }

    return hs->frameType;
}

void ws_test_sha1(void)
{
    char ws_responseKey[200];    
    unsigned char sha1_hash[30];
    unsigned int ws_key_length;
    char ws_ServerKey[] = {"ew4MSslhV5cm2TK95DGxYA=="}; //{"dGhlIHNhbXBsZSBub25jZQ=="};
    
    ws_responseKey[0]=0;
    sprintf(ws_responseKey,"%s258EAFA5-E914-47DA-95CA-C5AB0DC85B11",ws_ServerKey);
  
    ws_key_length = strlen(ws_responseKey);
    memset(sha1_hash, 0, sizeof (sha1_hash));

    ws_sha1(sha1_hash, ws_responseKey, ws_key_length);
    size_t base64Length = ws_base64(ws_responseKey, ws_key_length, sha1_hash, 20);
    ws_responseKey[base64Length-1] = '=';
    ws_responseKey[base64Length] = '\0';

    /* from: https://tools.ietf.org/html/rfc6455#page-6 
     * 
     * expected result:
     *      0xb3 0x7a 0x4f 0x2c 0xc0 0x62 0x4f 0x16 0x90 0xf6 0x46 0x06 0xcf 0x38 0x59 0x45 0xb2 0xbe 0xc4 0xea
     *      s3pPLMBiTxaQ9kYGzzhZRbK+xOo=
     */
    
    for (;;);
}

void ws_GetHandshakeAnswer(const struct handshake *hs, uint8_t *outFrame,
        size_t *outLength)
{
    assert(outFrame && *outLength);
    assert(hs->frameType == WS_OPENING_FRAME);
    assert(hs && hs->key);

    char *ws_responseKey = NULL;
    uint8_t ws_length = strlen(hs->key) + strlen_P(secret);
    ws_responseKey = malloc(ws_length);
    memcpy(ws_responseKey, hs->key, strlen(hs->key));
    memcpy_P(&(ws_responseKey[strlen(hs->key)]), secret, strlen_P(secret));
    unsigned char ws_shaHash[20];
    memset(ws_shaHash, 0, sizeof (ws_shaHash));
    ws_sha1(ws_shaHash, ws_responseKey, ws_length);
    size_t ws_base64Length = ws_base64(ws_responseKey, ws_length, ws_shaHash, 20);
    ws_responseKey[ws_base64Length-1] = '=';
    ws_responseKey[ws_base64Length] = '\0';

    int written = sprintf_P((char *) outFrame,
            PSTR("HTTP/1.1 101 Switching Protocols\r\n"
            "%s%s\r\n"
            "%s%s\r\n"
            "Sec-WebSocket-Accept: %s\r\n\r\n"),
            upgradeField,
            websocket,
            connectionField,
            upgrade2,
            ws_responseKey);

    free(ws_responseKey);
    // if assert fail, that means, that we corrupt memory
    assert(written <= *outLength);
    *outLength = written;
}

void ws_MakeFrame(const uint8_t *data, size_t dataLength,
        uint8_t *outFrame, size_t *outLength, enum ws_FrameType frameType)
{
    assert(outFrame && *outLength);
    assert(frameType < 0x10);
    if (dataLength > 0)
        assert(data);

    outFrame[0] = 0x80 | frameType;

    if (dataLength <= 125)
    {
        outFrame[1] = dataLength;
        *outLength = 2;
    }
    else if (dataLength <= 0xFFFF)
    {
        outFrame[1] = 126;
        uint16_t payloadLength16b = 0; //htons(dataLength); //MR:
        memcpy(&outFrame[2], &payloadLength16b, 2);
        *outLength = 4;
    }
    else
    {
        assert(dataLength <= 0xFFFF);

        /* implementation for 64bit systems
        outFrame[1] = 127;
        dataLength = htonll(dataLength);
        memcpy(&outFrame[2], &dataLength, 8);
         *outLength = 10;
         */
    }
    memcpy(&outFrame[*outLength], data, dataLength);
    *outLength += dataLength;
}

static size_t ws_getPayloadLength(const uint8_t *inputFrame, size_t inputLength,
        uint8_t *payloadFieldExtraBytes, enum ws_FrameType *frameType)
{
    size_t ws_payloadLength = inputFrame[1] & 0x7F;
    *payloadFieldExtraBytes = 0;
    if ((ws_payloadLength == 0x7E && inputLength < 4) || (ws_payloadLength == 0x7F && inputLength < 10))
    {
        *frameType = WS_INCOMPLETE_FRAME;
        return 0;
    }
    if (ws_payloadLength == 0x7F && (inputFrame[3] & 0x80) != 0x0)
    {
        *frameType = WS_ERROR_FRAME;
        return 0;
    }

    if (ws_payloadLength == 0x7E)
    {
        uint16_t payloadLength16b = 0;
        *payloadFieldExtraBytes = 2;
        memcpy(&payloadLength16b, &inputFrame[2], *payloadFieldExtraBytes);
        ws_payloadLength = 0; //ntohs(payloadLength16b); //MR:
    }
    else if (ws_payloadLength == 0x7F)
    {
        *frameType = WS_ERROR_FRAME;
        return 0;

        /* // implementation for 64bit systems
        uint64_t payloadLength64b = 0;
         *payloadFieldExtraBytes = 8;
        memcpy(&payloadLength64b, &inputFrame[2], *payloadFieldExtraBytes);
        if (payloadLength64b > SIZE_MAX) {
         *frameType = WS_ERROR_FRAME;
            return 0;
        }
        payloadLength = (size_t)ntohll(payloadLength64b);
         */
    }

    return ws_payloadLength;
}

enum ws_FrameType ws_ParseInputFrame(uint8_t *inputFrame, size_t inputLength,
        uint8_t **dataPtr, size_t *dataLength)
{
    assert(inputFrame && inputLength);
                
    if (inputLength < 2)
        return WS_INCOMPLETE_FRAME;

    if ((inputFrame[0] & 0x70) != 0x0) // checks extensions off
        return WS_ERROR_FRAME;
    if ((inputFrame[0] & 0x80) != 0x80) // we haven't continuation frames support
        return WS_ERROR_FRAME; // so, fin flag must be set
    if ((inputFrame[1] & 0x80) != 0x80) // checks masking bit
        return WS_ERROR_FRAME;

    uint8_t opcode = inputFrame[0] & 0x0F;
    if (opcode == WS_TEXT_FRAME ||
        opcode == WS_BINARY_FRAME ||
        opcode == WS_CLOSING_FRAME ||
        opcode == WS_PING_FRAME ||
        opcode == WS_PONG_FRAME
        )
    {
        enum ws_FrameType frameType = opcode;

        uint8_t payloadFieldExtraBytes = 0;
        size_t payloadLength = ws_getPayloadLength(inputFrame, inputLength,
                &payloadFieldExtraBytes, &frameType);
        if (payloadLength > 0)
        {
            if (payloadLength + 6 + payloadFieldExtraBytes > inputLength) // 4-maskingKey, 2-header
                return WS_INCOMPLETE_FRAME;
            uint8_t *maskingKey = &inputFrame[2 + payloadFieldExtraBytes];

            assert(payloadLength == inputLength - 6 - payloadFieldExtraBytes);

            *dataPtr = &inputFrame[2 + payloadFieldExtraBytes + 4];
            *dataLength = payloadLength;

            size_t i;
            for (i = 0; i < *dataLength; i++)
            {
                (*dataPtr)[i] = (*dataPtr)[i] ^ maskingKey[i % 4];
            }
        }
        return frameType;
    }

    return WS_ERROR_FRAME;
}

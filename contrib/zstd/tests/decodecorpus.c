/**
 * Copyright (c) 2017-present, Facebook, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "zstd.h"
#include "zstd_internal.h"
#include "mem.h"

// Direct access to internal compression functions is required
#include "zstd_compress.c"

#define XXH_STATIC_LINKING_ONLY
#include "xxhash.h"     /* XXH64 */

#ifndef MIN
    #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX_PATH
    #ifdef PATH_MAX
        #define MAX_PATH PATH_MAX
    #else
        #define MAX_PATH 256
    #endif
#endif

/*-************************************
*  DISPLAY Macros
**************************************/
#define DISPLAY(...)          fprintf(stderr, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...)  if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static U32 g_displayLevel = 0;

#define DISPLAYUPDATE(...)                                                     \
    do {                                                                       \
        if ((clockSpan(g_displayClock) > g_refreshRate) ||                     \
            (g_displayLevel >= 4)) {                                           \
            g_displayClock = clock();                                          \
            DISPLAY(__VA_ARGS__);                                              \
            if (g_displayLevel >= 4) fflush(stderr);                           \
        }                                                                      \
    } while (0)
static const clock_t g_refreshRate = CLOCKS_PER_SEC / 6;
static clock_t g_displayClock = 0;

static clock_t clockSpan(clock_t cStart)
{
    return clock() - cStart;   /* works even when overflow; max span ~ 30mn */
}

#define CHECKERR(code)                                                         \
    do {                                                                       \
        if (ZSTD_isError(code)) {                                              \
            DISPLAY("Error occurred while generating data: %s\n",              \
                    ZSTD_getErrorName(code));                                  \
            exit(1);                                                           \
        }                                                                      \
    } while (0)

/*-*******************************************************
*  Random function
*********************************************************/
#define CLAMP(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

static unsigned RAND(unsigned* src)
{
#define RAND_rotl32(x,r) ((x << r) | (x >> (32 - r)))
    static const U32 prime1 = 2654435761U;
    static const U32 prime2 = 2246822519U;
    U32 rand32 = *src;
    rand32 *= prime1;
    rand32 += prime2;
    rand32  = RAND_rotl32(rand32, 13);
    *src = rand32;
    return RAND_rotl32(rand32, 27);
#undef RAND_rotl32
}

#define DISTSIZE (8192)

/* Write `size` bytes into `ptr`, all of which are less than or equal to `maxSymb` */
static void RAND_bufferMaxSymb(U32* seed, void* ptr, size_t size, int maxSymb)
{
    size_t i;
    BYTE* op = ptr;

    for (i = 0; i < size; i++) {
        op[i] = (BYTE) (RAND(seed) % (maxSymb + 1));
    }
}

/* Write `size` random bytes into `ptr` */
static void RAND_buffer(U32* seed, void* ptr, size_t size)
{
    size_t i;
    BYTE* op = ptr;

    for (i = 0; i + 4 <= size; i += 4) {
        MEM_writeLE32(op + i, RAND(seed));
    }
    for (; i < size; i++) {
        op[i] = RAND(seed) & 0xff;
    }
}

/* Write `size` bytes into `ptr` following the distribution `dist` */
static void RAND_bufferDist(U32* seed, BYTE* dist, void* ptr, size_t size)
{
    size_t i;
    BYTE* op = ptr;

    for (i = 0; i < size; i++) {
        op[i] = dist[RAND(seed) % DISTSIZE];
    }
}

/* Generate a random distribution where the frequency of each symbol follows a
 * geometric distribution defined by `weight`
 * `dist` should have size at least `DISTSIZE` */
static void RAND_genDist(U32* seed, BYTE* dist, double weight)
{
    size_t i = 0;
    size_t statesLeft = DISTSIZE;
    BYTE symb = (BYTE) (RAND(seed) % 256);
    BYTE step = (BYTE) ((RAND(seed) % 256) | 1); /* force it to be odd so it's relatively prime to 256 */

    while (i < DISTSIZE) {
        size_t states = ((size_t)(weight * statesLeft)) + 1;
        size_t j;
        for (j = 0; j < states && i < DISTSIZE; j++, i++) {
            dist[i] = symb;
        }

        symb += step;
        statesLeft -= states;
    }
}

/* Generates a random number in the range [min, max) */
static inline U32 RAND_range(U32* seed, U32 min, U32 max)
{
    return (RAND(seed) % (max-min)) + min;
}

#define ROUND(x) ((U32)(x + 0.5))

/* Generates a random number in an exponential distribution with mean `mean` */
static double RAND_exp(U32* seed, double mean)
{
    double const u = RAND(seed) / (double) UINT_MAX;
    return log(1-u) * (-mean);
}

/*-*******************************************************
*  Constants and Structs
*********************************************************/
const char *BLOCK_TYPES[] = {"raw", "rle", "compressed"};

#define MAX_DECOMPRESSED_SIZE_LOG 20
#define MAX_DECOMPRESSED_SIZE (1ULL << MAX_DECOMPRESSED_SIZE_LOG)

#define MAX_WINDOW_LOG 22 /* Recommended support is 8MB, so limit to 4MB + mantissa */
#define MAX_BLOCK_SIZE (128ULL * 1024)

#define MIN_SEQ_LEN (3)
#define MAX_NB_SEQ ((MAX_BLOCK_SIZE + MIN_SEQ_LEN - 1) / MIN_SEQ_LEN)

BYTE CONTENT_BUFFER[MAX_DECOMPRESSED_SIZE];
BYTE FRAME_BUFFER[MAX_DECOMPRESSED_SIZE * 2];
BYTE LITERAL_BUFFER[MAX_BLOCK_SIZE];

seqDef SEQUENCE_BUFFER[MAX_NB_SEQ];
BYTE SEQUENCE_LITERAL_BUFFER[MAX_BLOCK_SIZE]; /* storeSeq expects a place to copy literals to */
BYTE SEQUENCE_LLCODE[MAX_BLOCK_SIZE];
BYTE SEQUENCE_MLCODE[MAX_BLOCK_SIZE];
BYTE SEQUENCE_OFCODE[MAX_BLOCK_SIZE];

unsigned WKSP[1024];

typedef struct {
    size_t contentSize; /* 0 means unknown (unless contentSize == windowSize == 0) */
    unsigned windowSize; /* contentSize >= windowSize means single segment */
} frameHeader_t;

/* For repeat modes */
typedef struct {
    U32 rep[ZSTD_REP_NUM];

    int hufInit;
    /* the distribution used in the previous block for repeat mode */
    BYTE hufDist[DISTSIZE];
    U32 hufTable [256]; /* HUF_CElt is an incomplete type */

    int fseInit;
    FSE_CTable offcodeCTable  [FSE_CTABLE_SIZE_U32(OffFSELog, MaxOff)];
    FSE_CTable matchlengthCTable[FSE_CTABLE_SIZE_U32(MLFSELog, MaxML)];
    FSE_CTable litlengthCTable  [FSE_CTABLE_SIZE_U32(LLFSELog, MaxLL)];

    /* Symbols that were present in the previous distribution, for use with
     * set_repeat */
    BYTE litlengthSymbolSet[36];
    BYTE offsetSymbolSet[29];
    BYTE matchlengthSymbolSet[53];
} cblockStats_t;

typedef struct {
    void* data;
    void* dataStart;
    void* dataEnd;

    void* src;
    void* srcStart;
    void* srcEnd;

    frameHeader_t header;

    cblockStats_t stats;
    cblockStats_t oldStats; /* so they can be rolled back if uncompressible */
} frame_t;

/*-*******************************************************
*  Generator Functions
*********************************************************/

struct {
    int contentSize; /* force the content size to be present */
} opts; /* advanced options on generation */

/* Generate and write a random frame header */
static void writeFrameHeader(U32* seed, frame_t* frame)
{
    BYTE* const op = frame->data;
    size_t pos = 0;
    frameHeader_t fh;

    BYTE windowByte = 0;

    int singleSegment = 0;
    int contentSizeFlag = 0;
    int fcsCode = 0;

    memset(&fh, 0, sizeof(fh));

    /* generate window size */
    {
        /* Follow window algorithm from specification */
        int const exponent = RAND(seed) % (MAX_WINDOW_LOG - 10);
        int const mantissa = RAND(seed) % 8;
        windowByte = (BYTE) ((exponent << 3) | mantissa);
        fh.windowSize = (1U << (exponent + 10));
        fh.windowSize += fh.windowSize / 8 * mantissa;
    }

    {
        /* Generate random content size */
        size_t highBit;
        if (RAND(seed) & 7) {
            /* do content of at least 128 bytes */
            highBit = 1ULL << RAND_range(seed, 7, MAX_DECOMPRESSED_SIZE_LOG);
        } else if (RAND(seed) & 3) {
            /* do small content */
            highBit = 1ULL << RAND_range(seed, 0, 7);
        } else {
            /* 0 size frame */
            highBit = 0;
        }
        fh.contentSize = highBit ? highBit + (RAND(seed) % highBit) : 0;

        /* provide size sometimes */
        contentSizeFlag = opts.contentSize | (RAND(seed) & 1);

        if (contentSizeFlag && (fh.contentSize == 0 || !(RAND(seed) & 7))) {
            /* do single segment sometimes */
            fh.windowSize = (U32) fh.contentSize;
            singleSegment = 1;
        }
    }

    if (contentSizeFlag) {
        /* Determine how large fcs field has to be */
        int minFcsCode = (fh.contentSize >= 256) +
                               (fh.contentSize >= 65536 + 256) +
                               (fh.contentSize > 0xFFFFFFFFU);
        if (!singleSegment && !minFcsCode) {
            minFcsCode = 1;
        }
        fcsCode = minFcsCode + (RAND(seed) % (4 - minFcsCode));
        if (fcsCode == 1 && fh.contentSize < 256) fcsCode++;
    }

    /* write out the header */
    MEM_writeLE32(op + pos, ZSTD_MAGICNUMBER);
    pos += 4;

    {
        BYTE const frameHeaderDescriptor =
                (BYTE) ((fcsCode << 6) | (singleSegment << 5) | (1 << 2));
        op[pos++] = frameHeaderDescriptor;
    }

    if (!singleSegment) {
        op[pos++] = windowByte;
    }

    if (contentSizeFlag) {
        switch (fcsCode) {
        default: /* Impossible */
        case 0: op[pos++] = (BYTE) fh.contentSize; break;
        case 1: MEM_writeLE16(op + pos, (U16) (fh.contentSize - 256)); pos += 2; break;
        case 2: MEM_writeLE32(op + pos, (U32) fh.contentSize); pos += 4; break;
        case 3: MEM_writeLE64(op + pos, (U64) fh.contentSize); pos += 8; break;
        }
    }

    DISPLAYLEVEL(2, " frame content size:\t%u\n", (U32)fh.contentSize);
    DISPLAYLEVEL(2, " frame window size:\t%u\n", fh.windowSize);
    DISPLAYLEVEL(2, " content size flag:\t%d\n", contentSizeFlag);
    DISPLAYLEVEL(2, " single segment flag:\t%d\n", singleSegment);

    frame->data = op + pos;
    frame->header = fh;
}

/* Write a literal block in either raw or RLE form, return the literals size */
static size_t writeLiteralsBlockSimple(U32* seed, frame_t* frame, size_t contentSize)
{
    BYTE* op = (BYTE*)frame->data;
    int const type = RAND(seed) % 2;
    int const sizeFormatDesc = RAND(seed) % 8;
    size_t litSize;
    size_t maxLitSize = MIN(contentSize, MAX_BLOCK_SIZE);

    if (sizeFormatDesc == 0) {
        /* Size_FormatDesc = ?0 */
        maxLitSize = MIN(maxLitSize, 31);
    } else if (sizeFormatDesc <= 4) {
        /* Size_FormatDesc = 01 */
        maxLitSize = MIN(maxLitSize, 4095);
    } else {
        /* Size_Format = 11 */
        maxLitSize = MIN(maxLitSize, 1048575);
    }

    litSize = RAND(seed) % (maxLitSize + 1);
    if (frame->src == frame->srcStart && litSize == 0) {
        litSize = 1; /* no empty literals if there's nothing preceding this block */
    }
    if (litSize + 3 > contentSize) {
        litSize = contentSize; /* no matches shorter than 3 are allowed */
    }
    /* use smallest size format that fits */
    if (litSize < 32) {
        op[0] = (type | (0 << 2) | (litSize << 3)) & 0xff;
        op += 1;
    } else if (litSize < 4096) {
        op[0] = (type | (1 << 2) | (litSize << 4)) & 0xff;
        op[1] = (litSize >> 4) & 0xff;
        op += 2;
    } else {
        op[0] = (type | (3 << 2) | (litSize << 4)) & 0xff;
        op[1] = (litSize >> 4) & 0xff;
        op[2] = (litSize >> 12) & 0xff;
        op += 3;
    }

    if (type == 0) {
        /* Raw literals */
        DISPLAYLEVEL(4, "   raw literals\n");

        RAND_buffer(seed, LITERAL_BUFFER, litSize);
        memcpy(op, LITERAL_BUFFER, litSize);
        op += litSize;
    } else {
        /* RLE literals */
        BYTE const symb = (BYTE) (RAND(seed) % 256);

        DISPLAYLEVEL(4, "   rle literals: 0x%02x\n", (U32)symb);

        memset(LITERAL_BUFFER, symb, litSize);
        op[0] = symb;
        op++;
    }

    frame->data = op;

    return litSize;
}

/* Generate a Huffman header for the given source */
static size_t writeHufHeader(U32* seed, HUF_CElt* hufTable, void* dst, size_t dstSize,
                                 const void* src, size_t srcSize)
{
    BYTE* const ostart = (BYTE*)dst;
    BYTE* op = ostart;

    unsigned huffLog = 11;
    U32 maxSymbolValue = 255;

    U32 count[HUF_SYMBOLVALUE_MAX+1];

    /* Scan input and build symbol stats */
    {   size_t const largest = FSE_count_wksp (count, &maxSymbolValue, (const BYTE*)src, srcSize, WKSP);
        if (largest == srcSize) { *ostart = ((const BYTE*)src)[0]; return 0; }   /* single symbol, rle */
        if (largest <= (srcSize >> 7)+1) return 0;   /* Fast heuristic : not compressible enough */
    }

    /* Build Huffman Tree */
    /* Max Huffman log is 11, min is highbit(maxSymbolValue)+1 */
    huffLog = RAND_range(seed, ZSTD_highbit32(maxSymbolValue)+1, huffLog+1);
    DISPLAYLEVEL(6, "     huffman log: %u\n", huffLog);
    {   size_t const maxBits = HUF_buildCTable_wksp (hufTable, count, maxSymbolValue, huffLog, WKSP, sizeof(WKSP));
        CHECKERR(maxBits);
        huffLog = (U32)maxBits;
    }

    /* Write table description header */
    {   size_t const hSize = HUF_writeCTable (op, dstSize, hufTable, maxSymbolValue, huffLog);
        if (hSize + 12 >= srcSize) return 0;   /* not useful to try compression */
        op += hSize;
    }

    return op - ostart;
}

/* Write a Huffman coded literals block and return the litearls size */
static size_t writeLiteralsBlockCompressed(U32* seed, frame_t* frame, size_t contentSize)
{
    BYTE* origop = (BYTE*)frame->data;
    BYTE* opend = (BYTE*)frame->dataEnd;
    BYTE* op;
    BYTE* const ostart = origop;
    int const sizeFormat = RAND(seed) % 4;
    size_t litSize;
    size_t hufHeaderSize = 0;
    size_t compressedSize = 0;
    size_t maxLitSize = MIN(contentSize-3, MAX_BLOCK_SIZE);

    symbolEncodingType_e hType;

    if (contentSize < 64) {
        /* make sure we get reasonably-sized literals for compression */
        return ERROR(GENERIC);
    }

    DISPLAYLEVEL(4, "   compressed literals\n");

    switch (sizeFormat) {
    case 0: /* fall through, size is the same as case 1 */
    case 1:
        maxLitSize = MIN(maxLitSize, 1023);
        origop += 3;
        break;
    case 2:
        maxLitSize = MIN(maxLitSize, 16383);
        origop += 4;
        break;
    case 3:
        maxLitSize = MIN(maxLitSize, 262143);
        origop += 5;
        break;
    default:; /* impossible */
    }

    do {
        op = origop;
        do {
            litSize = RAND(seed) % (maxLitSize + 1);
        } while (litSize < 32); /* avoid small literal sizes */
        if (litSize + 3 > contentSize) {
            litSize = contentSize; /* no matches shorter than 3 are allowed */
        }

        /* most of the time generate a new distribution */
        if ((RAND(seed) & 3) || !frame->stats.hufInit) {
            do {
                if (RAND(seed) & 3) {
                    /* add 10 to ensure some compressability */
                    double const weight = ((RAND(seed) % 90) + 10) / 100.0;

                    DISPLAYLEVEL(5, "    distribution weight: %d%%\n",
                                 (int)(weight * 100));

                    RAND_genDist(seed, frame->stats.hufDist, weight);
                } else {
                    /* sometimes do restricted range literals to force
                     * non-huffman headers */
                    DISPLAYLEVEL(5, "    small range literals\n");
                    RAND_bufferMaxSymb(seed, frame->stats.hufDist, DISTSIZE,
                                       15);
                }
                RAND_bufferDist(seed, frame->stats.hufDist, LITERAL_BUFFER,
                                litSize);

                /* generate the header from the distribution instead of the
                 * actual data to avoid bugs with symbols that were in the
                 * distribution but never showed up in the output */
                hufHeaderSize = writeHufHeader(
                        seed, (HUF_CElt*)frame->stats.hufTable, op, opend - op,
                        frame->stats.hufDist, DISTSIZE);
                CHECKERR(hufHeaderSize);
                /* repeat until a valid header is written */
            } while (hufHeaderSize == 0);
            op += hufHeaderSize;
            hType = set_compressed;

            frame->stats.hufInit = 1;
        } else {
            /* repeat the distribution/table from last time */
            DISPLAYLEVEL(5, "    huffman repeat stats\n");
            RAND_bufferDist(seed, frame->stats.hufDist, LITERAL_BUFFER,
                            litSize);
            hufHeaderSize = 0;
            hType = set_repeat;
        }

        do {
            compressedSize =
                    sizeFormat == 0
                            ? HUF_compress1X_usingCTable(
                                      op, opend - op, LITERAL_BUFFER, litSize,
                                      (HUF_CElt*)frame->stats.hufTable)
                            : HUF_compress4X_usingCTable(
                                      op, opend - op, LITERAL_BUFFER, litSize,
                                      (HUF_CElt*)frame->stats.hufTable);
            CHECKERR(compressedSize);
            /* this only occurs when it could not compress or similar */
        } while (compressedSize <= 0);

        op += compressedSize;

        compressedSize += hufHeaderSize;
        DISPLAYLEVEL(5, "    regenerated size: %u\n", (U32)litSize);
        DISPLAYLEVEL(5, "    compressed size: %u\n", (U32)compressedSize);
        if (compressedSize >= litSize) {
            DISPLAYLEVEL(5, "     trying again\n");
            /* if we have to try again, reset the stats so we don't accidentally
             * try to repeat a distribution we just made */
            frame->stats = frame->oldStats;
        } else {
            break;
        }
    } while (1);

    /* write header */
    switch (sizeFormat) {
    case 0: /* fall through, size is the same as case 1 */
    case 1: {
        U32 const header = hType | (sizeFormat << 2) | ((U32)litSize << 4) |
                           ((U32)compressedSize << 14);
        MEM_writeLE24(ostart, header);
        break;
    }
    case 2: {
        U32 const header = hType | (sizeFormat << 2) | ((U32)litSize << 4) |
                           ((U32)compressedSize << 18);
        MEM_writeLE32(ostart, header);
        break;
    }
    case 3: {
        U32 const header = hType | (sizeFormat << 2) | ((U32)litSize << 4) |
                           ((U32)compressedSize << 22);
        MEM_writeLE32(ostart, header);
        ostart[4] = (BYTE)(compressedSize >> 10);
        break;
    }
    default:; /* impossible */
    }

    frame->data = op;
    return litSize;
}

static size_t writeLiteralsBlock(U32* seed, frame_t* frame, size_t contentSize)
{
    /* only do compressed for larger segments to avoid compressibility issues */
    if (RAND(seed) & 7 && contentSize >= 64) {
        return writeLiteralsBlockCompressed(seed, frame, contentSize);
    } else {
        return writeLiteralsBlockSimple(seed, frame, contentSize);
    }
}

static inline void initSeqStore(seqStore_t *seqStore) {
    seqStore->sequencesStart = SEQUENCE_BUFFER;
    seqStore->litStart = SEQUENCE_LITERAL_BUFFER;
    seqStore->llCode = SEQUENCE_LLCODE;
    seqStore->mlCode = SEQUENCE_MLCODE;
    seqStore->ofCode = SEQUENCE_OFCODE;

    ZSTD_resetSeqStore(seqStore);
}

/* Randomly generate sequence commands */
static U32 generateSequences(U32* seed, frame_t* frame, seqStore_t* seqStore,
                                size_t contentSize, size_t literalsSize)
{
    /* The total length of all the matches */
    size_t const remainingMatch = contentSize - literalsSize;
    size_t excessMatch = 0;
    U32 numSequences = 0;

    U32 i;


    const BYTE* literals = LITERAL_BUFFER;
    BYTE* srcPtr = frame->src;

    if (literalsSize != contentSize) {
        /* each match must be at least MIN_SEQ_LEN, so this is the maximum
         * number of sequences we can have */
        U32 const maxSequences = (U32)remainingMatch / MIN_SEQ_LEN;
        numSequences = (RAND(seed) % maxSequences) + 1;

        /* the extra match lengths we have to allocate to each sequence */
        excessMatch = remainingMatch - numSequences * MIN_SEQ_LEN;
    }

    DISPLAYLEVEL(5, "    total match lengths: %u\n", (U32)remainingMatch);

    for (i = 0; i < numSequences; i++) {
        /* Generate match and literal lengths by exponential distribution to
         * ensure nice numbers */
        U32 matchLen =
                MIN_SEQ_LEN +
                ROUND(RAND_exp(seed, excessMatch / (double)(numSequences - i)));
        U32 literalLen =
                (RAND(seed) & 7)
                        ? ROUND(RAND_exp(seed,
                                         literalsSize /
                                                 (double)(numSequences - i)))
                        : 0;
        /* actual offset, code to send, and point to copy up to when shifting
         * codes in the repeat offsets history */
        U32 offset, offsetCode, repIndex;

        /* bounds checks */
        matchLen = (U32) MIN(matchLen, excessMatch + MIN_SEQ_LEN);
        literalLen = MIN(literalLen, (U32) literalsSize);
        if (i == 0 && srcPtr == frame->srcStart && literalLen == 0) literalLen = 1;
        if (i + 1 == numSequences) matchLen = MIN_SEQ_LEN + (U32) excessMatch;

        memcpy(srcPtr, literals, literalLen);
        srcPtr += literalLen;

        do {
            if (RAND(seed) & 7) {
                /* do a normal offset */
                offset = (RAND(seed) %
                          MIN(frame->header.windowSize,
                              (size_t)((BYTE*)srcPtr - (BYTE*)frame->srcStart))) +
                         1;
                offsetCode = offset + ZSTD_REP_MOVE;
                repIndex = 2;
            } else {
                /* do a repeat offset */
                offsetCode = RAND(seed) % 3;
                if (literalLen > 0) {
                    offset = frame->stats.rep[offsetCode];
                    repIndex = offsetCode;
                } else {
                    /* special case */
                    offset = offsetCode == 2 ? frame->stats.rep[0] - 1
                                           : frame->stats.rep[offsetCode + 1];
                    repIndex = MIN(2, offsetCode + 1);
                }
            }
        } while (offset > (size_t)((BYTE*)srcPtr - (BYTE*)frame->srcStart) || offset == 0);

        {   size_t j;
            for (j = 0; j < matchLen; j++) {
                *srcPtr = *(srcPtr-offset);
                srcPtr++;
            }
        }

        {   int r;
            for (r = repIndex; r > 0; r--) {
                frame->stats.rep[r] = frame->stats.rep[r - 1];
            }
            frame->stats.rep[0] = offset;
        }

        DISPLAYLEVEL(6, "      LL: %5u OF: %5u ML: %5u", literalLen, offset, matchLen);
        DISPLAYLEVEL(7, " srcPos: %8u seqNb: %3u",
                     (U32)((BYTE*)srcPtr - (BYTE*)frame->srcStart), i);
        DISPLAYLEVEL(6, "\n");
        if (offsetCode < 3) {
            DISPLAYLEVEL(7, "        repeat offset: %d\n", repIndex);
        }
        /* use libzstd sequence handling */
        ZSTD_storeSeq(seqStore, literalLen, literals, offsetCode,
                      matchLen - MINMATCH);

        literalsSize -= literalLen;
        excessMatch -= (matchLen - MIN_SEQ_LEN);
        literals += literalLen;
    }

    memcpy(srcPtr, literals, literalsSize);
    srcPtr += literalsSize;
    DISPLAYLEVEL(6, "      excess literals: %5u", (U32)literalsSize);
    DISPLAYLEVEL(7, " srcPos: %8u", (U32)((BYTE*)srcPtr - (BYTE*)frame->srcStart));
    DISPLAYLEVEL(6, "\n");

    return numSequences;
}

static void initSymbolSet(const BYTE* symbols, size_t len, BYTE* set, BYTE maxSymbolValue)
{
    size_t i;

    memset(set, 0, (size_t)maxSymbolValue+1);

    for (i = 0; i < len; i++) {
        set[symbols[i]] = 1;
    }
}

static int isSymbolSubset(const BYTE* symbols, size_t len, const BYTE* set, BYTE maxSymbolValue)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (symbols[i] > maxSymbolValue || !set[symbols[i]]) {
            return 0;
        }
    }
    return 1;
}

static size_t writeSequences(U32* seed, frame_t* frame, seqStore_t* seqStorePtr,
                             size_t nbSeq)
{
    /* This code is mostly copied from ZSTD_compressSequences in zstd_compress.c */
    U32 count[MaxSeq+1];
    S16 norm[MaxSeq+1];
    FSE_CTable* CTable_LitLength = frame->stats.litlengthCTable;
    FSE_CTable* CTable_OffsetBits = frame->stats.offcodeCTable;
    FSE_CTable* CTable_MatchLength = frame->stats.matchlengthCTable;
    U32 LLtype, Offtype, MLtype;   /* compressed, raw or rle */
    const seqDef* const sequences = seqStorePtr->sequencesStart;
    const BYTE* const ofCodeTable = seqStorePtr->ofCode;
    const BYTE* const llCodeTable = seqStorePtr->llCode;
    const BYTE* const mlCodeTable = seqStorePtr->mlCode;
    BYTE* const oend = (BYTE*)frame->dataEnd;
    BYTE* op = (BYTE*)frame->data;
    BYTE* seqHead;
    BYTE scratchBuffer[1<<MAX(MLFSELog,LLFSELog)];

    /* literals compressing block removed so that can be done separately */

    /* Sequences Header */
    if ((oend-op) < 3 /*max nbSeq Size*/ + 1 /*seqHead */) return ERROR(dstSize_tooSmall);
    if (nbSeq < 0x7F) *op++ = (BYTE)nbSeq;
    else if (nbSeq < LONGNBSEQ) op[0] = (BYTE)((nbSeq>>8) + 0x80), op[1] = (BYTE)nbSeq, op+=2;
    else op[0]=0xFF, MEM_writeLE16(op+1, (U16)(nbSeq - LONGNBSEQ)), op+=3;

    /* seqHead : flags for FSE encoding type */
    seqHead = op++;

    if (nbSeq==0) {
        frame->data = op;

        return 0;
    }

    /* convert length/distances into codes */
    ZSTD_seqToCodes(seqStorePtr);

    /* CTable for Literal Lengths */
    {   U32 max = MaxLL;
        size_t const mostFrequent = FSE_countFast_wksp(count, &max, llCodeTable, nbSeq, WKSP);
        if (mostFrequent == nbSeq) {
            /* do RLE if we have the chance */
            *op++ = llCodeTable[0];
            FSE_buildCTable_rle(CTable_LitLength, (BYTE)max);
            LLtype = set_rle;
        } else if (frame->stats.fseInit && !(RAND(seed) & 3) &&
                   isSymbolSubset(llCodeTable, nbSeq,
                                  frame->stats.litlengthSymbolSet, 35)) {
            /* maybe do repeat mode if we're allowed to */
            LLtype = set_repeat;
        } else if (!(RAND(seed) & 3)) {
            /* maybe use the default distribution */
            FSE_buildCTable_wksp(CTable_LitLength, LL_defaultNorm, MaxLL, LL_defaultNormLog, scratchBuffer, sizeof(scratchBuffer));
            LLtype = set_basic;
        } else {
            /* fall back on a full table */
            size_t nbSeq_1 = nbSeq;
            const U32 tableLog = FSE_optimalTableLog(LLFSELog, nbSeq, max);
            if (count[llCodeTable[nbSeq-1]]>1) { count[llCodeTable[nbSeq-1]]--; nbSeq_1--; }
            FSE_normalizeCount(norm, tableLog, count, nbSeq_1, max);
            { size_t const NCountSize = FSE_writeNCount(op, oend-op, norm, max, tableLog);   /* overflow protected */
              if (FSE_isError(NCountSize)) return ERROR(GENERIC);
              op += NCountSize; }
            FSE_buildCTable_wksp(CTable_LitLength, norm, max, tableLog, scratchBuffer, sizeof(scratchBuffer));
            LLtype = set_compressed;
    }   }

    /* CTable for Offsets */
    /* see Literal Lengths for descriptions of mode choices */
    {   U32 max = MaxOff;
        size_t const mostFrequent = FSE_countFast_wksp(count, &max, ofCodeTable, nbSeq, WKSP);
        if (mostFrequent == nbSeq) {
            *op++ = ofCodeTable[0];
            FSE_buildCTable_rle(CTable_OffsetBits, (BYTE)max);
            Offtype = set_rle;
        } else if (frame->stats.fseInit && !(RAND(seed) & 3) &&
                   isSymbolSubset(ofCodeTable, nbSeq,
                                  frame->stats.offsetSymbolSet, 28)) {
            Offtype = set_repeat;
        } else if (!(RAND(seed) & 3)) {
            FSE_buildCTable_wksp(CTable_OffsetBits, OF_defaultNorm, MaxOff, OF_defaultNormLog, scratchBuffer, sizeof(scratchBuffer));
            Offtype = set_basic;
        } else {
            size_t nbSeq_1 = nbSeq;
            const U32 tableLog = FSE_optimalTableLog(OffFSELog, nbSeq, max);
            if (count[ofCodeTable[nbSeq-1]]>1) { count[ofCodeTable[nbSeq-1]]--; nbSeq_1--; }
            FSE_normalizeCount(norm, tableLog, count, nbSeq_1, max);
            { size_t const NCountSize = FSE_writeNCount(op, oend-op, norm, max, tableLog);   /* overflow protected */
              if (FSE_isError(NCountSize)) return ERROR(GENERIC);
              op += NCountSize; }
            FSE_buildCTable_wksp(CTable_OffsetBits, norm, max, tableLog, scratchBuffer, sizeof(scratchBuffer));
            Offtype = set_compressed;
    }   }

    /* CTable for MatchLengths */
    /* see Literal Lengths for descriptions of mode choices */
    {   U32 max = MaxML;
        size_t const mostFrequent = FSE_countFast_wksp(count, &max, mlCodeTable, nbSeq, WKSP);
        if (mostFrequent == nbSeq) {
            *op++ = *mlCodeTable;
            FSE_buildCTable_rle(CTable_MatchLength, (BYTE)max);
            MLtype = set_rle;
        } else if (frame->stats.fseInit && !(RAND(seed) & 3) &&
                   isSymbolSubset(mlCodeTable, nbSeq,
                                  frame->stats.matchlengthSymbolSet, 52)) {
            MLtype = set_repeat;
        } else if (!(RAND(seed) & 3)) {
            /* sometimes do default distribution */
            FSE_buildCTable_wksp(CTable_MatchLength, ML_defaultNorm, MaxML, ML_defaultNormLog, scratchBuffer, sizeof(scratchBuffer));
            MLtype = set_basic;
        } else {
            /* fall back on table */
            size_t nbSeq_1 = nbSeq;
            const U32 tableLog = FSE_optimalTableLog(MLFSELog, nbSeq, max);
            if (count[mlCodeTable[nbSeq-1]]>1) { count[mlCodeTable[nbSeq-1]]--; nbSeq_1--; }
            FSE_normalizeCount(norm, tableLog, count, nbSeq_1, max);
            { size_t const NCountSize = FSE_writeNCount(op, oend-op, norm, max, tableLog);   /* overflow protected */
              if (FSE_isError(NCountSize)) return ERROR(GENERIC);
              op += NCountSize; }
            FSE_buildCTable_wksp(CTable_MatchLength, norm, max, tableLog, scratchBuffer, sizeof(scratchBuffer));
            MLtype = set_compressed;
    }   }
    frame->stats.fseInit = 1;
    initSymbolSet(llCodeTable, nbSeq, frame->stats.litlengthSymbolSet, 35);
    initSymbolSet(ofCodeTable, nbSeq, frame->stats.offsetSymbolSet, 28);
    initSymbolSet(mlCodeTable, nbSeq, frame->stats.matchlengthSymbolSet, 52);

    DISPLAYLEVEL(5, "    LL type: %d OF type: %d ML type: %d\n", LLtype, Offtype, MLtype);

    *seqHead = (BYTE)((LLtype<<6) + (Offtype<<4) + (MLtype<<2));

    /* Encoding Sequences */
    {   BIT_CStream_t blockStream;
        FSE_CState_t  stateMatchLength;
        FSE_CState_t  stateOffsetBits;
        FSE_CState_t  stateLitLength;

        CHECK_E(BIT_initCStream(&blockStream, op, oend-op), dstSize_tooSmall); /* not enough space remaining */

        /* first symbols */
        FSE_initCState2(&stateMatchLength, CTable_MatchLength, mlCodeTable[nbSeq-1]);
        FSE_initCState2(&stateOffsetBits,  CTable_OffsetBits,  ofCodeTable[nbSeq-1]);
        FSE_initCState2(&stateLitLength,   CTable_LitLength,   llCodeTable[nbSeq-1]);
        BIT_addBits(&blockStream, sequences[nbSeq-1].litLength, LL_bits[llCodeTable[nbSeq-1]]);
        if (MEM_32bits()) BIT_flushBits(&blockStream);
        BIT_addBits(&blockStream, sequences[nbSeq-1].matchLength, ML_bits[mlCodeTable[nbSeq-1]]);
        if (MEM_32bits()) BIT_flushBits(&blockStream);
        BIT_addBits(&blockStream, sequences[nbSeq-1].offset, ofCodeTable[nbSeq-1]);
        BIT_flushBits(&blockStream);

        {   size_t n;
            for (n=nbSeq-2 ; n<nbSeq ; n--) {      /* intentional underflow */
                BYTE const llCode = llCodeTable[n];
                BYTE const ofCode = ofCodeTable[n];
                BYTE const mlCode = mlCodeTable[n];
                U32  const llBits = LL_bits[llCode];
                U32  const ofBits = ofCode;                                     /* 32b*/  /* 64b*/
                U32  const mlBits = ML_bits[mlCode];
                                                                                /* (7)*/  /* (7)*/
                FSE_encodeSymbol(&blockStream, &stateOffsetBits, ofCode);       /* 15 */  /* 15 */
                FSE_encodeSymbol(&blockStream, &stateMatchLength, mlCode);      /* 24 */  /* 24 */
                if (MEM_32bits()) BIT_flushBits(&blockStream);                  /* (7)*/
                FSE_encodeSymbol(&blockStream, &stateLitLength, llCode);        /* 16 */  /* 33 */
                if (MEM_32bits() || (ofBits+mlBits+llBits >= 64-7-(LLFSELog+MLFSELog+OffFSELog)))
                    BIT_flushBits(&blockStream);                                /* (7)*/
                BIT_addBits(&blockStream, sequences[n].litLength, llBits);
                if (MEM_32bits() && ((llBits+mlBits)>24)) BIT_flushBits(&blockStream);
                BIT_addBits(&blockStream, sequences[n].matchLength, mlBits);
                if (MEM_32bits()) BIT_flushBits(&blockStream);                  /* (7)*/
                BIT_addBits(&blockStream, sequences[n].offset, ofBits);         /* 31 */
                BIT_flushBits(&blockStream);                                    /* (7)*/
        }   }

        FSE_flushCState(&blockStream, &stateMatchLength);
        FSE_flushCState(&blockStream, &stateOffsetBits);
        FSE_flushCState(&blockStream, &stateLitLength);

        {   size_t const streamSize = BIT_closeCStream(&blockStream);
            if (streamSize==0) return ERROR(dstSize_tooSmall);   /* not enough space */
            op += streamSize;
    }   }

    frame->data = op;

    return 0;
}

static size_t writeSequencesBlock(U32* seed, frame_t* frame, size_t contentSize,
                                  size_t literalsSize)
{
    seqStore_t seqStore;
    size_t numSequences;


    initSeqStore(&seqStore);

    /* randomly generate sequences */
    numSequences = generateSequences(seed, frame, &seqStore, contentSize, literalsSize);
    /* write them out to the frame data */
    CHECKERR(writeSequences(seed, frame, &seqStore, numSequences));

    return numSequences;
}

static size_t writeCompressedBlock(U32* seed, frame_t* frame, size_t contentSize)
{
    BYTE* const blockStart = (BYTE*)frame->data;
    size_t literalsSize;
    size_t nbSeq;

    DISPLAYLEVEL(4, "  compressed block:\n");

    literalsSize = writeLiteralsBlock(seed, frame, contentSize);

    DISPLAYLEVEL(4, "   literals size: %u\n", (U32)literalsSize);

    nbSeq = writeSequencesBlock(seed, frame, contentSize, literalsSize);

    DISPLAYLEVEL(4, "   number of sequences: %u\n", (U32)nbSeq);

    return (BYTE*)frame->data - blockStart;
}

static void writeBlock(U32* seed, frame_t* frame, size_t contentSize,
                       int lastBlock)
{
    int const blockTypeDesc = RAND(seed) % 8;
    size_t blockSize;
    int blockType;

    BYTE *const header = (BYTE*)frame->data;
    BYTE *op = header + 3;

    DISPLAYLEVEL(3, " block:\n");
    DISPLAYLEVEL(3, "  block content size: %u\n", (U32)contentSize);
    DISPLAYLEVEL(3, "  last block: %s\n", lastBlock ? "yes" : "no");

    if (blockTypeDesc == 0) {
        /* Raw data frame */

        RAND_buffer(seed, frame->src, contentSize);
        memcpy(op, frame->src, contentSize);

        op += contentSize;
        blockType = 0;
        blockSize = contentSize;
    } else if (blockTypeDesc == 1) {
        /* RLE */
        BYTE const symbol = RAND(seed) & 0xff;

        op[0] = symbol;
        memset(frame->src, symbol, contentSize);

        op++;
        blockType = 1;
        blockSize = contentSize;
    } else {
        /* compressed, most common */
        size_t compressedSize;
        blockType = 2;

        frame->oldStats = frame->stats;

        frame->data = op;
        compressedSize = writeCompressedBlock(seed, frame, contentSize);
        if (compressedSize > contentSize) {
            blockType = 0;
            memcpy(op, frame->src, contentSize);

            op += contentSize;
            blockSize = contentSize; /* fall back on raw block if data doesn't
                                        compress */

            frame->stats = frame->oldStats; /* don't update the stats */
        } else {
            op += compressedSize;
            blockSize = compressedSize;
        }
    }
    frame->src = (BYTE*)frame->src + contentSize;

    DISPLAYLEVEL(3, "  block type: %s\n", BLOCK_TYPES[blockType]);
    DISPLAYLEVEL(3, "  block size field: %u\n", (U32)blockSize);

    header[0] = (BYTE) ((lastBlock | (blockType << 1) | (blockSize << 3)) & 0xff);
    MEM_writeLE16(header + 1, (U16) (blockSize >> 5));

    frame->data = op;
}

static void writeBlocks(U32* seed, frame_t* frame)
{
    size_t contentLeft = frame->header.contentSize;
    size_t const maxBlockSize = MIN(MAX_BLOCK_SIZE, frame->header.windowSize);
    while (1) {
        /* 1 in 4 chance of ending frame */
        int const lastBlock = contentLeft > maxBlockSize ? 0 : !(RAND(seed) & 3);
        size_t blockContentSize;
        if (lastBlock) {
            blockContentSize = contentLeft;
        } else {
            if (contentLeft > 0 && (RAND(seed) & 7)) {
                /* some variable size blocks */
                blockContentSize = RAND(seed) % (MIN(maxBlockSize, contentLeft)+1);
            } else if (contentLeft > maxBlockSize && (RAND(seed) & 1)) {
                /* some full size blocks */
                blockContentSize = maxBlockSize;
            } else {
                /* some empty blocks */
                blockContentSize = 0;
            }
        }

        writeBlock(seed, frame, blockContentSize, lastBlock);

        contentLeft -= blockContentSize;
        if (lastBlock) break;
    }
}

static void writeChecksum(frame_t* frame)
{
    /* write checksum so implementations can verify their output */
    U64 digest = XXH64(frame->srcStart, (BYTE*)frame->src-(BYTE*)frame->srcStart, 0);
    DISPLAYLEVEL(2, "  checksum: %08x\n", (U32)digest);
    MEM_writeLE32(frame->data, (U32)digest);
    frame->data = (BYTE*)frame->data + 4;
}

static void outputBuffer(const void* buf, size_t size, const char* const path)
{
    /* write data out to file */
    const BYTE* ip = (const BYTE*)buf;
    FILE* out;
    if (path) {
        out = fopen(path, "wb");
    } else {
        out = stdout;
    }
    if (!out) {
        fprintf(stderr, "Failed to open file at %s: ", path);
        perror(NULL);
        exit(1);
    }

    {
        size_t fsize = size;
        size_t written = 0;
        while (written < fsize) {
            written += fwrite(ip + written, 1, fsize - written, out);
            if (ferror(out)) {
                fprintf(stderr, "Failed to write to file at %s: ", path);
                perror(NULL);
                exit(1);
            }
        }
    }

    if (path) {
        fclose(out);
    }
}

static void initFrame(frame_t* fr)
{
    memset(fr, 0, sizeof(*fr));
    fr->data = fr->dataStart = FRAME_BUFFER;
    fr->dataEnd = FRAME_BUFFER + sizeof(FRAME_BUFFER);
    fr->src = fr->srcStart = CONTENT_BUFFER;
    fr->srcEnd = CONTENT_BUFFER + sizeof(CONTENT_BUFFER);

    /* init repeat codes */
    fr->stats.rep[0] = 1;
    fr->stats.rep[1] = 4;
    fr->stats.rep[2] = 8;
}

/* Return the final seed */
static U32 generateFrame(U32 seed, frame_t* fr)
{
    /* generate a complete frame */
    DISPLAYLEVEL(1, "frame seed: %u\n", seed);

    initFrame(fr);

    writeFrameHeader(&seed, fr);
    writeBlocks(&seed, fr);
    writeChecksum(fr);

    return seed;
}

/*-*******************************************************
*  Test Mode
*********************************************************/

BYTE DECOMPRESSED_BUFFER[MAX_DECOMPRESSED_SIZE];

static size_t testDecodeSimple(frame_t* fr)
{
    /* test decoding the generated data with the simple API */
    size_t const ret = ZSTD_decompress(DECOMPRESSED_BUFFER, MAX_DECOMPRESSED_SIZE,
                           fr->dataStart, (BYTE*)fr->data - (BYTE*)fr->dataStart);

    if (ZSTD_isError(ret)) return ret;

    if (memcmp(DECOMPRESSED_BUFFER, fr->srcStart,
               (BYTE*)fr->src - (BYTE*)fr->srcStart) != 0) {
        return ERROR(corruption_detected);
    }

    return ret;
}

static size_t testDecodeStreaming(frame_t* fr)
{
    /* test decoding the generated data with the streaming API */
    ZSTD_DStream* zd = ZSTD_createDStream();
    ZSTD_inBuffer in;
    ZSTD_outBuffer out;
    size_t ret;

    if (!zd) return ERROR(memory_allocation);

    in.src = fr->dataStart;
    in.pos = 0;
    in.size = (BYTE*)fr->data - (BYTE*)fr->dataStart;

    out.dst = DECOMPRESSED_BUFFER;
    out.pos = 0;
    out.size = ZSTD_DStreamOutSize();

    ZSTD_initDStream(zd);
    while (1) {
        ret = ZSTD_decompressStream(zd, &out, &in);
        if (ZSTD_isError(ret)) goto cleanup; /* error */
        if (ret == 0) break; /* frame is done */

        /* force decoding to be done in chunks */
        out.size += MIN(ZSTD_DStreamOutSize(), MAX_DECOMPRESSED_SIZE - out.size);
    }

    ret = out.pos;

    if (memcmp(out.dst, fr->srcStart, out.pos) != 0) {
        return ERROR(corruption_detected);
    }

cleanup:
    ZSTD_freeDStream(zd);
    return ret;
}

static int runTestMode(U32 seed, unsigned numFiles, unsigned const testDurationS)
{
    unsigned fnum;

    clock_t const startClock = clock();
    clock_t const maxClockSpan = testDurationS * CLOCKS_PER_SEC;

    if (numFiles == 0 && !testDurationS) numFiles = 1;

    DISPLAY("seed: %u\n", seed);

    for (fnum = 0; fnum < numFiles || clockSpan(startClock) < maxClockSpan; fnum++) {
        frame_t fr;

        if (fnum < numFiles)
            DISPLAYUPDATE("\r%u/%u        ", fnum, numFiles);
        else
            DISPLAYUPDATE("\r%u           ", fnum);

        seed = generateFrame(seed, &fr);

        {   size_t const r = testDecodeSimple(&fr);
            if (ZSTD_isError(r)) {
                DISPLAY("Error in simple mode on test seed %u: %s\n", seed + fnum,
                        ZSTD_getErrorName(r));
                return 1;
            }
        }
        {   size_t const r = testDecodeStreaming(&fr);
            if (ZSTD_isError(r)) {
                DISPLAY("Error in streaming mode on test seed %u: %s\n", seed + fnum,
                        ZSTD_getErrorName(r));
                return 1;
            }
        }
    }

    DISPLAY("\r%u tests completed: ", fnum);
    DISPLAY("OK\n");

    return 0;
}

/*-*******************************************************
*  File I/O
*********************************************************/

static int generateFile(U32 seed, const char* const path,
                         const char* const origPath)
{
    frame_t fr;

    DISPLAY("seed: %u\n", seed);

    generateFrame(seed, &fr);

    outputBuffer(fr.dataStart, (BYTE*)fr.data - (BYTE*)fr.dataStart, path);
    if (origPath) {
        outputBuffer(fr.srcStart, (BYTE*)fr.src - (BYTE*)fr.srcStart, origPath);
    }
    return 0;
}

static int generateCorpus(U32 seed, unsigned numFiles, const char* const path,
                         const char* const origPath)
{
    char outPath[MAX_PATH];
    unsigned fnum;

    DISPLAY("seed: %u\n", seed);

    for (fnum = 0; fnum < numFiles; fnum++) {
        frame_t fr;

        DISPLAYUPDATE("\r%u/%u        ", fnum, numFiles);

        seed = generateFrame(seed, &fr);

        if (snprintf(outPath, MAX_PATH, "%s/z%06u.zst", path, fnum) + 1 > MAX_PATH) {
            DISPLAY("Error: path too long\n");
            return 1;
        }
        outputBuffer(fr.dataStart, (BYTE*)fr.data - (BYTE*)fr.dataStart, outPath);

        if (origPath) {
            if (snprintf(outPath, MAX_PATH, "%s/z%06u", origPath, fnum) + 1 > MAX_PATH) {
                DISPLAY("Error: path too long\n");
                return 1;
            }
            outputBuffer(fr.srcStart, (BYTE*)fr.src - (BYTE*)fr.srcStart, outPath);
        }
    }

    DISPLAY("\r%u/%u      \n", fnum, numFiles);

    return 0;
}


/*_*******************************************************
*  Command line
*********************************************************/
static U32 makeSeed(void)
{
    U32 t = (U32) time(NULL);
    return XXH32(&t, sizeof(t), 0) % 65536;
}

static unsigned readInt(const char** argument)
{
    unsigned val = 0;
    while ((**argument>='0') && (**argument<='9')) {
        val *= 10;
        val += **argument - '0';
        (*argument)++;
    }
    return val;
}

static void usage(const char* programName)
{
    DISPLAY( "Usage :\n");
    DISPLAY( "      %s [args]\n", programName);
    DISPLAY( "\n");
    DISPLAY( "Arguments :\n");
    DISPLAY( " -p<path> : select output path (default:stdout)\n");
    DISPLAY( "                in multiple files mode this should be a directory\n");
    DISPLAY( " -o<path> : select path to output original file (default:no output)\n");
    DISPLAY( "                in multiple files mode this should be a directory\n");
    DISPLAY( " -s#      : select seed (default:random based on time)\n");
    DISPLAY( " -n#      : number of files to generate (default:1)\n");
    DISPLAY( " -t       : activate test mode (test files against libzstd instead of outputting them)\n");
    DISPLAY( " -T#      : length of time to run tests for\n");
    DISPLAY( " -v       : increase verbosity level (default:0, max:7)\n");
    DISPLAY( " -h/H     : display help/long help and exit\n");
}

static void advancedUsage(const char* programName)
{
    usage(programName);
    DISPLAY( "\n");
    DISPLAY( "Advanced arguments :\n");
    DISPLAY( " --content-size    : always include the content size in the frame header\n");
}

int main(int argc, char** argv)
{
    U32 seed = 0;
    int seedset = 0;
    unsigned numFiles = 0;
    unsigned testDuration = 0;
    int testMode = 0;
    const char* path = NULL;
    const char* origPath = NULL;

    int argNb;

    /* Check command line */
    for (argNb=1; argNb<argc; argNb++) {
        const char* argument = argv[argNb];
        if(!argument) continue;   /* Protection if argument empty */

        /* Handle commands. Aggregated commands are allowed */
        if (argument[0]=='-') {
            argument++;
            while (*argument!=0) {
                switch(*argument)
                {
                case 'h':
                    usage(argv[0]);
                    return 0;
                case 'H':
                    advancedUsage(argv[0]);
                    return 0;
                case 'v':
                    argument++;
                    g_displayLevel++;
                    break;
                case 's':
                    argument++;
                    seedset=1;
                    seed = readInt(&argument);
                    break;
                case 'n':
                    argument++;
                    numFiles = readInt(&argument);
                    break;
                case 'T':
                    argument++;
                    testDuration = readInt(&argument);
                    if (*argument == 'm') {
                        testDuration *= 60;
                        argument++;
                        if (*argument == 'n') argument++;
                    }
                    break;
                case 'o':
                    argument++;
                    origPath = argument;
                    argument += strlen(argument);
                    break;
                case 'p':
                    argument++;
                    path = argument;
                    argument += strlen(argument);
                    break;
                case 't':
                    argument++;
                    testMode = 1;
                    break;
                case '-':
                    argument++;
                    if (strcmp(argument, "content-size") == 0) {
                        opts.contentSize = 1;
                    } else {
                        advancedUsage(argv[0]);
                        return 1;
                    }
                    argument += strlen(argument);
                    break;
                default:
                    usage(argv[0]);
                    return 1;
    }   }   }   }   /* for (argNb=1; argNb<argc; argNb++) */

    if (!seedset) {
        seed = makeSeed();
    }

    if (testMode) {
        return runTestMode(seed, numFiles, testDuration);
    } else {
        if (testDuration) {
            DISPLAY("Error: -T requires test mode (-t)\n\n");
            usage(argv[0]);
            return 1;
        }
    }

    if (!path) {
        DISPLAY("Error: path is required in file generation mode\n");
        usage(argv[0]);
        return 1;
    }

    if (numFiles == 0) {
        return generateFile(seed, path, origPath);
    } else {
        return generateCorpus(seed, numFiles, path, origPath);
    }
}

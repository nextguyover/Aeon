//
// SLIC - Simple Lossless Image Code
//
// Lossless compression starting point based on QOI by Dominic Szablewski - https://phoboslab.org
//
// Copyright 2022 BitBank Software, Inc. All Rights Reserved.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//===========================================================================

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "slic.h"
#include <stddef.h>
#include <string.h>

// // Simple callback example for Harvard architecture FLASH memory access
// int slic_flash_read(SLICFILE *pFile, uint8_t *pBuf, int32_t iLen)
// {
// int iBytesRead = 0;
// #ifdef PROGMEM
//     if (iLen + pFile->iPos > pFile->iSize)
//         iLen = pFile->iSize - pFile->iPos;
//     if (iLen < 0)
//         return 0;
//     memcpy_P(pBuf, &pFile->pData[pFile->iPos], iLen);
//     pFile->iPos += iLen;
//     iBytesRead = iLen;
// #endif // PROGMEM
//     return iBytesRead;
// } /* slic_flash_read() */

int slic_init_decode(const char *filename, SLICSTATE *pState, uint8_t *pData, int iDataSize, uint8_t *pPalette, SLIC_OPEN_CALLBACK *pfnOpen, SLIC_READ_CALLBACK *pfnRead) {
    slic_header hdr;
    int rc, i;

    if (pState == NULL) {
        return SLIC_INVALID_PARAM;
    }
    memset(pState, 0, sizeof(SLICSTATE));
    if (pfnOpen) {
        rc = (*pfnOpen)(filename, &pState->file);
        if (rc != SLIC_SUCCESS)
            return rc;
    }
    // Set up the SLICFILE structure in case data will come from a file or FLASH
    pState->pfnRead = pfnRead;
    pState->file.pData = pData;
    pState->file.iPos = 0;
    pState->file.iSize = iDataSize;

    if (pfnRead) {
        i = (*pfnRead)(&pState->file, pState->ucFileBuf, FILE_BUF_SIZE);
        memcpy(&hdr, pState->ucFileBuf, SLIC_HEADER_SIZE);
        pState->pInPtr = &pState->ucFileBuf[SLIC_HEADER_SIZE];
        pState->pInEnd = &pState->ucFileBuf[i];
    } else { // memory to memory
        memcpy(&hdr, pData, SLIC_HEADER_SIZE);
        pState->pInPtr = pData + SLIC_HEADER_SIZE;
        pState->pInEnd = &pData[iDataSize];
    }
    if (hdr.magic == SLIC_MAGIC) {
        pState->width = hdr.width;
        pState->height = hdr.height;
        pState->curr_pixel = pState->prev_pixel = 0xff000000;
        pState->bpp = hdr.bpp;
        pState->colorspace = hdr.colorspace;
        if (pState->bpp != 8 && pState->bpp != 16 && pState->bpp != 24 && pState->bpp != 32)
            return SLIC_BAD_FILE; // invalid bits per pixel
        if (pState->colorspace >= SLIC_COLORSPACE_COUNT)
            return SLIC_BAD_FILE;
        if (pState->colorspace == SLIC_PALETTE) {
            // DEBUG - fix for file based
            if (pPalette) { // copy the palette if the user wants it
                memcpy(pPalette, pState->pInPtr, 768);
            }
            pState->pInPtr += 768; // fixed size palette
        }
        pState->iPixelCount = (uint32_t)pState->width * (uint32_t)pState->height;
    } else {
        return SLIC_BAD_FILE;
    }
    return SLIC_SUCCESS;
} /* slic_init_decode() */

//
// Read more data from the data source
// if none exists --> error
// returns 0 for success, 1 for error
//
static int get_more_data(SLICSTATE *pState)
{
int i;
    if (pState->pfnRead) { // read more data
        i = (*pState->pfnRead)(&pState->file, pState->ucFileBuf, FILE_BUF_SIZE);
        pState->pInEnd = &pState->ucFileBuf[i];
        return 0;
    }
    return 1;
} /* get_more_data() */

//
// Decode N pixels into the user-supplied output buffer
//
int slic_decode(SLICSTATE *pState, uint8_t *pOut, int iOutSize) {
	uint8_t op, px8, *s, *d;
    const uint8_t *pEnd, *pSrcEnd;
    int32_t iBpp;
    uint32_t px, *index;
	int32_t run, bad_run;
    uint16_t *d16, *pEnd16, px16, *index16;

    if (pState == NULL || pOut == NULL) {
        return SLIC_INVALID_PARAM;
	}
    iBpp = pState->bpp >> 3;
    index = pState->index;
    d = pOut;
    run = pState->run;
    bad_run = pState->bad_run;
    if (iOutSize > pState->iPixelCount)
        iOutSize = pState->iPixelCount; // don't decode too much
    pState->iPixelCount -= iOutSize;
    pEnd = &d[iOutSize * (pState->bpp >> 3)];
    s = pState->pInPtr;
    pSrcEnd = pState->pInEnd;
    if (s >= pSrcEnd) {
        // Either we're at the end of the file or we need to read more data
        if (get_more_data(pState) && run == 0)
            return SLIC_DECODE_ERROR; // we're trying to go past the end, error
        s = pState->ucFileBuf;
        pSrcEnd = pState->pInEnd;
    }

    if (iBpp == 1) { // 8-bit grayscale/palette
        uint8_t *index8 = (uint8_t *)pState->index;
        px8 = (uint8_t)pState->curr_pixel;
        if (pState->extra_pixel) {
            pState->extra_pixel = 0;
            *d++ = px8;
        }
        while (d < pEnd) {
            if (run) {
                *d++ = px8; // need to deal with pixels 1 at a time
                run--;
                continue;
            }
            if (s >= pSrcEnd) {
                // Either we're at the end of the file or we need to read more data
                if (get_more_data(pState))
                    return SLIC_DECODE_ERROR; // we're trying to go past the end, error
                s = pState->ucFileBuf;
                pSrcEnd = pState->pInEnd;
            }
            if (bad_run) {
                px8 = *s++;
                *d++ = px8;
                index8[SLIC_GRAY_HASH(px8)] = px8;
                bad_run--;
                continue;
            }
            op = *s++; // get next compression op
            if ((op & SLIC_OP_MASK) == SLIC_OP_RUN8) {
                if (op == SLIC_OP_RUN8_1024) {
                    run = 1024;
                } else if (op == SLIC_OP_RUN8_256) {
                    run = 256;
                } else {
                    run = op + 1;
                }
                continue;
            } else if ((op & SLIC_OP_MASK) == SLIC_OP_BADRUN8) {
                bad_run = (op & 0x3f) + 1;
                continue;
            } else if ((op & SLIC_OP_MASK) == SLIC_OP_INDEX8) {
                *d++ = index8[op & 7];
                px8 = index8[(op >> 3) & 7];
                if (d < pEnd) { // fits in the requested output size?
                    *d++ = px8;
                } else {
                    pState->extra_pixel = 1; // get it next time through
                }
            } else { // must be DIFF8
                px8 += (op & 7)-4;
                index8[SLIC_GRAY_HASH(px8)] = px8;
                *d++ = px8;
                px8 += ((op >> 3) & 7)-4;
                index8[SLIC_GRAY_HASH(px8)] = px8;
                if (d < pEnd) {
                    *d++ = px8;
                } else {
                    pState->extra_pixel = 1; // get it next time through
                }
            }
        }
        pState->run = run;
        pState->bad_run = bad_run;
        pState->curr_pixel = px8;
        pState->pInPtr = s;
        return (pState->iPixelCount == 0) ? SLIC_DONE : SLIC_SUCCESS;
    } // 8-bit grayscale/palette decode

    if (iBpp == 2) { // RGB565
        d16 = (uint16_t *)d;
        index16 = (uint16_t *)pState->index;
        pEnd16 = (uint16_t *)pEnd;
        px16 = (uint16_t)pState->curr_pixel;
        if (pState->extra_pixel) {
            pState->extra_pixel = 0;
            *d16++ = px16;
        }
        while (d16 < pEnd16) {
            if (run) {
                *d16++ = px16;
                run--;
                continue;
            }
            if (s >= pSrcEnd) {
                // Either we're at the end of the file or we need to read more data
                if (get_more_data(pState))
                    return SLIC_DECODE_ERROR; // we're trying to go past the end, error
                s = pState->ucFileBuf;
                pSrcEnd = pState->pInEnd;
            }
            if (bad_run) {
                px16 = *s++;
                if (s >= pSrcEnd) {
                    // Either we're at the end of the file or we need to read more data
                    if (get_more_data(pState))
                        return SLIC_DECODE_ERROR; // we're trying to go past the end, error
                    s = pState->ucFileBuf;
                    pSrcEnd = pState->pInEnd;
                }
                px16 |= (*s++ << 8);
                *d16++ = px16;
                index16[SLIC_RGB565_HASH(px16)] = px16;
                bad_run--;
                continue;
            }
            op = *s++;
            if ((op & SLIC_OP_MASK) == SLIC_OP_RUN16) {
                if (op == SLIC_OP_RUN16_1024) {
                    run = 1024;
                    continue;
                } else if (op == SLIC_OP_RUN16_256) {
                    run = 256;
                    continue;
                } else {
                    run = op + 1;
                    continue;
                }
            } else if ((op & SLIC_OP_MASK) == SLIC_OP_BADRUN16) {
                bad_run = (op & 0x3f) + 1;
                continue;
            } else if ((op & SLIC_OP_MASK) == SLIC_OP_INDEX16) {
                *d16++ = index16[op & 7];
                px16 = index16[(op >> 3) & 7];
                if (d16 < pEnd16) { // fits in the requested output size?
                    *d16++ = px16;
                } else {
                    pState->extra_pixel = 1; // get it next time through
                }
            } else { // must be DIFF16
                uint8_t r, g, b;
                r = (uint8_t)(px16 >> 11);
                g = (uint8_t)((px16 >> 5) & 0x3f);
                b = (uint8_t)(px16 & 0x1f);
                r += ((op >> 4) & 3) - 2;
                g += ((op >> 2) & 3) - 2;
                b += ((op >> 0) & 3) - 2;
                px16 = (r << 11) | ((g & 0x3f) << 5) | (b & 0x1f);
                index16[SLIC_RGB565_HASH(px16)] = px16;
                *d16++ = px16;
            }
        } // for each output pixel
        pState->run = run;
        pState->bad_run = bad_run;
        pState->curr_pixel = px16;
        pState->pInPtr = s;
        return (pState->iPixelCount == 0) ? SLIC_DONE : SLIC_SUCCESS;
    } // RGB565 decode

    px = pState->curr_pixel;
    while (d < pEnd) {
        int iHash;
        if (run) {
#ifdef UNALIGNED_ALLOWED
            *(uint32_t *)d = px;
#else
            d[0] = (uint8_t)px;
            d[1] = (uint8_t)(px >> 8);
            d[2] = (uint8_t)(px >> 16);
            d[3] = (uint8_t)(px >> 24);
#endif
            run--;
            d += iBpp;
            continue;
        }
        if (s >= pSrcEnd) {
            // Either we're at the end of the file or we need to read more data
            if (get_more_data(pState))
                return SLIC_DECODE_ERROR; // we're trying to go past the end, error
            s = pState->ucFileBuf;
            pSrcEnd = pState->pInEnd;
        }
			op = *s++;
            if (op >= SLIC_OP_RUN && op < SLIC_OP_RGB) {
                if (op == SLIC_OP_RUN1024) {
                    run = 1024;
                    continue;
                } else if (op == SLIC_OP_RUN256) {
                    run = 256;
                    continue;
                } else {
                    run = (op & 0x3f) + 1;
                    continue;
                }
            }
            else if ((op & SLIC_OP_MASK) == SLIC_OP_INDEX) {
                px = index[op];
            }
			else if (op == SLIC_OP_RGB) {
                px &= 0xff000000;
#ifdef UNALIGNED_ALLOWED
                px |= (*(uint32_t *)s) & 0xffffff;
#else
                px |= s[0];
                px |= ((uint32_t)s[1] << 8);
                px |= ((uint32_t)s[2] << 16);
#endif
                s += 3;
			}
			else if (op == SLIC_OP_RGBA) {
#ifdef UNALIGNED_ALLOWED
                px = *(uint32_t *)s;
                s += 4;
#else
                px = *s++;
                px |= ((uint32_t)*s++ << 8);
                px |= ((uint32_t)*s++ << 16);
                px |= ((uint32_t)*s++ << 24);
#endif
			}
			else if ((op & SLIC_OP_MASK) == SLIC_OP_DIFF) {
                uint8_t r, g, b;
                r = px;
                g = (px >> 8);
                b = (px >> 16);
                r += ((op >> 4) & 0x03) - 2;
                g += ((op >> 2) & 0x03) - 2;
                b += ( op       & 0x03) - 2;
                px &= 0xff000000;
                px |= (r & 0xff);
                px |= ((uint32_t)(g & 0xff) << 8);
                px |= ((uint32_t)(b & 0xff) << 16);
			}
			else if ((op & SLIC_OP_MASK) == SLIC_OP_LUMA) {
				int b2 = *s++;
				int vg = (op & 0x3f) - 32;
                uint8_t r, g, b;
                r = px;
                g = (px >> 8);
                b = (px >> 16);
                r += vg - 8 + ((b2 >> 4) & 0x0f);
                g += vg;
                b += vg - 8 +  (b2       & 0x0f);
                px &= 0xff000000;
                px |= (r & 0xff);
                px |= ((uint32_t)(g & 0xff) << 8);
                px |= ((uint32_t)(b & 0xff) << 16);
			}
        iHash = px * 3;
        iHash += ((px >> 8) * 5);
        iHash += ((px >> 16) * 7);
        iHash += ((px >> 24) * 11);
        index[iHash & 63] = px;

#ifdef UNALIGNED_ALLOWED
        *(uint32_t *)d = px;
#else
        d[0] = (uint8_t)px;
        d[1] = (uint8_t)(px >> 8);
        d[2] = (uint8_t)(px >> 16);
        d[3] = (uint8_t)(px >> 24);
#endif
        d += iBpp;
	} // while decoding each pixel (3/4 bpp)

    pState->run = run;
    pState->bad_run = bad_run;
    pState->curr_pixel = px;
    pState->pInPtr = s;
    return (pState->iPixelCount == 0) ? SLIC_DONE : SLIC_SUCCESS;
} /* slic_decode() */

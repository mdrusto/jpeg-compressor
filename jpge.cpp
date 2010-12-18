// jpge.cpp - C++ class for JPEG compression.
// Public domain, Rich Geldreich <richgel99@gmail.com>
// v1.00 - Last updated Dec. 18, 2010
// Note: The current DCT function was derived from an old version of libjpeg.

#include "jpge.h"

#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#define JPGE_MAX(a,b) (((a)>(b))?(a):(b))
#define JPGE_MIN(a,b) (((a)<(b))?(a):(b))
#define JPGE_OUT_BUF_SIZE (2048)

namespace jpge {

// Various JPEG enums and tables.
enum { M_SOF0 = 0xC0, M_DHT = 0xC4, M_SOI = 0xD8, M_EOI = 0xD9, M_SOS = 0xDA, M_DQT = 0xDB, M_APP0 = 0xE0 };

static uint8 s_zag[64] = { 0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
                         35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63, };

static int16 s_std_lum_quant[64] = { 16,  11,  12,  14,  12,  10,  16,  14, 13,  14,  18,  17,  16,  19,  24,  40, 26,  24,  22,  22,  24,  49,  35,  37, 29,  40,  58,  51,  61,  60,  57,  51,
                                     56,  55,  64,  72,  92,  78,  64,  68, 87,  69,  55,  56,  80, 109,  81,  87, 95,  98, 103, 104, 103,  62,  77, 113, 121, 112, 100, 120,  92, 101, 103,  99 };

static int16 s_std_croma_quant[64] = { 17,  18,  18,  24,  21,  24,  47,  26, 26,  47,  99,  66,  56,  66,  99,  99, 99,  99,  99,  99,  99,  99,  99,  99, 99,  99,  99,  99,  99,  99,  99,  99,
                                       99,  99,  99,  99,  99,  99,  99,  99, 99,  99,  99,  99,  99,  99,  99,  99, 99,  99,  99,  99,  99,  99,  99,  99, 99,  99,  99,  99,  99,  99,  99,  99 };
                                       
enum { DC_LUM_CODES = 12, AC_LUM_CODES = 256, DC_CHROMA_CODES = 12, AC_CHROMA_CODES = 256 };                                       

static uint8 s_dc_lum_bits[17] = { 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
static uint8 s_dc_lum_val[DC_LUM_CODES] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static uint8 s_ac_lum_bits[17] = { 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };

static uint8 s_ac_lum_val[AC_LUM_CODES]  =
{
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

static uint8 s_dc_chroma_bits[17] = { 0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0 };
static uint8 s_dc_chroma_val[DC_CHROMA_CODES]  = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
static uint8 s_ac_chroma_bits[17] = { 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };

static uint8 s_ac_chroma_val[AC_CHROMA_CODES] = 
{ 
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

// Low-level helper functions.
static inline void *jpge_malloc(size_t nSize) { return malloc(nSize); }
static inline void *jpge_cmalloc(size_t nSize) { return calloc(nSize, 1); }
static inline void jpge_free(void *p) { return free(p); }

template <class T> inline void clear_obj(T &obj) { memset(&obj, 0, sizeof(obj)); }

const int YR = 19595, YG = 38470, YB = 7471, CB_R = -11059, CB_G = -21709, CB_B = 32768, CR_R = 32768, CR_G = -27439, CR_B = -5329;
static inline uint8 clamp(int i) { if (static_cast<uint>(i) > 255U) { if (i < 0) i = 0; else if (i > 255) i = 255; } return static_cast<uint8>(i); }

static void RGB_to_YCC(uint8* Pdst, const uint8 *src, int num_pixels)
{
  for ( ; num_pixels; Pdst += 3, src += 3, num_pixels--)
  {
    const int r = src[0], g = src[1], b = src[2];
    Pdst[0] = static_cast<uint8>((r * YR + g * YG + b * YB + 32768) >> 16);
    Pdst[1] = clamp(128 + ((r * CB_R + g * CB_G + b * CB_B + 32768) >> 16));
    Pdst[2] = clamp(128 + ((r * CR_R + g * CR_G + b * CR_B + 32768) >> 16));
  }
}

static void RGB_to_Y(uint8* Pdst, const uint8 *src, int num_pixels)
{
  for ( ; num_pixels; Pdst++, src += 3, num_pixels--)
    Pdst[0] = static_cast<uint8>((src[0] * YR + src[1] * YG + src[2] * YB + 32768) >> 16);
}

static void Y_to_YCC(uint8* Pdst, const uint8* src, int num_pixels)
{
  for( ; num_pixels; Pdst += 3, src++, num_pixels--) { Pdst[0] = src[0]; Pdst[1] = 128; Pdst[2] = 128; }
}

// Forward DCT
#define CONST_BITS 13
#define PASS1_BITS 2
#define FIX_0_298631336 ((int32)2446)    /* FIX(0.298631336) */
#define FIX_0_390180644 ((int32)3196)    /* FIX(0.390180644) */
#define FIX_0_541196100 ((int32)4433)    /* FIX(0.541196100) */
#define FIX_0_765366865 ((int32)6270)    /* FIX(0.765366865) */
#define FIX_0_899976223 ((int32)7373)    /* FIX(0.899976223) */
#define FIX_1_175875602 ((int32)9633)    /* FIX(1.175875602) */
#define FIX_1_501321110 ((int32)12299)   /* FIX(1.501321110) */
#define FIX_1_847759065 ((int32)15137)   /* FIX(1.847759065) */
#define FIX_1_961570560 ((int32)16069)   /* FIX(1.961570560) */
#define FIX_2_053119869 ((int32)16819)   /* FIX(2.053119869) */
#define FIX_2_562915447 ((int32)20995)   /* FIX(2.562915447) */
#define FIX_3_072711026 ((int32)25172)   /* FIX(3.072711026) */
#define DCT_DESCALE(x, n) (((x) + (((int32)1) << ((n) - 1))) >> (n))
#define DCT_MUL(var, const)  (((int16) (var)) * ((int32) (const)))

static void dct(int32 *data)
{
  int c;
  int32 z1, z2, z3, z4, z5, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp10, tmp11, tmp12, tmp13, *data_ptr;

  data_ptr = data;

  for (c = 7; c >= 0; c--)
  {
    tmp0 = data_ptr[0] + data_ptr[7];
    tmp7 = data_ptr[0] - data_ptr[7];
    tmp1 = data_ptr[1] + data_ptr[6];
    tmp6 = data_ptr[1] - data_ptr[6];
    tmp2 = data_ptr[2] + data_ptr[5];
    tmp5 = data_ptr[2] - data_ptr[5];
    tmp3 = data_ptr[3] + data_ptr[4];
    tmp4 = data_ptr[3] - data_ptr[4];
    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;
    data_ptr[0] = (int32)((tmp10 + tmp11) << PASS1_BITS);
    data_ptr[4] = (int32)((tmp10 - tmp11) << PASS1_BITS);
    z1 = DCT_MUL(tmp12 + tmp13, FIX_0_541196100);
    data_ptr[2] = (int32)DCT_DESCALE(z1 + DCT_MUL(tmp13, FIX_0_765366865), CONST_BITS-PASS1_BITS);
    data_ptr[6] = (int32)DCT_DESCALE(z1 + DCT_MUL(tmp12, - FIX_1_847759065), CONST_BITS-PASS1_BITS);
    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;
    z5 = DCT_MUL(z3 + z4, FIX_1_175875602);
    tmp4 = DCT_MUL(tmp4, FIX_0_298631336);
    tmp5 = DCT_MUL(tmp5, FIX_2_053119869);
    tmp6 = DCT_MUL(tmp6, FIX_3_072711026);
    tmp7 = DCT_MUL(tmp7, FIX_1_501321110);
    z1   = DCT_MUL(z1, -FIX_0_899976223);
    z2   = DCT_MUL(z2, -FIX_2_562915447);
    z3   = DCT_MUL(z3, -FIX_1_961570560);
    z4   = DCT_MUL(z4, -FIX_0_390180644);
    z3 += z5;
    z4 += z5;
    data_ptr[7] = (int32)DCT_DESCALE(tmp4 + z1 + z3, CONST_BITS-PASS1_BITS);
    data_ptr[5] = (int32)DCT_DESCALE(tmp5 + z2 + z4, CONST_BITS-PASS1_BITS);
    data_ptr[3] = (int32)DCT_DESCALE(tmp6 + z2 + z3, CONST_BITS-PASS1_BITS);
    data_ptr[1] = (int32)DCT_DESCALE(tmp7 + z1 + z4, CONST_BITS-PASS1_BITS);
    data_ptr += 8;
  }

  data_ptr = data;

  for (c = 7; c >= 0; c--)
  {
    tmp0 = data_ptr[8*0] + data_ptr[8*7];
    tmp7 = data_ptr[8*0] - data_ptr[8*7];
    tmp1 = data_ptr[8*1] + data_ptr[8*6];
    tmp6 = data_ptr[8*1] - data_ptr[8*6];
    tmp2 = data_ptr[8*2] + data_ptr[8*5];
    tmp5 = data_ptr[8*2] - data_ptr[8*5];
    tmp3 = data_ptr[8*3] + data_ptr[8*4];
    tmp4 = data_ptr[8*3] - data_ptr[8*4];
    tmp10 = tmp0 + tmp3;
    tmp13 = tmp0 - tmp3;
    tmp11 = tmp1 + tmp2;
    tmp12 = tmp1 - tmp2;
    data_ptr[8*0] = (int32)DCT_DESCALE(tmp10 + tmp11, PASS1_BITS+3);
    data_ptr[8*4] = (int32)DCT_DESCALE(tmp10 - tmp11, PASS1_BITS+3);
    z1 = DCT_MUL(tmp12 + tmp13, FIX_0_541196100);
    data_ptr[8*2] = (int32)DCT_DESCALE(z1 + DCT_MUL(tmp13, FIX_0_765366865), CONST_BITS+PASS1_BITS+3);
    data_ptr[8*6] = (int32)DCT_DESCALE(z1 + DCT_MUL(tmp12, - FIX_1_847759065), CONST_BITS+PASS1_BITS+3);
    z1 = tmp4 + tmp7;
    z2 = tmp5 + tmp6;
    z3 = tmp4 + tmp6;
    z4 = tmp5 + tmp7;
    z5 = DCT_MUL(z3 + z4, FIX_1_175875602);
    tmp4 = DCT_MUL(tmp4, FIX_0_298631336);
    tmp5 = DCT_MUL(tmp5, FIX_2_053119869);
    tmp6 = DCT_MUL(tmp6, FIX_3_072711026);
    tmp7 = DCT_MUL(tmp7, FIX_1_501321110);
    z1   = DCT_MUL(z1, -FIX_0_899976223);
    z2   = DCT_MUL(z2, -FIX_2_562915447);
    z3   = DCT_MUL(z3, -FIX_1_961570560);
    z4   = DCT_MUL(z4, -FIX_0_390180644);
    z3 += z5;
    z4 += z5;
    data_ptr[8*7] = (int32)DCT_DESCALE(tmp4 + z1 + z3, CONST_BITS + PASS1_BITS + 3);
    data_ptr[8*5] = (int32)DCT_DESCALE(tmp5 + z2 + z4, CONST_BITS + PASS1_BITS + 3);
    data_ptr[8*3] = (int32)DCT_DESCALE(tmp6 + z2 + z3, CONST_BITS + PASS1_BITS + 3);
    data_ptr[8*1] = (int32)DCT_DESCALE(tmp7 + z1 + z4, CONST_BITS + PASS1_BITS + 3);
    data_ptr++;
  }
}

// JPEG marker generation.
void jpeg_encoder::emit_marker(int marker)
{
  m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_obj(uint8(0xFF));
  m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_obj(uint8(marker));
}

void jpeg_encoder::emit_byte(uint8 i)
{
  m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_obj(i);
}

void jpeg_encoder::emit_word(uint i)
{
  m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_obj(uint8(i >> 8));
  m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_obj(uint8(i & 0xFF));
}

void jpeg_encoder::emit_jfif_app0()
{
  emit_marker(M_APP0);
  emit_word(2 + 4 + 1 + 2 + 1 + 2 + 2 + 1 + 1);
  emit_byte(0x4A); emit_byte(0x46); emit_byte(0x49); emit_byte(0x46); /* Identifier: ASCII "JFIF" */
  emit_byte(0);
  emit_byte(1);      /* Major version */
  emit_byte(1);      /* Minor version */
  emit_byte(0);      /* Density unit */
  emit_word(1);
  emit_word(1);
  emit_byte(0);      /* No thumbnail image */
  emit_byte(0);
}

void jpeg_encoder::emit_dqt()
{
  for (int i = 0; i < ((m_num_components == 3) ? 2 : 1); i++)
  {
    emit_marker(M_DQT);
    emit_word(64 + 1 + 2);
    emit_byte(i);
    for (int j = 0; j < 64; j++)
      emit_byte(m_quantization_tables[i][j]);
  }
}

void jpeg_encoder::emit_sof()
{
  emit_marker(M_SOF0);                           /* baseline */
  emit_word(3 * m_num_components + 2 + 5 + 1);
  emit_byte(8);                                  /* precision */
  emit_word(m_image_y);
  emit_word(m_image_x);
  emit_byte(m_num_components);

  for (int i = 0; i < m_num_components; i++)
  {
    emit_byte(i + 1);                                   /* component ID     */
    emit_byte((m_comp_h_samp[i] << 4) + m_comp_v_samp[i]);  /* h and v sampling */
    emit_byte(i > 0);                                   /* quant. table num */
  }
}

void jpeg_encoder::emit_dht(uint8 *bits, uint8 *val, int index, bool ac_flag)
{
  int i, length;

  emit_marker(M_DHT);

  length = 0;

  for (i = 1; i <= 16; i++)
    length += bits[i];

  emit_word(length + 2 + 1 + 16);
  emit_byte(index + (ac_flag << 4));

  for (i = 1; i <= 16; i++)
    emit_byte(bits[i]);

  for (i = 0; i < length; i++)
    emit_byte(val[i]);
}

void jpeg_encoder::emit_dhts()
{
  emit_dht(m_huff_bits[0+0], m_huff_val[0+0], 0, false);
  emit_dht(m_huff_bits[2+0], m_huff_val[2+0], 0, true);
  if (m_num_components == 3)
  {
    emit_dht(m_huff_bits[0+1], m_huff_val[0+1], 1, false);
    emit_dht(m_huff_bits[2+1], m_huff_val[2+1], 1, true);
  }
}

void jpeg_encoder::emit_sos()
{
  emit_marker(M_SOS);
  emit_word(2 * m_num_components + 2 + 1 + 3);
  emit_byte(m_num_components);

  for (int i = 0; i < m_num_components; i++)
  {
    emit_byte(i + 1);

    if (i == 0)
      emit_byte((0 << 4) + 0);
    else
      emit_byte((1 << 4) + 1);
  }

  emit_byte(0);     /* spectral selection */
  emit_byte(63);
  emit_byte(0);
}

void jpeg_encoder::emit_markers()
{
  emit_marker(M_SOI);
  emit_jfif_app0();
  emit_dqt();
  emit_sof();
  emit_dhts();
  emit_sos();
}

bool jpeg_encoder::compute_huffman_table(uint * *codes, uint8 * *code_sizes, uint8 *bits, uint8 *val)
{
  int p, i, l, last_p, si;
  uint8 huff_size[257];
  uint huff_code[257];
  uint code;

  p = 0;

  for (l = 1; l <= 16; l++)
    for (i = 1; i <= bits[l]; i++)
      huff_size[p++] = (char)l;

  huff_size[p] = 0; last_p = p;

  code = 0; si = huff_size[0]; p = 0;

  while (huff_size[p])
  {
    while (huff_size[p] == si)
      huff_code[p++] = code++;

    code <<= 1;
    si++;
  }

  for (p = 0, i = 0; p < last_p; p++)
    i = JPGE_MAX(i, val[p]);

  *codes = static_cast<uint*>(jpge_cmalloc((i + 1) * sizeof(uint)));
  *code_sizes = static_cast<uint8*>(jpge_cmalloc((i + 1) * sizeof(uint8)));

  if (!(*codes) || !(*code_sizes))
  {
    jpge_free(*codes); *codes = NULL;
    jpge_free(*code_sizes); *code_sizes = NULL;
    return false;
  }

  for (p = 0; p < last_p; p++)
  {
    (*codes)[val[p]]      = huff_code[p];
    (*code_sizes)[val[p]] = huff_size[p];
  }

  return true;
}

// Quantization table generation.
void jpeg_encoder::compute_quant_table(int32 *dst, int16 *src)
{
  int32 q;

  if (m_params.m_quality < 50)
    q = 5000 / m_params.m_quality;
  else
    q = 200 - m_params.m_quality * 2;

  for (int i = 0; i < 64; i++)
  {
    int32 j = *src++;

    j = (j * q + 50L) / 100L;

    if (j < 1)
      j = 1;
    else if (j > 255)
      j = 255;

    *dst++ = j;
  }
}

// Higher-level methods.
void jpeg_encoder::first_pass_init()
{
  m_bit_buffer = 0; m_bits_in = 0;

  memset(m_last_dc_val, 0, 3 * sizeof(int));

  m_mcu_y_ofs = 0;
}

bool jpeg_encoder::second_pass_init()
{
  if (!compute_huffman_table(&m_huff_codes[0+0], &m_huff_code_sizes[0+0], m_huff_bits[0+0], m_huff_val[0+0])) return false;
  if (!compute_huffman_table(&m_huff_codes[2+0], &m_huff_code_sizes[2+0], m_huff_bits[2+0], m_huff_val[2+0])) return false;
  if (!compute_huffman_table(&m_huff_codes[0+1], &m_huff_code_sizes[0+1], m_huff_bits[0+1], m_huff_val[0+1])) return false;
  if (!compute_huffman_table(&m_huff_codes[2+1], &m_huff_code_sizes[2+1], m_huff_bits[2+1], m_huff_val[2+1])) return false;
  first_pass_init();
  emit_markers();
  return true;
}

bool jpeg_encoder::jpg_open(int p_x_res, int p_y_res, int src_channels)
{
  switch (m_params.m_subsampling)
  {
    case 0:
    {
      m_num_components = 1;
      m_comp_h_samp[0] = 1; m_comp_v_samp[0] = 1;
      m_mcu_x          = 8; m_mcu_y          = 8;
      break;
    }
    case 1:
    {
      m_num_components = 3;
      m_comp_h_samp[0] = 1; m_comp_v_samp[0] = 1;
      m_comp_h_samp[1] = 1; m_comp_v_samp[1] = 1;
      m_comp_h_samp[2] = 1; m_comp_v_samp[2] = 1;
      m_mcu_x          = 8; m_mcu_y          = 8;
      break;
    }
    case 2:
    {
      m_num_components = 3;
      m_comp_h_samp[0] = 2; m_comp_v_samp[0] = 1;
      m_comp_h_samp[1] = 1; m_comp_v_samp[1] = 1;
      m_comp_h_samp[2] = 1; m_comp_v_samp[2] = 1;
      m_mcu_x          = 16; m_mcu_y         = 8;
      break;
    }
    case 3:
    {
      m_num_components = 3;
      m_comp_h_samp[0] = 2; m_comp_v_samp[0] = 2;
      m_comp_h_samp[1] = 1; m_comp_v_samp[1] = 1;
      m_comp_h_samp[2] = 1; m_comp_v_samp[2] = 1;
      m_mcu_x          = 16; m_mcu_y         = 16;
    }
  }

  m_image_x        = p_x_res;
  m_image_y        = p_y_res;
  m_image_bpp      = src_channels;
  m_image_bpl      = m_image_x * src_channels;

  m_image_x_mcu    = (m_image_x + m_mcu_x - 1) & (~(m_mcu_x - 1));
  m_image_y_mcu    = (m_image_y + m_mcu_y - 1) & (~(m_mcu_y - 1));
  m_image_bpl_xlt  = m_image_x * m_num_components;
  m_image_bpl_mcu  = m_image_x_mcu * m_num_components;
  m_mcus_per_row   = m_image_x_mcu / m_mcu_x;

  #define JPGE_ALLOC_CHECK(exp) do { if ((exp) == NULL) return false; } while(0)

  JPGE_ALLOC_CHECK(m_mcu_lines = static_cast<uint8**>(jpge_cmalloc(m_mcu_y * sizeof(uint8 *))));

  for (int i = 0; i < m_mcu_y; i++)
    JPGE_ALLOC_CHECK(m_mcu_lines[i] = static_cast<uint8*>(jpge_malloc(m_image_bpl_mcu)));

  JPGE_ALLOC_CHECK(m_sample_array = static_cast<sample_array_t*>(jpge_malloc(64 * sizeof(sample_array_t))));
  JPGE_ALLOC_CHECK(m_coefficient_array = static_cast<int16*>(jpge_malloc(64 * sizeof(int16))));

  JPGE_ALLOC_CHECK(m_quantization_tables[0]        = static_cast<int32*>(jpge_malloc(64 * sizeof(int32))));
  JPGE_ALLOC_CHECK(m_quantization_tables[1]        = static_cast<int32*>(jpge_malloc(64 * sizeof(int32))));

  compute_quant_table(m_quantization_tables[0], s_std_lum_quant);
  compute_quant_table(m_quantization_tables[1], m_params.m_no_chroma_discrim_flag ? s_std_lum_quant : s_std_croma_quant);

  JPGE_ALLOC_CHECK(m_out_buf_ofs = m_out_buf = static_cast<uint8*>(jpge_malloc(m_out_buf_left = JPGE_OUT_BUF_SIZE)));

  JPGE_ALLOC_CHECK(m_huff_bits[0+0] = static_cast<uint8*>(jpge_cmalloc(17)));
  JPGE_ALLOC_CHECK(m_huff_val [0+0] = static_cast<uint8*>(jpge_cmalloc(DC_LUM_CODES)));
  JPGE_ALLOC_CHECK(m_huff_bits[2+0] = static_cast<uint8*>(jpge_cmalloc(17)));
  JPGE_ALLOC_CHECK(m_huff_val [2+0] = static_cast<uint8*>(jpge_cmalloc(AC_LUM_CODES)));
  JPGE_ALLOC_CHECK(m_huff_bits[0+1] = static_cast<uint8*>(jpge_cmalloc(17)));
  JPGE_ALLOC_CHECK(m_huff_val [0+1] = static_cast<uint8*>(jpge_cmalloc(DC_CHROMA_CODES)));
  JPGE_ALLOC_CHECK(m_huff_bits[2+1] = static_cast<uint8*>(jpge_cmalloc(17)));
  JPGE_ALLOC_CHECK(m_huff_val [2+1] = static_cast<uint8*>(jpge_cmalloc(AC_CHROMA_CODES)));

  memcpy(m_huff_bits[0+0], s_dc_lum_bits, 17);
  memcpy(m_huff_val [0+0], s_dc_lum_val, DC_LUM_CODES);
  memcpy(m_huff_bits[2+0], s_ac_lum_bits, 17);
  memcpy(m_huff_val [2+0], s_ac_lum_val, AC_LUM_CODES);
  memcpy(m_huff_bits[0+1], s_dc_chroma_bits, 17);
  memcpy(m_huff_val [0+1], s_dc_chroma_val, DC_CHROMA_CODES);
  memcpy(m_huff_bits[2+1], s_ac_chroma_bits, 17);
  memcpy(m_huff_val [2+1], s_ac_chroma_val, AC_CHROMA_CODES);

  if (!second_pass_init()) return false;

  return m_all_stream_writes_succeeded;
}

void jpeg_encoder::load_block_8_8_grey(int x)
{
  uint8 *src;
  sample_array_t *dst = m_sample_array;

  x <<= 3;

  for (int i = 0; i < 8; i++, dst += 8)
  {
    src = m_mcu_lines[i] + x;

    dst[0] = src[0] - 128; dst[1] = src[1] - 128; dst[2] = src[2] - 128; dst[3] = src[3] - 128;
    dst[4] = src[4] - 128; dst[5] = src[5] - 128; dst[6] = src[6] - 128; dst[7] = src[7] - 128;
  }
}

void jpeg_encoder::load_block_8_8(int x, int y, int c)
{
  uint8 *src;
  sample_array_t *dst = m_sample_array;

  x = (x * (8 * 3)) + c;
  y <<= 3;

  for (int i = 0; i < 8; i++, dst += 8)
  {
    src = m_mcu_lines[y + i] + x;

    dst[0] = src[0 * 3] - 128; dst[1] = src[1 * 3] - 128; dst[2] = src[2 * 3] - 128; dst[3] = src[3 * 3] - 128;
    dst[4] = src[4 * 3] - 128; dst[5] = src[5 * 3] - 128; dst[6] = src[6 * 3] - 128; dst[7] = src[7 * 3] - 128;
  }
}

void jpeg_encoder::load_block_16_8(int x, int c)
{
  uint8 *src1, *src2;
  sample_array_t *dst = m_sample_array;

  x = (x * (16 * 3)) + c;

  int a = 0, b = 2;
  for (int i = 0; i < 16; i += 2, dst += 8)
  {
    src1 = m_mcu_lines[i + 0] + x;
    src2 = m_mcu_lines[i + 1] + x;

    dst[0] = ((src1[ 0 * 3] + src1[ 1 * 3] + src2[ 0 * 3] + src2[ 1 * 3] + a) >> 2) - 128;
    dst[1] = ((src1[ 2 * 3] + src1[ 3 * 3] + src2[ 2 * 3] + src2[ 3 * 3] + b) >> 2) - 128;
    dst[2] = ((src1[ 4 * 3] + src1[ 5 * 3] + src2[ 4 * 3] + src2[ 5 * 3] + a) >> 2) - 128;
    dst[3] = ((src1[ 6 * 3] + src1[ 7 * 3] + src2[ 6 * 3] + src2[ 7 * 3] + b) >> 2) - 128;
    dst[4] = ((src1[ 8 * 3] + src1[ 9 * 3] + src2[ 8 * 3] + src2[ 9 * 3] + a) >> 2) - 128;
    dst[5] = ((src1[10 * 3] + src1[11 * 3] + src2[10 * 3] + src2[11 * 3] + b) >> 2) - 128;
    dst[6] = ((src1[12 * 3] + src1[13 * 3] + src2[12 * 3] + src2[13 * 3] + a) >> 2) - 128;
    dst[7] = ((src1[14 * 3] + src1[15 * 3] + src2[14 * 3] + src2[15 * 3] + b) >> 2) - 128;

    int temp = a; a = b; b = temp;
  }
}

void jpeg_encoder::load_block_16_8_8(int x, int c)
{
  uint8 *src1;
  sample_array_t *dst = m_sample_array;

  x = (x * (16 * 3)) + c;

  int a = 0, b = 2;
  for (int i = 0; i < 8; i++, dst += 8)
  {
    src1 = m_mcu_lines[i + 0] + x;

    dst[0] = ((src1[ 0 * 3] + src1[ 1 * 3] + a) >> 1) - 128;
    dst[1] = ((src1[ 2 * 3] + src1[ 3 * 3] + b) >> 1) - 128;
    dst[2] = ((src1[ 4 * 3] + src1[ 5 * 3] + a) >> 1) - 128;
    dst[3] = ((src1[ 6 * 3] + src1[ 7 * 3] + b) >> 1) - 128;
    dst[4] = ((src1[ 8 * 3] + src1[ 9 * 3] + a) >> 1) - 128;
    dst[5] = ((src1[10 * 3] + src1[11 * 3] + b) >> 1) - 128;
    dst[6] = ((src1[12 * 3] + src1[13 * 3] + a) >> 1) - 128;
    dst[7] = ((src1[14 * 3] + src1[15 * 3] + b) >> 1) - 128;

    int temp = a; a = b; b = temp;
  }
}

void jpeg_encoder::load_coefficients(int component_num)
{
  int32 *q = m_quantization_tables[component_num > 0];
  int16 *dst = m_coefficient_array;
  for (int i = 0; i < 64; i++)
  {
    sample_array_t j = m_sample_array[s_zag[i]];

    if (j < 0)
    {
      if ((j = -j + (*q >> 1)) < *q)
        *dst++ = 0;
      else
        *dst++ = -(j / *q);
    }
    else
    {
      if ((j = j + (*q >> 1)) < *q)
        *dst++ = 0;
      else
        *dst++ = (j / *q);
    }

    q++;
  }
}

void jpeg_encoder::flush_output_buffer()
{
  if (m_out_buf_left != JPGE_OUT_BUF_SIZE)
  {
    m_all_stream_writes_succeeded = m_all_stream_writes_succeeded && m_pStream->put_buf(m_out_buf, JPGE_OUT_BUF_SIZE - m_out_buf_left);
  }

  m_out_buf_ofs  = m_out_buf;
  m_out_buf_left = JPGE_OUT_BUF_SIZE;
}

void jpeg_encoder::put_bits(uint bits, uint8 len)
{
  bits &= ((1UL << len) - 1);

  m_bit_buffer |= ((uint32)bits << (24 - (m_bits_in += len)));

  while (m_bits_in >= 8)
  {
    uint8 c;

    #define JPGE_PUT_BYTE(c) { *m_out_buf_ofs++ = (c); if (--m_out_buf_left == 0) flush_output_buffer(); }
    JPGE_PUT_BYTE(c = (uint8)((m_bit_buffer >> 16) & 0xFF));
    if (c == 0xFF) JPGE_PUT_BYTE(0);

    m_bit_buffer <<= 8;

    m_bits_in -= 8;
  }
}

void jpeg_encoder::code_coefficients_pass_two(int component_num)
{
  int i, j, run_len, nbits, temp1, temp2;
  int16 *src = m_coefficient_array;
  uint *codes[2];
  uint8 *code_sizes[2];

  if (component_num == 0)
  {
    codes[0] = m_huff_codes[0 + 0];
    codes[1] = m_huff_codes[2 + 0];
    code_sizes[0] = m_huff_code_sizes[0 + 0];
    code_sizes[1] = m_huff_code_sizes[2 + 0];
  }
  else
  {
    codes[0] = m_huff_codes[0 + 1];
    codes[1] = m_huff_codes[2 + 1];
    code_sizes[0] = m_huff_code_sizes[0 + 1];
    code_sizes[1] = m_huff_code_sizes[2 + 1];
  }

  temp1 = temp2 = src[0] - m_last_dc_val[component_num];
  m_last_dc_val[component_num] = src[0];

  if (temp1 < 0)
  {
    temp1 = -temp1;
    temp2--;
  }

  nbits = 0;
  while (temp1)
  {
    nbits++;
    temp1 >>= 1;
  }

  put_bits(codes[0][nbits], code_sizes[0][nbits]);
  if (nbits)
    put_bits(temp2, nbits);

  for (run_len = 0, i = 1; i < 64; i++)
  {
    if ((temp1 = m_coefficient_array[i]) == 0)
      run_len++;
    else
    {
      while (run_len >= 16)
      {
        put_bits(codes[1][0xF0], code_sizes[1][0xF0]);
        run_len -= 16;
      }

      if ((temp2 = temp1) < 0)
      {
        temp1 = -temp1;
        temp2--;
      }

      nbits = 1;
      while (temp1 >>= 1)
        nbits++;

      j = (run_len << 4) + nbits;

      put_bits(codes[1][j], code_sizes[1][j]);
      put_bits(temp2, nbits);

      run_len = 0;
    }
  }

  if (run_len)
    put_bits(codes[1][0], code_sizes[1][0]);
}

void jpeg_encoder::code_block(int component_num)
{
  dct(m_sample_array);
  load_coefficients(component_num);
  code_coefficients_pass_two(component_num);
}

void jpeg_encoder::process_mcu_row()
{
  if (m_num_components == 1)
  {
    for (int i = 0; i < m_mcus_per_row; i++)
    {
      load_block_8_8_grey(i); code_block(0);
    }
  }
  else if ((m_comp_h_samp[0] == 1) && (m_comp_v_samp[0] == 1))
  {
    for (int i = 0; i < m_mcus_per_row; i++)
    {
      load_block_8_8(i, 0, 0); code_block(0);
      load_block_8_8(i, 0, 1); code_block(1);
      load_block_8_8(i, 0, 2); code_block(2);
    }
  }
  else if ((m_comp_h_samp[0] == 2) && (m_comp_v_samp[0] == 1))
  {
    for (int i = 0; i < m_mcus_per_row; i++)
    {
      load_block_8_8(i * 2 + 0, 0, 0); code_block(0);
      load_block_8_8(i * 2 + 1, 0, 0); code_block(0);
      load_block_16_8_8(i, 1); code_block(1);
      load_block_16_8_8(i, 2); code_block(2);
    }
  }
  else if ((m_comp_h_samp[0] == 2) && (m_comp_v_samp[0] == 2))
  {
    for (int i = 0; i < m_mcus_per_row; i++)
    {
      load_block_8_8(i * 2 + 0, 0, 0); code_block(0);
      load_block_8_8(i * 2 + 1, 0, 0); code_block(0);
      load_block_8_8(i * 2 + 0, 1, 0); code_block(0);
      load_block_8_8(i * 2 + 1, 1, 0); code_block(0);
      load_block_16_8(i, 1); code_block(1);
      load_block_16_8(i, 2); code_block(2);
    }
  }
}

bool jpeg_encoder::terminate_pass_two()
{
  put_bits(0x7F, 7);
  flush_output_buffer();
  emit_marker(M_EOI);
  return true;
}

bool jpeg_encoder::process_end_of_image()
{
  if (m_mcu_y_ofs)
  {
    for (int i = m_mcu_y_ofs; i < m_mcu_y; i++)
      memcpy(m_mcu_lines[i], m_mcu_lines[m_mcu_y_ofs - 1], m_image_bpl_mcu);

    process_mcu_row();
  }

  return terminate_pass_two();
}

void jpeg_encoder::load_mcu(const void *src)
{
  const uint8* Psrc = reinterpret_cast<const uint8*>(src);

  // OK to write up to m_image_bpl_xlt bytes to Pdst
  uint8* Pdst = m_mcu_lines[m_mcu_y_ofs];

  if (m_num_components == 1)
  {
    if (m_image_bpp == 3)
      RGB_to_Y(Pdst, Psrc, m_image_x);
    else
      memcpy(Pdst, Psrc, m_image_x);
  }
  else
  {
    if (m_image_bpp == 3)
      RGB_to_YCC(Pdst, Psrc, m_image_x);
    else
      Y_to_YCC(Pdst, Psrc, m_image_x);
  }

  if (m_num_components == 1)
    memset(m_mcu_lines[m_mcu_y_ofs] + m_image_bpl_xlt, Pdst[m_image_bpl_xlt - 1], m_image_x_mcu - m_image_x);
  else
  {
    const uint8 y  = Pdst[m_image_bpl_xlt - 3 + 0];
    const uint8 cb = Pdst[m_image_bpl_xlt - 3 + 1];
    const uint8 cr = Pdst[m_image_bpl_xlt - 3 + 2];
    uint8 *dst = m_mcu_lines[m_mcu_y_ofs] + m_image_bpl_xlt;

    for (int i = m_image_x; i < m_image_x_mcu; i++)
    {
      *dst++ = y;
      *dst++ = cb;
      *dst++ = cr;
    }
  }

  if (++m_mcu_y_ofs == m_mcu_y)
  {
    process_mcu_row();
    m_mcu_y_ofs = 0;
  }
}

void jpeg_encoder::clear()
{
  m_num_components = 0;
  m_image_x = 0;
  m_image_y = 0;
  m_image_bpp = 0;
  m_image_bpl = 0;
  m_image_x_mcu = 0;
  m_image_y_mcu = 0;
  m_image_bpl_xlt = 0;
  m_image_bpl_mcu = 0;
  m_mcus_per_row = 0;
  m_mcu_x = 0;
  m_mcu_y = 0;
  m_mcu_lines = NULL;
  m_mcu_y_ofs = 0;
  m_sample_array = NULL;
  m_coefficient_array = NULL;
  m_out_buf = NULL;
  m_out_buf_ofs = NULL;
  m_out_buf_left = 0;
  m_bit_buffer = 0;
  m_bits_in = 0;
  clear_obj(m_comp_h_samp);
  clear_obj(m_comp_v_samp);
  clear_obj(m_quantization_tables);
  clear_obj(m_huff_codes);
  clear_obj(m_huff_code_sizes);
  clear_obj(m_huff_bits);
  clear_obj(m_huff_val);
  clear_obj(m_last_dc_val);
  m_all_stream_writes_succeeded = true;
}

jpeg_encoder::jpeg_encoder()
{
  clear();
}

jpeg_encoder::~jpeg_encoder()
{
  deinit();
}

bool jpeg_encoder::init(output_stream *pStream, int width, int height, int src_channels, const params &comp_params)
{
  deinit();

  if ((!pStream) || (width < 1) || (height < 1)) return false;
  if ((src_channels != 1) && (src_channels != 3)) return false;
  if (!comp_params.check()) return false;

  m_pStream = pStream;
  m_params = comp_params;

  return jpg_open(width, height, src_channels);
}

void jpeg_encoder::deinit()
{
  if (m_mcu_lines)
  {
    for (int i = 0; i < m_mcu_y; i++)
      jpge_free(m_mcu_lines[i]);
    jpge_free(m_mcu_lines);
  }

  jpge_free(m_sample_array);
  jpge_free(m_coefficient_array);
  jpge_free(m_quantization_tables[0]);
  jpge_free(m_quantization_tables[1]);

  for (int i = 0; i < 4; i++)
  {
    jpge_free(m_huff_codes[i]);
    jpge_free(m_huff_code_sizes[i]);
    jpge_free(m_huff_bits[i]);
    jpge_free(m_huff_val[i]);
  }
  jpge_free(m_out_buf);
  clear();
}

bool jpeg_encoder::process_scanline(const void* pScanline)
{
  if (m_all_stream_writes_succeeded)
  {
    if (!pScanline)
    {
      if (!process_end_of_image())
          return false;
    }
    else
    {
      load_mcu(pScanline);
    }
  }

  return m_all_stream_writes_succeeded;
}

// Higher level wrappers (optional).

#include <stdio.h>

class cfile_stream : public output_stream
{
   cfile_stream(const cfile_stream &);
   cfile_stream &operator= (const cfile_stream &);

   FILE* m_pFile;
   bool m_bStatus;

public:
   cfile_stream() : m_pFile(NULL), m_bStatus(false) { }

   virtual ~cfile_stream()
   {
      close();
   }

   bool open(const char *pFilename)
   {
      close();
      m_pFile = fopen(pFilename, "wb");
      m_bStatus = (m_pFile != NULL);
      return m_bStatus;
   }

   bool close()
   {
      if (m_pFile)
      {
         if (fclose(m_pFile) == EOF)
         {
            m_bStatus = false;
         }
      }
      return m_bStatus;
   }

   virtual bool put_buf(const void* pBuf, int len)
   {
      m_bStatus = m_bStatus && (fwrite(pBuf, len, 1, m_pFile) == 1);
      return m_bStatus;
   }

   uint get_size() const
   {
      return m_pFile ? ftell(m_pFile) : 0;
   }
};

// Writes JPEG image to file.
bool compress_image_to_jpeg_file(const char *pFilename, int width, int height, int num_channels, const uint8 *pImage_data, const params &comp_params)
{
   cfile_stream dst_stream;
   if (!dst_stream.open(pFilename))
      return false;

   jpge::jpeg_encoder dst_image;
   if (!dst_image.init(&dst_stream, width, height, num_channels, comp_params))
      return false;

    for (int i = 0; i < height; i++)
    {
       const uint8* pBuf = pImage_data + i * width * num_channels;
       if (!dst_image.process_scanline(pBuf))
          return false;
    }
    if (!dst_image.process_scanline(NULL))
       return false;

   dst_image.deinit();

   return dst_stream.close();
}

class memory_stream : public output_stream
{
   memory_stream(const memory_stream &);
   memory_stream &operator= (const memory_stream &);

   uint8 *m_pBuf;
   uint m_buf_size, m_buf_ofs;

public:
   memory_stream(void *pBuf, uint buf_size) : m_pBuf(static_cast<uint8*>(pBuf)), m_buf_size(buf_size), m_buf_ofs(0) { }

   virtual ~memory_stream() { }

   virtual bool put_buf(const void* pBuf, int len)
   {
      uint buf_remaining = m_buf_size - m_buf_ofs;
      if ((uint)len > buf_remaining)
         return false;
      memcpy(m_pBuf + m_buf_ofs, pBuf, len);
      m_buf_ofs += len;
      return true;
   }

   uint get_size() const
   {
      return m_buf_ofs;
   }
};

bool compress_image_to_jpeg_file_in_memory(void *pBuf, int &buf_size, int width, int height, int num_channels, const uint8 *pImage_data, const params &comp_params)
{
   if ((!pBuf) || (!buf_size)) 
      return false;

   memory_stream dst_stream(pBuf, buf_size);

   buf_size = 0;

   jpge::jpeg_encoder dst_image;
   if (!dst_image.init(&dst_stream, width, height, num_channels, comp_params))
      return false;

   for (int i = 0; i < height; i++)
   {
      const uint8* pBuf = pImage_data + i * width * num_channels;
      if (!dst_image.process_scanline(pBuf))
         return false;
   }
   if (!dst_image.process_scanline(NULL))
      return false;

   dst_image.deinit();

   buf_size = dst_stream.get_size();
   return true;
}

} // namespace jpge
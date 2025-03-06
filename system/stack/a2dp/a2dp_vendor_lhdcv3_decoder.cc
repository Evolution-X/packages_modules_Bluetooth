/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "bluetooth-a2dp"
#include "a2dp_vendor_lhdcv3_decoder.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <lhdcBT_dec.h>
#include "stack/include/bt_hdr.h"
#include "common/time_util.h"
#include "btif/include/btif_av_co.h"
#include "internal_include/bt_trace.h"
#include "stack/include/bt_hdr.h"
#include "osi/include/osi.h"
#include <bluetooth/log.h>
#include "os/log.h"
using namespace bluetooth;


#define A2DP_LHDC_FUNC_DISABLE		0
#define A2DP_LHDC_FUNC_ENABLE		1

#define LHDCV3_DEC_MAX_SAMPLES_PER_FRAME  256
#define LHDCV3_DEC_MAX_CHANNELS           2
#define LHDCV3_DEC_MAX_BIT_DEPTH          32
#define LHDCV3_DEC_FRAME_NUM              16
#define LHDCV3_DEC_BUF_BYTES              (LHDCV3_DEC_FRAME_NUM * LHDCV3_DEC_MAX_SAMPLES_PER_FRAME * LHDCV3_DEC_MAX_CHANNELS * (LHDCV3_DEC_MAX_BIT_DEPTH >> 3))
#define LHDCV3_DEC_PACKET_NUM             8

#define LHDCV3_DEC_INPUT_BUF_BYTES        1024

#define LHDCV3_DEC_PKT_HDR_BYTES          2


typedef struct {
  lhdc_ver_t  version;
  uint32_t    sample_rate;
  uint8_t     bits_per_sample;
  uint8_t     func_ch_split;
  uint8_t     func_ar;
  uint8_t     func_jas;
  uint8_t     func_meta;

  uint32_t    timestamp;        // Timestamp for the A2DP frames
  uint8_t     decode_buf[LHDCV3_DEC_PACKET_NUM][LHDCV3_DEC_BUF_BYTES];
  uint32_t    dec_buf_idx;

  uint8_t     dec_input_buf[LHDCV3_DEC_INPUT_BUF_BYTES];
  uint32_t    dec_input_buf_bytes;

  decoded_data_callback_t decode_callback;
} tA2DP_LHDCV3_DECODER_CB;

static tA2DP_LHDCV3_DECODER_CB a2dp_lhdcv3_decoder_cb;


//#define _DEC_REC_FILE_
#if defined(_DEC_REC_FILE_)
#define RAW_FILE_NAME "/sdcard/Download/lhdcdec.raw"
#define PCM_FILE_NAME "/sdcard/Download/decoded.pcm"
static FILE *rawFile = NULL;
static FILE *pcmFile = NULL;
#endif

//
// Decoder for LHDC Sink Codec
//

//
// The LHDC decoder shared library, and the functions to use
//
static const char* LHDC_DECODER_LIB_NAME = "liblhdcBT_dec.so";
static void* lhdc_decoder_lib_handle = NULL;


static const char* LHDCDEC_INIT_DECODER_NAME = "lhdcBT_dec_init_decoder";
typedef int (*tLHDCDEC_INIT_DECODER)(tLHDCV3_DEC_CONFIG *config);

static const char* LHDCDEC_CHECK_FRAME_DATA_ENOUGH_NAME = "lhdcBT_dec_check_frame_data_enough";
typedef int (*tLHDCDEC_CHECK_FRAME_DATA_ENOUGH)(const uint8_t *frameData, uint32_t frameBytes, uint32_t *packetBytes);

static const char* LHDCDEC_DECODE_NAME = "lhdcBT_dec_decode";
typedef int (*tLHDCDEC_DECODE)(const uint8_t *frameData, uint32_t frameBytes, uint8_t* pcmData, uint32_t* pcmBytes, uint32_t bits_depth);

static const char* LHDCDEC_DEINIT_DECODER_NAME = "lhdcBT_dec_deinit_decoder";
typedef int (*tLHDCDEC_DEINIT_DECODER)(void);


static tLHDCDEC_INIT_DECODER lhdcdec_init_decoder;
static tLHDCDEC_CHECK_FRAME_DATA_ENOUGH lhdcdec_check_frame_data_enough;
static tLHDCDEC_DECODE lhdcdec_decode;
static tLHDCDEC_DEINIT_DECODER lhdcdec_deinit_decoder;




// LHDC v4 Extend flags
#define A2DP_LHDC_FLAG_JAS            0x40
#define A2DP_LHDC_FLAG_AR             0x80

#define A2DP_LHDC_FLAG_LLAC           0x80

#define A2DP_LHDC_FLAG_META           0x10
#define A2DP_LHDC_FLAG_MBR            0x20
#define A2DP_LHDC_FLAG_LARC           0x40
#define A2DP_LHDC_FLAG_V4             0x80



// offset  0		1B	codec capability length (11 Bytes)
// offset  1		1B	[7:4] media type
// offset  2		1B  codec type
// offset  3		4B	Vendor ID
// offset  7		2B	Codec ID
// offset  9		3B 	LHDC specific capability
#define A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1	9
#define A2DP_LHDCV3_CODEC_INFO_SPECIFIC_2	10
#define A2DP_LHDCV3_CODEC_INFO_SPECIFIC_3	11


bool save_codec_info (const uint8_t* p_codec_info)
{
  if (p_codec_info == NULL)
  {
    return false;
  }


  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & 
      A2DP_LHDC_SAMPLING_FREQ_44100)
  {
    a2dp_lhdcv3_decoder_cb.sample_rate = 44100;
  }
  else if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & 
           A2DP_LHDC_SAMPLING_FREQ_48000)
  {
	a2dp_lhdcv3_decoder_cb.sample_rate = 48000;
  }
  else if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & 
           A2DP_LHDC_SAMPLING_FREQ_96000)
  {
	a2dp_lhdcv3_decoder_cb.sample_rate = 96000;
  }
  else
  {
    return false;
  }


  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & 
      A2DP_LHDC_BIT_FMT_16)
  {
    a2dp_lhdcv3_decoder_cb.bits_per_sample = 16;
  }
  else if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & 
           A2DP_LHDC_BIT_FMT_24)
  {
	a2dp_lhdcv3_decoder_cb.bits_per_sample = 24;
  }
  else
  {
    return false;
  }

  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_2] & A2DP_LHDC_FLAG_LLAC)
  {
    //LLAC only 
    a2dp_lhdcv3_decoder_cb.version = VERSION_LLAC;
    log::info(":LLAC only" );
  }
  else if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_3] & A2DP_LHDC_FLAG_V4) 
  {
    //LHDCV4 only 
    a2dp_lhdcv3_decoder_cb.version = VERSION_4;
    log::info(":LHDC V4 only");
  }
  else
  {
    //LHDCV3 only 
	a2dp_lhdcv3_decoder_cb.version = VERSION_3;
    log::info("':LHDC V3 only");
  }

  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_3] & A2DP_LHDC_CH_SPLIT_NONE)
  {
    a2dp_lhdcv3_decoder_cb.func_ch_split = A2DP_LHDC_FUNC_DISABLE;
  }
  else if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_3] & A2DP_LHDC_CH_SPLIT_TWS)
  {
	a2dp_lhdcv3_decoder_cb.func_ch_split = A2DP_LHDC_FUNC_ENABLE;
  }
  else
  {
    return false;
  }

  // AR
  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & A2DP_LHDC_FLAG_AR)
  {
	a2dp_lhdcv3_decoder_cb.func_ar = A2DP_LHDC_FUNC_ENABLE;
  }
  else
  {
    a2dp_lhdcv3_decoder_cb.func_ar = A2DP_LHDC_FUNC_DISABLE;
  }

  // JAS
  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_1] & A2DP_LHDC_FLAG_JAS)
  {
	a2dp_lhdcv3_decoder_cb.func_jas = A2DP_LHDC_FUNC_ENABLE;
  }
  else
  {
    a2dp_lhdcv3_decoder_cb.func_jas = A2DP_LHDC_FUNC_DISABLE;
  }

  // META
  if (p_codec_info[A2DP_LHDCV3_CODEC_INFO_SPECIFIC_3] & A2DP_LHDC_FLAG_META)
  {
	a2dp_lhdcv3_decoder_cb.func_meta = A2DP_LHDC_FUNC_ENABLE;
  }
  else
  {
    a2dp_lhdcv3_decoder_cb.func_meta = A2DP_LHDC_FUNC_DISABLE;
  }

  return true;

}


static void* load_func(const char* func_name) {

  void* func_ptr = NULL;

  if ((func_name == NULL) ||
      (lhdc_decoder_lib_handle == NULL))  {

    return NULL;
  }

  func_ptr = dlsym(lhdc_decoder_lib_handle, func_name);

  if (func_ptr == NULL) {
    log::error("cannot find function ,in the encoder library:{}", dlerror());
    A2DP_VendorUnloadDecoderLhdcV3();
    return NULL;
  }

  return func_ptr;
}


bool A2DP_VendorLoadDecoderLhdcV3(void) {

  if (lhdc_decoder_lib_handle != NULL) return true;  // Already loaded

  memset(&a2dp_lhdcv3_decoder_cb, 0, sizeof(a2dp_lhdcv3_decoder_cb));

  // Open the encoder library
  lhdc_decoder_lib_handle = dlopen(LHDC_DECODER_LIB_NAME, RTLD_NOW);
  if (lhdc_decoder_lib_handle == NULL) {
  //  LOG_ERROR("%s: cannot open LHDC decoder library %s: %s", __func__,
             // LHDC_DECODER_LIB_NAME, dlerror());
    log::error("cannot open LHDC decoder library : {}",LHDC_DECODER_LIB_NAME,dlerror());
              
    return false;
  }


  // Load all functions
  lhdcdec_init_decoder = (tLHDCDEC_INIT_DECODER)load_func(LHDCDEC_INIT_DECODER_NAME);
  if (lhdcdec_init_decoder == NULL) return false;

  lhdcdec_check_frame_data_enough = (tLHDCDEC_CHECK_FRAME_DATA_ENOUGH)load_func(LHDCDEC_CHECK_FRAME_DATA_ENOUGH_NAME);
  if (lhdcdec_check_frame_data_enough == NULL) return false;

  lhdcdec_decode = (tLHDCDEC_DECODE)load_func(LHDCDEC_DECODE_NAME);
  if (lhdcdec_decode == NULL) return false;

  lhdcdec_deinit_decoder = (tLHDCDEC_DEINIT_DECODER)load_func(LHDCDEC_DEINIT_DECODER_NAME);
  if (lhdcdec_deinit_decoder == NULL) return false;

  return true;
}


void A2DP_VendorUnloadDecoderLhdcV3(void) {
  a2dp_vendor_lhdcv3_decoder_cleanup();
}


bool a2dp_vendor_lhdcv3_decoder_init(decoded_data_callback_t decode_callback) {
  //LOG_ERROR("%s: A2DP Sink", __func__);
  log::error("A2DP Sink");

  tLHDCV3_DEC_CONFIG  lhdcdec_config;
  int  fn_ret;


  if ((lhdc_decoder_lib_handle == NULL) ||
      (lhdcdec_init_decoder == NULL) ||
	  (lhdcdec_deinit_decoder == NULL)) {

	return false;
  }

  lhdcdec_deinit_decoder ();

  lhdcdec_config.version = a2dp_lhdcv3_decoder_cb.version;
  lhdcdec_config.sample_rate = a2dp_lhdcv3_decoder_cb.sample_rate;
  lhdcdec_config.bits_depth = a2dp_lhdcv3_decoder_cb.bits_per_sample;

  fn_ret = lhdcdec_init_decoder (&lhdcdec_config);

  if (fn_ret != LHDCBT_DEC_FUNC_SUCCEED) {

    return false;
  }

  a2dp_lhdcv3_decoder_cb.dec_buf_idx = 0;
  a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes = 0;
  a2dp_lhdcv3_decoder_cb.decode_callback = decode_callback;

#if defined(_DEC_REC_FILE_)
  if (rawFile == NULL) {
    rawFile = fopen(RAW_FILE_NAME,"wb");
    log::info(":Create recode file = {}" ,rawFile);
  }
  if (pcmFile == NULL) {
    pcmFile = fopen(PCM_FILE_NAME,"wb");
    log::info(":Create recode file = {}",pcmFile);
  }
#endif
  return true;
}


void a2dp_vendor_lhdcv3_decoder_cleanup(void) {
  // Cleanup any LHDC-related state

  log::info(":lhdc_decoder_lib_handle = {}",lhdc_decoder_lib_handle);

  if (lhdc_decoder_lib_handle == NULL) {

	return;
  }

  if (lhdcdec_deinit_decoder != NULL) {

    lhdcdec_deinit_decoder ();
  }

  memset(&a2dp_lhdcv3_decoder_cb, 0, sizeof(a2dp_lhdcv3_decoder_cb));

  dlclose(lhdc_decoder_lib_handle);
  lhdc_decoder_lib_handle = NULL;

#if defined(_DEC_REC_FILE_)
  if (rawFile != NULL) {
    fclose(rawFile);
    rawFile = NULL;
    remove(RAW_FILE_NAME);
  }
  if (pcmFile != NULL) {
    fclose(pcmFile);
    pcmFile = NULL;
    remove(PCM_FILE_NAME);
  }
#endif
}

bool a2dp_vendor_lhdcv3_decoder_decode_packet(BT_HDR* p_buf) {
  uint8_t* data;
  size_t data_size;
  uint32_t out_used = 0;
  int fn_ret;
  uint32_t dec_buf_idx;
  uint8_t *ptr_src;
  uint8_t *ptr_dst;
  uint32_t packet_bytes;
  uint32_t i;


  if (p_buf == NULL) {

	return false;
  }

  data = p_buf->data + p_buf->offset;
  data_size = p_buf->len;

  dec_buf_idx = a2dp_lhdcv3_decoder_cb.dec_buf_idx++;
  if (a2dp_lhdcv3_decoder_cb.dec_buf_idx >= LHDCV3_DEC_PACKET_NUM)
  {
	a2dp_lhdcv3_decoder_cb.dec_buf_idx = 0;
  }


  if (data_size == 0) {
    //LOG_ERROR("%s: Empty packet", __func__);
    log::error(":Empty packet");
    return false;
  }

  if ((lhdc_decoder_lib_handle == NULL) ||
      (lhdcdec_decode == NULL)) {

    //LOG_ERROR("%s: Invalid handle!", __func__);
    log::error("Invalid handle!");
    return false;
  }

#if defined(_DEC_REC_FILE_)
  if (rawFile != NULL && data_size > 0) {
    fwrite(data + LHDCV3_DEC_PKT_HDR_BYTES, sizeof(uint8_t), data_size - LHDCV3_DEC_PKT_HDR_BYTES, rawFile);
  }
#endif

  if ((a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes + data_size) > LHDCV3_DEC_INPUT_BUF_BYTES)
  {
	// the data queued is useless
	// discard them
    a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes = 0;

	if (data_size > LHDCV3_DEC_INPUT_BUF_BYTES)
	{
	  // input data is too big (more than buffer size)!!
	  // just ingore it, and do nothing
	  return true;
	}
  }

  memcpy (&(a2dp_lhdcv3_decoder_cb.dec_input_buf[a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes]), 
          data, 
          data_size);
  a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes += data_size;

  packet_bytes = 0;
  fn_ret = lhdcdec_check_frame_data_enough (a2dp_lhdcv3_decoder_cb.dec_input_buf, 
                                            a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes,
											&packet_bytes);

  if (fn_ret == LHDCBT_DEC_FUNC_INPUT_NOT_ENOUGH) {
    //LOG_ERROR("%s: Input buffer is NOT enough!, but return true", __func__);
    log::error("Input buffer is NOT enough!, but return true");
    return true;
  }
  else if (fn_ret != LHDCBT_DEC_FUNC_SUCCEED) {

    //LOG_ERROR("%s: fail to check frame data!", __func__);
    log::error(":fail to check frame data!");
    // clear the data in the input buffer
    a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes = 0;
    return false;
  }

  if (packet_bytes != (a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes - LHDCV3_DEC_PKT_HDR_BYTES))
  {
	// strange!
	// queued data is NOT exactly equal to one packet!
	// maybe wrong data in buffer
	// discard data queued previously, and save input data
	//LOG_ERROR("%s: queued data is NOT exactly equal to one packet! packet (%d),  input (%d)", __func__, packet_bytes, a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes);
  log::error("queued data is NOT exactly equal to one packet! packet {},{}",packet_bytes,a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes);
	a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes = 0;
	memcpy (&(a2dp_lhdcv3_decoder_cb.dec_input_buf[a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes]), 
            data, 
            data_size);
    a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes += data_size;
	return true;
  }

  out_used = sizeof(a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx]);
  fn_ret = lhdcdec_decode (a2dp_lhdcv3_decoder_cb.dec_input_buf, 
                           a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes, 
                           a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx],
						   &out_used,
						   a2dp_lhdcv3_decoder_cb.bits_per_sample);

  // finish decoding
  // clear the data in the input buffer
  a2dp_lhdcv3_decoder_cb.dec_input_buf_bytes = 0;

  if (fn_ret != LHDCBT_DEC_FUNC_SUCCEED) {
      log::error("fail to decode lhdc stream!");
    return false;
  }

  if (a2dp_lhdcv3_decoder_cb.bits_per_sample == 24) {
    ptr_src = a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx];
	ptr_dst = a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx];

    for (i = 0; i < (out_used >> 2) ; i++) {
	  *ptr_dst++ = *ptr_src++;
	  *ptr_dst++ = *ptr_src++;
	  *ptr_dst++ = *ptr_src++;
	  ptr_src++;
    }
	
	out_used = (out_used >> 2) * 3;
  }

#if defined(_DEC_REC_FILE_)
  if (pcmFile != NULL && out_used > 0 && out_used <= sizeof(a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx])) {
    int write_bytes;

    write_bytes = fwrite(a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx], sizeof(uint8_t), out_used, pcmFile);
  }
#endif

  a2dp_lhdcv3_decoder_cb.decode_callback(
      reinterpret_cast<uint8_t*>(a2dp_lhdcv3_decoder_cb.decode_buf[dec_buf_idx]), out_used);

  return true;
}

void a2dp_vendor_lhdcv3_decoder_start(void) {
  log::info("a2dp_vendor_lhdcv3_decoder_start");
  // do nothing
}

void a2dp_vendor_lhdcv3_decoder_suspend(void) {
  log::info("a2dp_vendor_lhdcv3_decoder_suspend");
  // do nothing
}

void a2dp_vendor_lhdcv3_decoder_configure(const uint8_t* p_codec_info) {
  //int32_t sample_rate;
  //int32_t bits_per_sample;
  //int32_t channel_mode;

  if (p_codec_info == NULL) {
    log::error("p_codec_info is NULL");
    return;
  }
}

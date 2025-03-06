/*
 * Copyright (C) 2022 The Android Open Source Project
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
#include "a2dp_vendor_lhdcv5_decoder.h"

#include <dlfcn.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include <lhdcv5BT_dec.h>

#include "a2dp_vendor_lhdcv5.h"
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

#define LHDCV5_DEC_MAX_SAMPLES_PER_FRAME  256
#define LHDCV5_DEC_MAX_CHANNELS           2
#define LHDCV5_DEC_MAX_BIT_DEPTH          32
#define LHDCV5_DEC_FRAME_NUM              16
#define LHDCV5_DEC_BUF_BYTES              (LHDCV5_DEC_FRAME_NUM * \
                                           LHDCV5_DEC_MAX_SAMPLES_PER_FRAME * \
                                           LHDCV5_DEC_MAX_CHANNELS * \
                                           (LHDCV5_DEC_MAX_BIT_DEPTH >> 3))
#define LHDCV5_DEC_PACKET_NUM             8
#define LHDCV5_DEC_INPUT_BUF_BYTES        1024
#define LHDCV5_DEC_PKT_HDR_BYTES          2

typedef struct {
  pthread_mutex_t mutex;
  HANDLE_LHDCV5_BT lhdc_handle;
  bool has_lhdc_handle;  // True if lhdc_handle is valid

  uint32_t    sample_rate;
  uint8_t     bits_per_sample;
  lhdc_ver_t  version;
  uint8_t     func_ar;
  uint8_t     func_jas;
  uint8_t     func_meta;
  uint8_t     func_lless;
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  uint8_t     func_lless_raw;
#endif
  uint8_t     func_lless96K;

  uint8_t     decode_buf[LHDCV5_DEC_PACKET_NUM][LHDCV5_DEC_BUF_BYTES];
  uint32_t    dec_buf_idx;

  uint8_t     dec_input_buf[LHDCV5_DEC_INPUT_BUF_BYTES];
  uint32_t    dec_input_buf_bytes;

  decoded_data_callback_t decode_callback;
} tA2DP_LHDCV5_DECODER_CB;

static tA2DP_LHDCV5_DECODER_CB a2dp_lhdcv5_decoder_cb;


//#define _V5DEC_REC_FILE_
#if defined(_V5DEC_REC_FILE_)
#define V5RAW_FILE_NAME "/sdcard/Download/lhdcv5dec.raw"
#define V5PCM_FILE_NAME "/sdcard/Download/v5decoded.pcm"
static FILE *rawFile = NULL;
static FILE *pcmFile = NULL;
#endif

//
// The LHDCV5 decoder shared library, and the functions to use
//
static const char* LHDC_DECODER_LIB_NAME = "liblhdcv5BT_dec.so";
static void* lhdc_decoder_lib_handle = NULL;

static const char* LHDCDEC_INIT_DECODER_NAME = "lhdcv5BT_dec_init_decoder";
typedef int32_t (*tLHDCDEC_INIT_DECODER)(HANDLE_LHDCV5_BT *handle,
    tLHDCV5_DEC_CONFIG *config);

static const char* LHDCDEC_CHECK_FRAME_DATA_ENOUGH_NAME =
    "lhdcv5BT_dec_check_frame_data_enough";
typedef int32_t (*tLHDCDEC_CHECK_FRAME_DATA_ENOUGH)(const uint8_t *frameData,
    uint32_t frameBytes, uint32_t *packetBytes);

static const char* LHDCDEC_DECODE_NAME = "lhdcv5BT_dec_decode";
typedef int32_t (*tLHDCDEC_DECODE)(const uint8_t *frameData, uint32_t frameBytes,
    uint8_t* pcmData, uint32_t* pcmBytes, uint32_t bits_depth);

static const char* LHDCDEC_DEINIT_DECODER_NAME = "lhdcv5BT_dec_deinit_decoder";
typedef int32_t (*tLHDCDEC_DEINIT_DECODER)(HANDLE_LHDCV5_BT handle);

static tLHDCDEC_INIT_DECODER lhdcv5dec_init_decoder;
static tLHDCDEC_CHECK_FRAME_DATA_ENOUGH lhdcv5dec_check_frame_data_enough;
static tLHDCDEC_DECODE lhdcv5dec_decode;
static tLHDCDEC_DEINIT_DECODER lhdcv5dec_deinit_decoder;

//LHDCV5 codec info (capabilities) format: refer in a2dp_vendor_lhdcv5_constants.h
#define A2DP_LHDCV5_CODEC_INFO_ATTR_1 (3+6) //P6
#define A2DP_LHDCV5_CODEC_INFO_ATTR_2 (3+7) //P7
#define A2DP_LHDCV5_CODEC_INFO_ATTR_3 (3+8) //P8
#define A2DP_LHDCV5_CODEC_INFO_ATTR_4 (3+9) //P9
#define A2DP_LHDCV5_CODEC_INFO_ATTR_5 (3+10) //P10


bool a2dp_lhdcv5_decoder_save_codec_info (const uint8_t* p_codec_info)
{
  if (p_codec_info == NULL) {
    return false;
  }

  if (lhdc_decoder_lib_handle == NULL) {
    return false;
  }

  // Sampling Frequency
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_1] &
      A2DP_LHDCV5_SAMPLING_FREQ_44100) {
    a2dp_lhdcv5_decoder_cb.sample_rate = 44100;
  } else if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_1] &
      A2DP_LHDCV5_SAMPLING_FREQ_48000) {
    a2dp_lhdcv5_decoder_cb.sample_rate = 48000;
  } else if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_1] &
      A2DP_LHDCV5_SAMPLING_FREQ_96000) {
    a2dp_lhdcv5_decoder_cb.sample_rate = 96000;
  } else if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_1] &
      A2DP_LHDCV5_SAMPLING_FREQ_192000) {
    a2dp_lhdcv5_decoder_cb.sample_rate = 192000;
  } else {
    return false;
  }

  // Bit Depth
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_2] &
      A2DP_LHDCV5_BIT_FMT_16) {
    a2dp_lhdcv5_decoder_cb.bits_per_sample = 16;
  } else if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_2] &
      A2DP_LHDCV5_BIT_FMT_24) {
    a2dp_lhdcv5_decoder_cb.bits_per_sample = 24;
  } else if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_2] &
      A2DP_LHDCV5_BIT_FMT_32) {
    a2dp_lhdcv5_decoder_cb.bits_per_sample = 32;
  } else {
    return false;
  }

  // version
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_3] &
      A2DP_LHDCV5_VER_1) {
    a2dp_lhdcv5_decoder_cb.version = VERSION_5;
  } else {
    return false;
  }

  // AR
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_4] &
      A2DP_LHDCV5_FEATURE_AR) {
    a2dp_lhdcv5_decoder_cb.func_ar = A2DP_LHDC_FUNC_ENABLE;
  } else {
    a2dp_lhdcv5_decoder_cb.func_ar = A2DP_LHDC_FUNC_DISABLE;
  }

  // JAS
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_4] &
      A2DP_LHDCV5_FEATURE_JAS) {
    a2dp_lhdcv5_decoder_cb.func_jas = A2DP_LHDC_FUNC_ENABLE;
  } else {
    a2dp_lhdcv5_decoder_cb.func_jas = A2DP_LHDC_FUNC_DISABLE;
  }

  // META
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_4] &
      A2DP_LHDCV5_FEATURE_META) {
    a2dp_lhdcv5_decoder_cb.func_meta = A2DP_LHDC_FUNC_ENABLE;
  } else {
    a2dp_lhdcv5_decoder_cb.func_meta = A2DP_LHDC_FUNC_DISABLE;
  }

  // LOSSLESS 48KHz
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_4] &
      A2DP_LHDCV5_FEATURE_LLESS48K) {
    a2dp_lhdcv5_decoder_cb.func_lless = A2DP_LHDC_FUNC_ENABLE;
  } else {
    a2dp_lhdcv5_decoder_cb.func_lless = A2DP_LHDC_FUNC_DISABLE;
  }

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  // LOSSLESS RAW Mode: can be enabled when lossless is already enabled
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_5] & A2DP_LHDCV5_FEATURE_LLESS_RAW) {
    if (a2dp_lhdcv5_decoder_cb.func_lless == A2DP_LHDC_FUNC_ENABLE) {
      a2dp_lhdcv5_decoder_cb.func_lless_raw = A2DP_LHDC_FUNC_ENABLE;
    } else {
      a2dp_lhdcv5_decoder_cb.func_lless_raw = A2DP_LHDC_FUNC_DISABLE;
    }
  } else {
    a2dp_lhdcv5_decoder_cb.func_lless_raw = A2DP_LHDC_FUNC_DISABLE;
  }
#endif

  // LOSSLESS 96KHz can be enabled when lossless is already enabled
  if (p_codec_info[A2DP_LHDCV5_CODEC_INFO_ATTR_4] &
      A2DP_LHDCV5_FEATURE_LLESS96K) {
    if (a2dp_lhdcv5_decoder_cb.func_lless == A2DP_LHDC_FUNC_ENABLE) {
      a2dp_lhdcv5_decoder_cb.func_lless96K = A2DP_LHDC_FUNC_ENABLE;
    } else {
      a2dp_lhdcv5_decoder_cb.func_lless96K = A2DP_LHDC_FUNC_DISABLE;
    }
  } else {
    a2dp_lhdcv5_decoder_cb.func_lless96K = A2DP_LHDC_FUNC_DISABLE;
  }

  return true;
}


static void* load_func(const char* func_name) {

  void* func_ptr = NULL;

  if ((func_name == NULL) ||
      (lhdc_decoder_lib_handle == NULL)) {
    log::error( ": null ptr");
    return NULL;
  }

  func_ptr = dlsym(lhdc_decoder_lib_handle, func_name);

  if (func_ptr == NULL) {
    log::error(
        ": cannot find function '{}' in the encoder library: {}",
         func_name, dlerror());
    A2DP_VendorUnloadDecoderLhdcV5();
    return NULL;
  }

  return func_ptr;
}


bool A2DP_VendorLoadDecoderLhdcV5(void) {

  if (lhdc_decoder_lib_handle != NULL) {
    return true;  // Already loaded
  }

  // Initialize the control block
  memset(&a2dp_lhdcv5_decoder_cb, 0, sizeof(a2dp_lhdcv5_decoder_cb));

  pthread_mutex_init(&(a2dp_lhdcv5_decoder_cb.mutex), NULL);

  // Open the encoder library
  lhdc_decoder_lib_handle = dlopen(LHDC_DECODER_LIB_NAME, RTLD_NOW);
  if (lhdc_decoder_lib_handle == NULL) {
    log::error( ": cannot open LHDCV5 decoder library ", dlerror());
    return false;
  }

  // Load all functions
  lhdcv5dec_init_decoder = (tLHDCDEC_INIT_DECODER)load_func(LHDCDEC_INIT_DECODER_NAME);
  if (lhdcv5dec_init_decoder == NULL) return false;

  lhdcv5dec_check_frame_data_enough =
      (tLHDCDEC_CHECK_FRAME_DATA_ENOUGH)load_func(LHDCDEC_CHECK_FRAME_DATA_ENOUGH_NAME);
  if (lhdcv5dec_check_frame_data_enough == NULL) return false;

  lhdcv5dec_decode = (tLHDCDEC_DECODE)load_func(LHDCDEC_DECODE_NAME);
  if (lhdcv5dec_decode == NULL) return false;

  lhdcv5dec_deinit_decoder =
      (tLHDCDEC_DEINIT_DECODER)load_func(LHDCDEC_DEINIT_DECODER_NAME);
  if (lhdcv5dec_deinit_decoder == NULL) return false;

  log::info( ": LHDCV5 decoder library loaded");
  return true;
}


void A2DP_VendorUnloadDecoderLhdcV5(void) {

  a2dp_vendor_lhdcv5_decoder_cleanup();

  pthread_mutex_destroy(&(a2dp_lhdcv5_decoder_cb.mutex));
  memset(&a2dp_lhdcv5_decoder_cb, 0, sizeof(a2dp_lhdcv5_decoder_cb));

  lhdcv5dec_init_decoder = NULL;
  lhdcv5dec_check_frame_data_enough = NULL;
  lhdcv5dec_decode = NULL;
  lhdcv5dec_deinit_decoder = NULL;

  if (lhdc_decoder_lib_handle != NULL) {
    dlclose(lhdc_decoder_lib_handle);
    lhdc_decoder_lib_handle = NULL;
  }

#if defined(_V5DEC_REC_FILE_)
  if (rawFile != NULL) {
    fclose(rawFile);
    rawFile = NULL;
    remove(V5RAW_FILE_NAME);
  }
  if (pcmFile != NULL) {
    fclose(pcmFile);
    pcmFile = NULL;
    remove(V5PCM_FILE_NAME);
  }
#endif
  log::info( ": unload LHDC V5 decoder");
}


bool a2dp_vendor_lhdcv5_decoder_init(decoded_data_callback_t decode_callback) {
  int32_t api_ret;
  tLHDCV5_DEC_CONFIG lhdcdec_config;

  if ((lhdc_decoder_lib_handle == NULL) ||
      (lhdcv5dec_init_decoder == NULL) ||
      (lhdcv5dec_deinit_decoder == NULL)) {
    return false;
  }

  pthread_mutex_lock(&(a2dp_lhdcv5_decoder_cb.mutex));

  //log::info( ": has_lhdc_handle({}) handle_base ({}) handle({})",
     // a2dp_lhdcv5_decoder_cb.has_lhdc_handle,
    //  &(a2dp_lhdcv5_decoder_cb.lhdc_handle),
     // a2dp_lhdcv5_decoder_cb.lhdc_handle);

  if (a2dp_lhdcv5_decoder_cb.has_lhdc_handle) {
    api_ret = lhdcv5dec_deinit_decoder(a2dp_lhdcv5_decoder_cb.lhdc_handle);
    if (api_ret != LHDCV5BT_DEC_API_SUCCEED) {
      log::error( ": fail to deinit decoder {}", api_ret);
      pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
      return false;
    }
    a2dp_lhdcv5_decoder_cb.has_lhdc_handle = false;
    a2dp_lhdcv5_decoder_cb.lhdc_handle = NULL;
    log::info( ": handle cleaned");
  }

  lhdcdec_config.version = a2dp_lhdcv5_decoder_cb.version;
  lhdcdec_config.sample_rate = a2dp_lhdcv5_decoder_cb.sample_rate;
  lhdcdec_config.bits_depth = a2dp_lhdcv5_decoder_cb.bits_per_sample;
  lhdcdec_config.bit_rate = 400000;
  lhdcdec_config.lossless_enable = a2dp_lhdcv5_decoder_cb.func_lless;
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  lhdcdec_config.lossless_raw_enable = a2dp_lhdcv5_decoder_cb.func_lless_raw;
#endif

  //check supported configuration for lossless 96KHz (24bit)
  if (a2dp_lhdcv5_decoder_cb.func_lless == 1 &&
      a2dp_lhdcv5_decoder_cb.func_lless96K == 1 &&
      a2dp_lhdcv5_decoder_cb.sample_rate == 96000) {
    if (a2dp_lhdcv5_decoder_cb.bits_per_sample != 24) {
      log::error( ": wrong configuration for lossless 96KHz 24bit!");
      pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
      return false;
    }
  }

  if (a2dp_lhdcv5_decoder_cb.has_lhdc_handle == false &&
      a2dp_lhdcv5_decoder_cb.lhdc_handle == NULL) {
    log::info( ": to init decoder...");
    api_ret = lhdcv5dec_init_decoder(&(a2dp_lhdcv5_decoder_cb.lhdc_handle), &lhdcdec_config);
    if (api_ret != LHDCV5BT_DEC_API_SUCCEED) {
      log::error( ": falied to init decoder {}", api_ret);
      pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
      return false;
    }
    a2dp_lhdcv5_decoder_cb.has_lhdc_handle = true;
  }

  a2dp_lhdcv5_decoder_cb.dec_buf_idx = 0;
  a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes = 0;
  a2dp_lhdcv5_decoder_cb.decode_callback = decode_callback;

#if defined(_V5DEC_REC_FILE_)
  if (rawFile == NULL) {
    rawFile = fopen(V5RAW_FILE_NAME,"wb");
    log::info( ": create recode file = {}", rawFile);
  }
  if (pcmFile == NULL) {
    pcmFile = fopen(V5PCM_FILE_NAME,"wb");
    log::info( ": create recode file = {}", pcmFile);
  }
#endif

  log::info( ": init LHDCV5 decoder success");

  pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
  return true;
}


void a2dp_vendor_lhdcv5_decoder_cleanup(void) {
  int32_t api_ret;

  pthread_mutex_lock(&(a2dp_lhdcv5_decoder_cb.mutex));

  if (a2dp_lhdcv5_decoder_cb.has_lhdc_handle) {
    api_ret = lhdcv5dec_deinit_decoder(a2dp_lhdcv5_decoder_cb.lhdc_handle);
    if (api_ret != LHDCV5BT_DEC_API_SUCCEED) {
      log::error( ": fail to deinit LHDCV5 decoder {}", api_ret);
      pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
      return;
    }
  }

  a2dp_lhdcv5_decoder_cb.has_lhdc_handle = false;
  a2dp_lhdcv5_decoder_cb.lhdc_handle = NULL;

  log::info( ": deinit LHDCV5 decoder success");
  pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
}


bool a2dp_vendor_lhdcv5_decoder_decode_packet(BT_HDR* p_buf) {
  int32_t api_ret;
  uint8_t *data;
  size_t data_size;
  uint32_t out_used = 0;
  uint32_t dec_buf_idx;
  uint8_t *ptr_src;
  uint8_t *ptr_dst;
  uint32_t packet_bytes;
  uint32_t i;

  log::info( ": enter");


  if ((lhdc_decoder_lib_handle == NULL) ||
      (lhdcv5dec_decode == NULL)) {
    log::error( ": lib not loaded!");
    return false;
  }

  // check handle
  if (!a2dp_lhdcv5_decoder_cb.has_lhdc_handle || !a2dp_lhdcv5_decoder_cb.lhdc_handle) {
    log::error( ": handle not existed!");
    return false;
  }

  if (p_buf == NULL) {
    return false;
  }

  pthread_mutex_lock(&(a2dp_lhdcv5_decoder_cb.mutex));

  data = p_buf->data + p_buf->offset;
  data_size = p_buf->len;

  if (data_size == 0) {
    log::error( ": Empty packet");
    pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
    return false;
  }


  dec_buf_idx = a2dp_lhdcv5_decoder_cb.dec_buf_idx++;
  if (a2dp_lhdcv5_decoder_cb.dec_buf_idx >= LHDCV5_DEC_PACKET_NUM) {
    a2dp_lhdcv5_decoder_cb.dec_buf_idx = 0;
  }

#if defined(_V5DEC_REC_FILE_)
  if (rawFile != NULL && data_size > 0) {
    fwrite(data + LHDCV5_DEC_PKT_HDR_BYTES, sizeof(uint8_t),
        data_size - LHDCV5_DEC_PKT_HDR_BYTES, rawFile);
  }
#endif

  if ((a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes + data_size) > LHDCV5_DEC_INPUT_BUF_BYTES) {
    // the data queued is useless
    // discard them
    a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes = 0;

    if (data_size > LHDCV5_DEC_INPUT_BUF_BYTES)
    {
      // input data is too big (more than buffer size)!!
      // just ingore it, and do nothing
      pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
      return true;
    }
  }

  memcpy (&(a2dp_lhdcv5_decoder_cb.dec_input_buf[a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes]),
      data, data_size);
  a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes += data_size;

  packet_bytes = 0;
  api_ret = lhdcv5dec_check_frame_data_enough(a2dp_lhdcv5_decoder_cb.dec_input_buf,
      a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes,
      &packet_bytes);
  if (api_ret != LHDCV5BT_DEC_API_SUCCEED) {
    log::error( ": fail to check frame data! {}", api_ret);
    // clear the data in the input buffer
    a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes = 0;
    pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
    return false;
  }

  if (packet_bytes != (a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes - LHDCV5_DEC_PKT_HDR_BYTES)) {
    // strange!
    // queued data is NOT exactly equal to one packet!
    // maybe wrong data in buffer
    // discard data queued previously, and save input data
    log::error( ": queued data is NOT exactly equal to one packet! packet ({}),  input ({})",
         packet_bytes, a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes);

    a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes = 0;
    memcpy(&(a2dp_lhdcv5_decoder_cb.dec_input_buf[a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes]),
        data,
        data_size);
    a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes += data_size;
    pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
    return true;
  }

  out_used = sizeof(a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx]);

  api_ret = lhdcv5dec_decode(a2dp_lhdcv5_decoder_cb.dec_input_buf,
      a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes,
      a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx],
      &out_used,
      a2dp_lhdcv5_decoder_cb.bits_per_sample);

  // finish decoding
  // clear the data in the input buffer
  a2dp_lhdcv5_decoder_cb.dec_input_buf_bytes = 0;

  if (api_ret != LHDCV5BT_DEC_API_SUCCEED) {
    log::error( ": fail to decode lhdc stream! {}", api_ret);
    pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
    return false;
  }

  if (a2dp_lhdcv5_decoder_cb.bits_per_sample == 24) { //PCM_24_BIT_PACKCED
    ptr_src = a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx];
    ptr_dst = a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx];

    for (i = 0; i < (out_used >> 2) ; i++) {
      *ptr_dst++ = *ptr_src++;
      *ptr_dst++ = *ptr_src++;
      *ptr_dst++ = *ptr_src++;
      ptr_src++;
    }
    out_used = (out_used >> 2) * 3;
  } else if (a2dp_lhdcv5_decoder_cb.bits_per_sample == 32) {
    ptr_dst = a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx];

    for (i = 0; i < (out_used >> 2) ; i++) {
      ptr_dst[3] = ptr_dst[2];
      ptr_dst[2] = ptr_dst[1];
      ptr_dst[1] = ptr_dst[0];
      ptr_dst[0] = 0;
      ptr_dst+=4;
    }
  }

#if defined(_V5DEC_REC_FILE_)
  if (pcmFile != NULL && out_used > 0 &&
      out_used <= sizeof(a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx])) {
    fwrite(a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx],
        sizeof(uint8_t), out_used, pcmFile);
  }
#endif

  a2dp_lhdcv5_decoder_cb.decode_callback(
      reinterpret_cast<uint8_t*>(a2dp_lhdcv5_decoder_cb.decode_buf[dec_buf_idx]), out_used);

  pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
  return true;
}

void a2dp_vendor_lhdcv5_decoder_start(void) {
  //pthread_mutex_lock(&(a2dp_lhdcv5_decoder_cb.mutex));
  log::info("");
  // do nothing

  //pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
}

void a2dp_vendor_lhdcv5_decoder_suspend(void) {
  //pthread_mutex_lock(&(a2dp_lhdcv5_decoder_cb.mutex));
  log::info("");
  // do nothing
}

void a2dp_vendor_lhdcv5_decoder_configure(const uint8_t* p_codec_info) {
  if (p_codec_info == NULL) {
    log::info(": p_codec_info is NULL");
    return;
  }
  //pthread_mutex_lock(&(a2dp_lhdcv5_decoder_cb.mutex));
  log::info("");
  //pthread_mutex_unlock(&(a2dp_lhdcv5_decoder_cb.mutex));
}

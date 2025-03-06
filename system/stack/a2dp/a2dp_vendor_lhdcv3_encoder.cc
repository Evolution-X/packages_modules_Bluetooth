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
#define ATRACE_TAG ATRACE_TAG_AUDIO

#include "a2dp_vendor_lhdcv3_encoder.h"

#ifndef OS_GENERIC
#include <cutils/trace.h>
#endif
#include <dlfcn.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include <lhdcBT.h>

#include "a2dp_vendor.h"
#include "a2dp_vendor_lhdcv3.h"
#include "common/time_util.h"
#include "osi/include/allocator.h"
#include "btif/include/btif_av_co.h"
#include "internal_include/bt_trace.h"
#include "stack/include/bt_hdr.h"
#include "osi/include/osi.h"
#include <bluetooth/log.h>
#include "os/log.h"
using namespace bluetooth;

//
// Encoder for LHDC Source Codec
//
//#define MCPS_FRAME_ENCODE    // estimate MCPS of encoding frame

#ifdef MCPS_FRAME_ENCODE
#define STATIS_MILLION_UNIT   (1000000)
#define STATIS_KILO_UNIT      (1000)
#define STATIS_CPU_FREQ_MHZ   (1708800 / 1000)  //referenced CPU clock: 1708800KHz
#define MCPS_FRAME_ENCODE_DUR (60)             //statistics duration (sec)
uint64_t mcpsStat_time_dur_cnt_lhdcv3 = 0;     //statistics duration counter (sec)
uint64_t mcpsStat_all_enc_us_lhdcv3 = 0;       //total execution time(us) of current encoded frames
uint64_t mcpsStat_all_enc_frm_lhdcv3 = 0;      //total number of current encoded frames
uint64_t mcpsStat_enc_frm_per_sec_lhdcv3 = 0;  //number of frame encoded in one sec
uint64_t mcpsStat_all_time_dur_ms_lhdcv3 = 0;  //total real time(ms) when enter final statistics calculation
#endif

//#define LHDCV3_SRC_EXT_API_ENABLE // Savitehc LHDC_EXT_API

//
// The LHDC encoder shared library, and the functions to use
//
static const char* LHDC_ENCODER_LIB_NAME = "liblhdcBT_enc.so";
static void* lhdc_encoder_lib_handle = NULL;

static const char* LHDC_GET_HANDLE_NAME = "lhdcBT_get_handle";
typedef HANDLE_LHDC_BT (*tLHDC_GET_HANDLE)(int version);

static const char* LHDC_FREE_HANDLE_NAME = "lhdcBT_free_handle";
typedef void (*tLHDC_FREE_HANDLE)(HANDLE_LHDC_BT hLhdcParam);

static const char* LHDC_GET_BITRATE_NAME = "lhdcBT_get_bitrate";
typedef int (*tLHDC_GET_BITRATE)(HANDLE_LHDC_BT hLhdcParam);
static const char* LHDC_SET_BITRATE_NAME = "lhdcBT_set_bitrate";
typedef int (*tLHDC_SET_BITRATE)(HANDLE_LHDC_BT hLhdcParam, int index);

static const char* LHDC_INIT_ENCODER_NAME = "lhdcBT_init_encoder";
typedef int (*tLHDC_INIT_ENCODER)(HANDLE_LHDC_BT hLhdcParam,int sampling_freq, int bitPerSample, int bitrate_inx, int dualChannels, int need_padding, int mtu_size, int interval);

static const char* LHDC_AUTO_ADJUST_BITRATE_NAME = "lhdcBT_adjust_bitrate";
typedef int (*tLHDC_AUTO_ADJUST_BITRATE)(HANDLE_LHDC_BT hLhdcParam, size_t queueLength);


static const char* LHDC_ENCODE_NAME = "lhdcBT_encodeV3";
typedef int (*tLHDC_ENCODE)(HANDLE_LHDC_BT hLhdcParam, void* p_pcm, unsigned char* out_put, uint32_t * written, uint32_t * out_fraems);

static const char* LHDC_SET_LIMIT_BITRATE_ENABLED_NAME = "lhdcBT_set_max_bitrate";
typedef void (*tLHDC_SET_LIMIT_BITRATE_ENABLED)(HANDLE_LHDC_BT hLhdcParam, int max_rate_index);

static const char* LHDC_GET_BLOCK_SIZE = "lhdcBT_get_block_Size";
typedef int (*tLHDC_GET_BLOCK_SIZE)(HANDLE_LHDC_BT hLhdcParam);


static const char* LHDC_SET_EXT_FUNC = "lhdcBT_set_ext_func_state";
typedef int (*tLHDC_SET_EXT_FUNC)(HANDLE_LHDC_BT handle, lhdcBT_ext_func_field_t field, bool enabled, void * priv, int priv_data_len);


static const char* LHDC_SET_MBR_FUNC = "lhdcBT_set_hasMinBitrateLimit";
typedef int (*tLHDC_SET_MBR_FUNC)(HANDLE_LHDC_BT handle, bool enabled);
//int lhdcBT_set_hasMinBitrateLimit(HANDLE_LHDC_BT handle, bool enabled )
//int lhdcBT_set_ext_func_state(HANDLE_LHDC_BT handle, lhdcBT_ext_func_field_t field, bool enabled, void * priv, int priv_data_len)
/*leo set_gyro_pos */
//static const char* LHDC_SET_GYRO_POS_NAME = "lhdcBT_set_gyro_pos";
//typedef int (*tLHDC_SET_GYRO_POS)(HANDLE_LHDC_BT hLhdcParam, uint32_t world_coordinate_x, uint32_t world_coordinate_y, uint32_t world_coordinate_z);


static tLHDC_GET_HANDLE lhdc_get_handle;
static tLHDC_FREE_HANDLE lhdc_free_handle;
static tLHDC_GET_BITRATE lhdc_get_bitrate;
static tLHDC_SET_BITRATE lhdc_set_bitrate;
static tLHDC_INIT_ENCODER lhdc_init_encoder;
static tLHDC_ENCODE lhdc_encode_func;
static tLHDC_AUTO_ADJUST_BITRATE lhdc_auto_adjust_bitrate;
static tLHDC_SET_LIMIT_BITRATE_ENABLED lhdc_set_limit_bitrate;
static tLHDC_GET_BLOCK_SIZE lhdc_get_block_size;
static tLHDC_SET_EXT_FUNC lhdc_set_ext_func;
static tLHDC_SET_MBR_FUNC lhdc_set_mbr_func;



// A2DP LHDC encoder interval in milliseconds
#define A2DP_LHDC_ENCODER_SHORT_INTERVAL_MS 10
#define A2DP_LHDC_ENCODER_INTERVAL_MS 20

// offset
#if (BTA_AV_CO_CP_SCMS_T == TRUE)
#define A2DP_LHDC_OFFSET (AVDT_MEDIA_OFFSET + A2DP_LHDC_MPL_HDR_LEN + 1)
#else
#define A2DP_LHDC_OFFSET (AVDT_MEDIA_OFFSET + A2DP_LHDC_MPL_HDR_LEN)
#endif


namespace fmt {
template <>
struct formatter<LHDCBT_SMPL_FMT_T> : enum_formatter<LHDCBT_SMPL_FMT_T> {};
}  // namespace fmt


//#define A2DP_LHDC_OFFSET (AVDT_MEDIA_OFFSET + 0)

typedef struct {
  uint32_t sample_rate;
  uint8_t channel_mode;
  uint8_t bits_per_sample;
  int quality_mode_index;
  //int latency_mode_index;
  int pcm_wlength;
  LHDCBT_SMPL_FMT_T pcm_fmt;
  int8_t channelSplitMode;
  int8_t maxTargetBitrate;
  bool isLLEnabled;
} tA2DP_LHDC_ENCODER_PARAMS;

typedef struct {
  uint32_t counter;
  uint32_t bytes_per_tick; /* pcm bytes read each media task tick */
  uint64_t last_frame_us;
} tA2DP_LHDC_FEEDING_STATE;

typedef struct {
  uint64_t session_start_us;

  size_t media_read_total_expected_packets;
  size_t media_read_total_expected_reads_count;
  size_t media_read_total_expected_read_bytes;

  size_t media_read_total_dropped_packets;
  size_t media_read_total_actual_reads_count;
  size_t media_read_total_actual_read_bytes;
} a2dp_lhdc_encoder_stats_t;

typedef struct {
  a2dp_source_read_callback_t read_callback;
  a2dp_source_enqueue_callback_t enqueue_callback;
  uint16_t TxAaMtuSize;
  size_t TxQueueLength;

  bool use_SCMS_T;
  bool is_peer_edr;          // True if the peer device supports EDR
  bool peer_supports_3mbps;  // True if the peer device supports 3Mbps EDR
  uint16_t peer_mtu;         // MTU of the A2DP peer
  uint32_t timestamp;        // Timestamp for the A2DP frames

  HANDLE_LHDC_BT lhdc_handle;
  bool has_lhdc_handle;  // True if lhdc_handle is valid
  uint8_t version;

  tA2DP_FEEDING_PARAMS feeding_params;
  tA2DP_LHDC_ENCODER_PARAMS lhdc_encoder_params;
  tA2DP_LHDC_FEEDING_STATE lhdc_feeding_state;

  a2dp_lhdc_encoder_stats_t stats;
  uint32_t buf_seq;
  uint32_t bytes_read;
} tA2DP_LHDC_ENCODER_CB;

//static bool lhdc_abr_loaded = false;



typedef struct _lhdc_frame_Info {
    uint32_t frame_len;
    uint32_t isSplit;
    uint32_t isLeft;

} lhdc_frame_Info_t;


//#define _RECODER_FILE_
#if defined(_RECODER_FILE_)
#define ENCODED_FILE_NAME "/sdcard/Download/lhdc.raw"
#define PCM_FILE_NAME     "/sdcard/Download/source.pcm"
static FILE  *RecFile = NULL;
static FILE *pcmFile = NULL;
#endif

static tA2DP_LHDC_ENCODER_CB a2dp_lhdc_encoder_cb;

static void a2dp_vendor_lhdcv3_encoder_update(uint16_t peer_mtu,
                                            A2dpCodecConfig* a2dp_codec_config,
                                            bool* p_restart_input,
                                            bool* p_restart_output,
                                            bool* p_config_updated);
static void a2dp_lhdcv3_get_num_frame_iteration(uint8_t* num_of_iterations,
                                              uint8_t* num_of_frames,
                                              uint64_t timestamp_us);

static void a2dp_lhdcV3_encode_frames(uint8_t nb_frame);
static bool a2dp_lhdcv3_read_feeding(uint8_t* read_buffer, uint32_t *bytes_read);
static std::string quality_mode_index_to_name(int quality_mode_index);
//static std::string latency_mode_index_to_name(int latency_mode_index);

static void* load_func(const char* func_name) {
  void* func_ptr = dlsym(lhdc_encoder_lib_handle, func_name);
  if (func_ptr == NULL) {
    log::error(
              ": cannot find function '' in the encoder library: ",
               func_name, dlerror());
    A2DP_VendorUnloadEncoderLhdcV3();
    return NULL;
  }
  return func_ptr;
}

bool A2DP_VendorLoadEncoderLhdcV3(void) {
  if (lhdc_encoder_lib_handle != NULL) return true;  // Already loaded

  // Initialize the control block
  memset(&a2dp_lhdc_encoder_cb, 0, sizeof(a2dp_lhdc_encoder_cb));

  // Open the encoder library
  lhdc_encoder_lib_handle = dlopen(LHDC_ENCODER_LIB_NAME, RTLD_NOW);
  if (lhdc_encoder_lib_handle == NULL) {
    log::error( ": cannot open LHDC encoder library : {}",
              LHDC_ENCODER_LIB_NAME, dlerror());
    return false;
  }


  // Load all functions
  lhdc_get_handle = (tLHDC_GET_HANDLE)load_func(LHDC_GET_HANDLE_NAME);
  if (lhdc_get_handle == NULL) return false;
  lhdc_free_handle = (tLHDC_FREE_HANDLE)load_func(LHDC_FREE_HANDLE_NAME);
  if (lhdc_free_handle == NULL) return false;
  lhdc_get_bitrate = (tLHDC_GET_BITRATE)load_func(LHDC_GET_BITRATE_NAME);
  if (lhdc_get_bitrate == NULL) return false;
  lhdc_set_bitrate = (tLHDC_SET_BITRATE)load_func(LHDC_SET_BITRATE_NAME);
  if (lhdc_set_bitrate == NULL) return false;
  lhdc_init_encoder = (tLHDC_INIT_ENCODER)load_func(LHDC_INIT_ENCODER_NAME);
  if (lhdc_init_encoder == NULL) return false;
  lhdc_encode_func = (tLHDC_ENCODE)load_func(LHDC_ENCODE_NAME);
  if (lhdc_encode_func == NULL) return false;
  lhdc_auto_adjust_bitrate = (tLHDC_AUTO_ADJUST_BITRATE)load_func(LHDC_AUTO_ADJUST_BITRATE_NAME);
  if (lhdc_auto_adjust_bitrate == NULL) return false;


  lhdc_set_limit_bitrate = (tLHDC_SET_LIMIT_BITRATE_ENABLED)load_func(LHDC_SET_LIMIT_BITRATE_ENABLED_NAME);
  if (lhdc_set_limit_bitrate == NULL) return false;
  lhdc_get_block_size = (tLHDC_GET_BLOCK_SIZE)load_func(LHDC_GET_BLOCK_SIZE);
  if (lhdc_get_block_size == NULL) return false;
  lhdc_set_ext_func = (tLHDC_SET_EXT_FUNC)load_func(LHDC_SET_EXT_FUNC);
  if (lhdc_set_ext_func == NULL) return false;
  lhdc_set_mbr_func = (tLHDC_SET_MBR_FUNC)load_func(LHDC_SET_MBR_FUNC);
  if (lhdc_set_mbr_func == NULL) return false;

  return true;
}

void A2DP_VendorUnloadEncoderLhdcV3(void) {
  // Cleanup any LHDC-related state

   // log::info( ": a2dp_lhdc_encoder_cb.has_lhdc_handle = {}, lhdc_free_handle = {}",
       //        a2dp_lhdc_encoder_cb.has_lhdc_handle, lhdc_free_handle);
  if (a2dp_lhdc_encoder_cb.has_lhdc_handle && lhdc_free_handle != NULL)
    lhdc_free_handle(a2dp_lhdc_encoder_cb.lhdc_handle);
  memset(&a2dp_lhdc_encoder_cb, 0, sizeof(a2dp_lhdc_encoder_cb));

  lhdc_get_handle = NULL;
  lhdc_free_handle = NULL;
  lhdc_get_bitrate = NULL;
  lhdc_set_bitrate = NULL;
  lhdc_init_encoder = NULL;
  lhdc_encode_func = NULL;
  lhdc_auto_adjust_bitrate = NULL;
  lhdc_set_limit_bitrate = NULL;
  lhdc_get_block_size = NULL;
  lhdc_set_ext_func = NULL;
  lhdc_set_mbr_func = NULL;

  if (lhdc_encoder_lib_handle != NULL) {
    dlclose(lhdc_encoder_lib_handle);
    lhdc_encoder_lib_handle = NULL;
  }
}

void a2dp_vendor_lhdcv3_encoder_init(
    const tA2DP_ENCODER_INIT_PEER_PARAMS* p_peer_params,
    A2dpCodecConfig* a2dp_codec_config,
    a2dp_source_read_callback_t read_callback,
    a2dp_source_enqueue_callback_t enqueue_callback) {
  if (a2dp_lhdc_encoder_cb.has_lhdc_handle)
    lhdc_free_handle(a2dp_lhdc_encoder_cb.lhdc_handle);

  memset(&a2dp_lhdc_encoder_cb, 0, sizeof(a2dp_lhdc_encoder_cb));

  a2dp_lhdc_encoder_cb.stats.session_start_us = bluetooth::common::time_get_os_boottime_us();

  a2dp_lhdc_encoder_cb.read_callback = read_callback;
  a2dp_lhdc_encoder_cb.enqueue_callback = enqueue_callback;
  a2dp_lhdc_encoder_cb.is_peer_edr = p_peer_params->is_peer_edr;
  a2dp_lhdc_encoder_cb.peer_supports_3mbps = p_peer_params->peer_supports_3mbps;
  a2dp_lhdc_encoder_cb.peer_mtu = p_peer_params->peer_mtu;
  a2dp_lhdc_encoder_cb.timestamp = 0;


  a2dp_lhdc_encoder_cb.use_SCMS_T = false;  // TODO: should be a parameter
#if (BTA_AV_CO_CP_SCMS_T == TRUE)
  a2dp_lhdc_encoder_cb.use_SCMS_T = true;
#endif

  // NOTE: Ignore the restart_input / restart_output flags - this initization
  // happens when the connection is (re)started.
  bool restart_input = false;
  bool restart_output = false;
  bool config_updated = false;
  a2dp_vendor_lhdcv3_encoder_update(a2dp_lhdc_encoder_cb.peer_mtu,
                                  a2dp_codec_config, &restart_input,
                                  &restart_output, &config_updated);
}

#if 0
bool A2dpCodecConfigLhdcV3::updateEncoderUserConfig(
    const tA2DP_ENCODER_INIT_PEER_PARAMS* p_peer_params, bool* p_restart_input,
    bool* p_restart_output, bool* p_config_updated) {
  if (a2dp_lhdc_encoder_cb.has_lhdc_handle) {
    lhdc_free_handle(a2dp_lhdc_encoder_cb.lhdc_handle);
    a2dp_lhdc_encoder_cb.has_lhdc_handle = false;
    log::info( ": clean lhdc handle");
  }
  a2dp_lhdc_encoder_cb.is_peer_edr = p_peer_params->is_peer_edr;
  a2dp_lhdc_encoder_cb.peer_supports_3mbps = p_peer_params->peer_supports_3mbps;
  a2dp_lhdc_encoder_cb.peer_mtu = p_peer_params->peer_mtu;
  a2dp_lhdc_encoder_cb.timestamp = 0;

  if (a2dp_lhdc_encoder_cb.peer_mtu == 0) {
    log::error(
              ": Cannot update the codec encoder for : "
              "invalid peer MTU",
               name().c_str());
    return false;
  }

  a2dp_vendor_lhdcv3_encoder_update(a2dp_lhdc_encoder_cb.peer_mtu, this,
                                  p_restart_input, p_restart_output,
                                  p_config_updated);
  return true;
}
#endif

// Update the A2DP LHDC encoder.
// |peer_mtu| is the peer MTU.
// |a2dp_codec_config| is the A2DP codec to use for the update.
static void a2dp_vendor_lhdcv3_encoder_update(uint16_t peer_mtu,
                                            A2dpCodecConfig* a2dp_codec_config,
                                            bool* p_restart_input,
                                            bool* p_restart_output,
                                            bool* p_config_updated) {
  tA2DP_LHDC_ENCODER_PARAMS* p_encoder_params =
      &a2dp_lhdc_encoder_cb.lhdc_encoder_params;
  uint8_t codec_info[AVDT_CODEC_SIZE];

  *p_restart_input = false;
  *p_restart_output = false;
  *p_config_updated = false;

  //Example for limit bit rate
  //lhdc_set_limit_bitrate(a2dp_lhdc_encoder_cb.lhdc_handle, 0);


  if (!a2dp_codec_config->copyOutOtaCodecConfig(codec_info)) {
    log::error(
              ": Cannot update the codec encoder for : {}"
              "invalid codec config",
               a2dp_codec_config->name().c_str());
    return;
  }
  const uint8_t* p_codec_info = codec_info;
  btav_a2dp_codec_config_t codec_config = a2dp_codec_config->getCodecConfig();

  uint32_t verCode = A2DP_VendorGetVersionLhdcV3(p_codec_info);  //LHDC V3 should 1!

  bool isLLAC = A2DP_VendorHasLLACFlagLhdcV3(p_codec_info);

  bool isLHDCV4 = A2DP_VendorHasV4FlagLhdcV3(p_codec_info);


  log::info( ":codec_config.codec_specific_1 = {}, codec_config.codec_specific_2 = {}",  (int32_t)codec_config.codec_specific_1, (int32_t)codec_config.codec_specific_2);
  if ((codec_config.codec_specific_1 & A2DP_LHDC_VENDOR_CMD_MASK) == A2DP_LHDC_QUALITY_MAGIC_NUM) {
      int newValue = codec_config.codec_specific_1 & 0xff;

      // adjust non-supported quality modes and wrap to internal library used index
      if (newValue == A2DP_LHDC_QUALITY_ABR) {
        newValue = LHDCBT_QUALITY_AUTO; //9->8
      }

      if (newValue != p_encoder_params->quality_mode_index) {
        p_encoder_params->quality_mode_index = newValue;
        log::info( ": setting internal quality mode index: ({})", 
                  quality_mode_index_to_name(p_encoder_params->quality_mode_index)
                      .c_str(), p_encoder_params->quality_mode_index);
      }
  }else {
      p_encoder_params->quality_mode_index = LHDCBT_QUALITY_AUTO;
      log::info( ": setting default quality mode to ABR");
  }

  if (!a2dp_lhdc_encoder_cb.has_lhdc_handle) {
      uint32_t versionSetup = 2;
      if(isLLAC && !isLHDCV4 && verCode == 1){
        //LLAC Only
        versionSetup = 4;
        log::info( ": init to LLAC : {}", versionSetup);
      }else if(!isLLAC && isLHDCV4 && verCode == 1) {
        //LHDCV4 Only
        versionSetup = 3;
        log::info( ": init to LHDC V4 : {}", versionSetup);
      }else if(!isLLAC && !isLHDCV4 && verCode == 1) {
        //LHDCV3 Only
        versionSetup = 2;
        log::info( ": init to LHDC V3 : {}", versionSetup);
      }else {
        log::info( ": Flags check incorrect. So init to LHDCV3 only : {}", versionSetup);
      }


      a2dp_lhdc_encoder_cb.lhdc_handle = lhdc_get_handle(versionSetup);
      if (a2dp_lhdc_encoder_cb.lhdc_handle == NULL) {
        log::error( ": Cannot get LHDC encoder handle");
        return;  // TODO: Return an error?
      }
      a2dp_lhdc_encoder_cb.has_lhdc_handle = true;
  }
  a2dp_lhdc_encoder_cb.version = A2DP_VendorGetVersionLhdcV3(p_codec_info);


  // The feeding parameters
  tA2DP_FEEDING_PARAMS* p_feeding_params = &a2dp_lhdc_encoder_cb.feeding_params;
  p_feeding_params->sample_rate =
      A2DP_VendorGetTrackSampleRateLhdcV3(p_codec_info);
  p_feeding_params->bits_per_sample =
      a2dp_codec_config->getAudioBitsPerSample();
  p_feeding_params->channel_count =
      A2DP_VendorGetTrackChannelCountLhdcV3(p_codec_info);
  log::info( ":(feeding) sample_rate={} bits_per_sample={} channel_count={}",
             p_feeding_params->sample_rate,
            p_feeding_params->bits_per_sample, p_feeding_params->channel_count);

  // The codec parameters
  p_encoder_params->sample_rate =
      a2dp_lhdc_encoder_cb.feeding_params.sample_rate;

  uint16_t mtu_size =
      BT_DEFAULT_BUFFER_SIZE - A2DP_LHDC_OFFSET - sizeof(BT_HDR);

  a2dp_lhdc_encoder_cb.TxAaMtuSize = (mtu_size < peer_mtu) ? mtu_size : peer_mtu;

  //get separation feature.
  p_encoder_params->channelSplitMode = A2DP_VendorGetChannelSplitModeLhdcV3(p_codec_info);
  // Set the quality mode index
  //int old_quality_mode_index = p_encoder_params->quality_mode_index;

  p_encoder_params->maxTargetBitrate = A2DP_VendorGetMaxDatarateLhdcV3(p_codec_info);


  p_encoder_params->isLLEnabled = (codec_config.codec_specific_2 & 1ULL) != 0 ? true :false; //A2DP_VendorGetLowLatencyState(p_codec_info);


  p_encoder_params->pcm_wlength =
      a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample >> 3;
  // Set the Audio format from pcm_wlength
  p_encoder_params->pcm_fmt = LHDCBT_SMPL_FMT_S16;
  if (p_encoder_params->pcm_wlength == 2)
    p_encoder_params->pcm_fmt = LHDCBT_SMPL_FMT_S16;
  else if (p_encoder_params->pcm_wlength == 3)
    p_encoder_params->pcm_fmt = LHDCBT_SMPL_FMT_S24;
 // else if (p_encoder_params->pcm_wlength == 4)
//    p_encoder_params->pcm_fmt = LHDCBT_SMPL_FMT_S32;

  log::info( ": MTU={}, peer_mtu={}", 
            a2dp_lhdc_encoder_cb.TxAaMtuSize, peer_mtu);
  log::info(
            ": sample_rate: {} "
            "quality_mode_index: {} pcm_wlength: {} pcm_fmt: {}",
             p_encoder_params->sample_rate,
            p_encoder_params->quality_mode_index, p_encoder_params->pcm_wlength,
            p_encoder_params->pcm_fmt);

#if (BTA_AV_CO_CP_SCMS_T == TRUE)
    uint32_t max_mtu_len = ( uint32_t)( a2dp_lhdc_encoder_cb.TxAaMtuSize - A2DP_LHDC_MPL_HDR_LEN - 1);
#else
    uint32_t max_mtu_len = ( uint32_t)( a2dp_lhdc_encoder_cb.TxAaMtuSize - A2DP_LHDC_MPL_HDR_LEN);
#endif
  
  log::info( ":AR Flag = {}",  A2DP_VendorHasARFlagLhdcV3(p_codec_info));
  lhdc_set_ext_func(a2dp_lhdc_encoder_cb.lhdc_handle, LHDCBT_EXT_FUNC_AR, A2DP_VendorHasARFlagLhdcV3(p_codec_info), NULL, 0);
  lhdc_set_ext_func(a2dp_lhdc_encoder_cb.lhdc_handle, LHDCBT_EXT_FUNC_JAS, A2DP_VendorHasJASFlagLhdcV3(p_codec_info), NULL, 0);
  lhdc_set_ext_func(a2dp_lhdc_encoder_cb.lhdc_handle, LHDCBT_EXT_FUNC_LARC, A2DP_VendorHasLARCFlagLhdcV3(p_codec_info), NULL, 0);
  lhdc_set_mbr_func(a2dp_lhdc_encoder_cb.lhdc_handle, A2DP_VendorHasMinBRFlagLhdcV3(p_codec_info));
  // Initialize the encoder.
  // NOTE: MTU in the initialization must include the AVDT media header size.
  int result = lhdc_init_encoder(
      a2dp_lhdc_encoder_cb.lhdc_handle,
      p_encoder_params->sample_rate,
      p_encoder_params->pcm_fmt,
      p_encoder_params->quality_mode_index,
      p_encoder_params->channelSplitMode > A2DP_LHDC_CH_SPLIT_NONE ? 1 : 0,
      0 /* This parameter alaways is 0 in A2DP */ ,
      max_mtu_len,
      a2dp_vendor_lhdcv3_get_encoder_interval_ms()
  );
  lhdc_set_limit_bitrate(a2dp_lhdc_encoder_cb.lhdc_handle, p_encoder_params->maxTargetBitrate);

  lhdc_set_bitrate(a2dp_lhdc_encoder_cb.lhdc_handle, p_encoder_params->quality_mode_index);

#ifdef MCPS_FRAME_ENCODE
  mcpsStat_all_enc_us_lhdcv3 = 0;
  mcpsStat_all_enc_frm_lhdcv3 = 0;
  mcpsStat_time_dur_cnt_lhdcv3 = 0;
  mcpsStat_enc_frm_per_sec_lhdcv3 = 0;
  mcpsStat_all_time_dur_ms_lhdcv3 = 0;
#endif

#if defined(_RECODER_FILE_)
  if (RecFile == NULL) {
    RecFile = fopen(ENCODED_FILE_NAME,"wb");
    log::info( ": Create recode file = {}",  RecFile);
  }
  if (pcmFile == NULL) {
    pcmFile = fopen(PCM_FILE_NAME,"wb");
    log::info( ": Create recode file = {}",  pcmFile);
  }
#endif
  if (result != 0) {
    log::error( ": error initializing the LHDC encoder: {}", 
              result);
  }
}

void a2dp_vendor_lhdcv3_encoder_cleanup(void) {
  if (a2dp_lhdc_encoder_cb.has_lhdc_handle)
    lhdc_free_handle(a2dp_lhdc_encoder_cb.lhdc_handle);
  memset(&a2dp_lhdc_encoder_cb, 0, sizeof(a2dp_lhdc_encoder_cb));
#if defined(_RECODER_FILE_)
  if (RecFile != NULL) {
    fclose(RecFile);
    RecFile = NULL;
    remove(ENCODED_FILE_NAME);
  }
  if (pcmFile != NULL) {
    fclose(pcmFile);
    pcmFile = NULL;
    remove(PCM_FILE_NAME);
  }
#endif
}

void a2dp_vendor_lhdcv3_feeding_reset(void) {
  /* By default, just clear the entire state */
  memset(&a2dp_lhdc_encoder_cb.lhdc_feeding_state, 0,
         sizeof(a2dp_lhdc_encoder_cb.lhdc_feeding_state));

  int encoder_interval = a2dp_vendor_lhdcv3_get_encoder_interval_ms();
  a2dp_lhdc_encoder_cb.lhdc_feeding_state.bytes_per_tick =
      (a2dp_lhdc_encoder_cb.feeding_params.sample_rate *
       a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample / 8 *
       a2dp_lhdc_encoder_cb.feeding_params.channel_count *
       encoder_interval) /
      1000;
  a2dp_lhdc_encoder_cb.buf_seq = 0;
  a2dp_lhdc_encoder_cb.bytes_read = 0;
  a2dp_lhdc_encoder_cb.lhdc_feeding_state.last_frame_us = 0;
  tA2DP_LHDC_ENCODER_PARAMS* p_encoder_params = &a2dp_lhdc_encoder_cb.lhdc_encoder_params;
  if (p_encoder_params->quality_mode_index == LHDCBT_QUALITY_AUTO) {
    if(lhdc_set_bitrate != NULL && a2dp_lhdc_encoder_cb.has_lhdc_handle) {
      lhdc_set_bitrate(a2dp_lhdc_encoder_cb.lhdc_handle, LHDCBT_QUALITY_RESET_AUTO);
    }
  }
  log::info( ": PCM bytes per tick {}, reset timestamp", 
            a2dp_lhdc_encoder_cb.lhdc_feeding_state.bytes_per_tick);

#ifdef MCPS_FRAME_ENCODE
  mcpsStat_all_enc_us_lhdcv3 = 0;
  mcpsStat_all_enc_frm_lhdcv3 = 0;
  mcpsStat_time_dur_cnt_lhdcv3 = 0;
  mcpsStat_enc_frm_per_sec_lhdcv3 = 0;
  mcpsStat_all_time_dur_ms_lhdcv3 = 0;
#endif
}

void a2dp_vendor_lhdcv3_feeding_flush(void) {
  a2dp_lhdc_encoder_cb.lhdc_feeding_state.counter = 0;
  log::info( "");
}

uint64_t a2dp_vendor_lhdcv3_get_encoder_interval_ms(void) {
  log::info( ": A2DP_LHDC_ENCODER_INTERVAL_MS {}",
               a2dp_lhdc_encoder_cb.lhdc_encoder_params.isLLEnabled ? A2DP_LHDC_ENCODER_SHORT_INTERVAL_MS : A2DP_LHDC_ENCODER_INTERVAL_MS);
  if (a2dp_lhdc_encoder_cb.lhdc_encoder_params.isLLEnabled){
      return A2DP_LHDC_ENCODER_SHORT_INTERVAL_MS;
  }else{
      return A2DP_LHDC_ENCODER_INTERVAL_MS;
  }
}

int a2dp_vendor_lhdcv3_get_effective_frame_size() {
  return a2dp_lhdc_encoder_cb.TxAaMtuSize;
}

void a2dp_vendor_lhdcv3_send_frames(uint64_t timestamp_us) {
  uint8_t nb_frame = 0;
  uint8_t nb_iterations = 0;

  a2dp_lhdcv3_get_num_frame_iteration(&nb_iterations, &nb_frame, timestamp_us);
  log::info( ": Sending {} frames per iteration, {} iterations",
               nb_frame, nb_iterations);

  if (nb_frame == 0) return;

  for (uint8_t counter = 0; counter < nb_iterations; counter++) {
    // Transcode frame and enqueue
    a2dp_lhdcV3_encode_frames(nb_frame);
  }
}

// Obtains the number of frames to send and number of iterations
// to be used. |num_of_iterations| and |num_of_frames| parameters
// are used as output param for returning the respective values.
static void a2dp_lhdcv3_get_num_frame_iteration(uint8_t* num_of_iterations,
                                              uint8_t* num_of_frames,
                                              uint64_t timestamp_us) {
  uint32_t result = 0;
  uint8_t nof = 0;
  uint8_t noi = 1;

  *num_of_iterations = 0;
  *num_of_frames = 0;

  int32_t pcm_bytes_per_frame = lhdc_get_block_size(a2dp_lhdc_encoder_cb.lhdc_handle);
  if (pcm_bytes_per_frame <= 0) {
    log::info( ": lhdc_get_block_size error!");
    return;
  }

  pcm_bytes_per_frame = pcm_bytes_per_frame *
  a2dp_lhdc_encoder_cb.feeding_params.channel_count *
  a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample / 8;
  log::info( ": pcm_bytes_per_frame {}",  pcm_bytes_per_frame);

  int encoder_interval = a2dp_vendor_lhdcv3_get_encoder_interval_ms();
  uint32_t us_this_tick = encoder_interval * 1000;
  uint64_t now_us = timestamp_us;
  if (a2dp_lhdc_encoder_cb.lhdc_feeding_state.last_frame_us != 0)
    us_this_tick =
        (now_us - a2dp_lhdc_encoder_cb.lhdc_feeding_state.last_frame_us);
  a2dp_lhdc_encoder_cb.lhdc_feeding_state.last_frame_us = now_us;

  a2dp_lhdc_encoder_cb.lhdc_feeding_state.counter +=
      a2dp_lhdc_encoder_cb.lhdc_feeding_state.bytes_per_tick * us_this_tick /
      (encoder_interval * 1000);

  result =
      a2dp_lhdc_encoder_cb.lhdc_feeding_state.counter / pcm_bytes_per_frame;
  a2dp_lhdc_encoder_cb.lhdc_feeding_state.counter -=
      result * pcm_bytes_per_frame;
  nof = result;

  log::info( ": effective num of frames {}, iterations {}",  nof, noi);

  *num_of_frames = nof;
  *num_of_iterations = noi;
}

static BT_HDR *bt_buf_new( void) {
    BT_HDR *p_buf = ( BT_HDR*)osi_malloc( BT_DEFAULT_BUFFER_SIZE);
    if ( p_buf == NULL) {
        // LeoKu(C): should not happen
        log::error(  ": bt_buf_new failed!");
        return  NULL;
    }

    p_buf->offset = A2DP_LHDC_OFFSET;
    p_buf->len = 0;
    p_buf->layer_specific = 0;
    return  p_buf;
}

static void a2dp_lhdcV3_encode_frames(uint8_t nb_frame){
  //tA2DP_LHDC_ENCODER_PARAMS* p_encoder_params =
  //    &a2dp_lhdc_encoder_cb.lhdc_encoder_params;
  int32_t samples_per_frame = lhdc_get_block_size(a2dp_lhdc_encoder_cb.lhdc_handle);
  if (samples_per_frame <= 0) {
    log::error (": lhdc_get_block_size error!");
    return;
  }

#if defined(_RECODER_FILE_)
  uint32_t pcm_bytes_per_frame = samples_per_frame *
                                 a2dp_lhdc_encoder_cb.feeding_params.channel_count *
                                 a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample / 8;
#endif

#if (BTA_AV_CO_CP_SCMS_T == TRUE)
  uint32_t max_mtu_len = ( uint32_t)( a2dp_lhdc_encoder_cb.TxAaMtuSize - A2DP_LHDC_MPL_HDR_LEN - 1);
#else
  uint32_t max_mtu_len = ( uint32_t)( a2dp_lhdc_encoder_cb.TxAaMtuSize - A2DP_LHDC_MPL_HDR_LEN);
#endif

  static float mtu_usage = 0;
  static int mtu_usage_cnt = 0;
  static uint64_t time_prev_ms = bluetooth::common::time_get_os_boottime_ms();
  static uint32_t all_send_bytes = 0;
  uint8_t read_buffer[samples_per_frame * 2 * 4];
  uint8_t latency =0; // p_encoder_params->latency_mode_index;
  int32_t out_frames = 0, remain_nb_frame = nb_frame;
  int32_t written = 0;
  uint32_t bytes_read = 0;
  uint8_t* packet;
  BT_HDR * p_buf = NULL;

#ifdef MCPS_FRAME_ENCODE
  uint64_t time_prev_enc_us_2 = 0;
  uint64_t time_aft_enc_us_2 = 0;
#endif

  while (nb_frame) {
      if ((p_buf = bt_buf_new()) == NULL) {
          log::error ( ": ERROR");
          return;
      }
    uint32_t written_frame = 0;
    do {
      uint32_t temp_bytes_read = 0;
      if (a2dp_lhdcv3_read_feeding(read_buffer, &temp_bytes_read)) {
        a2dp_lhdc_encoder_cb.bytes_read += temp_bytes_read;
        packet = (uint8_t*)(p_buf + 1) + p_buf->offset + p_buf->len;

        //int result =

      #if defined(_RECODER_FILE_)
        if (pcmFile != NULL) {
          fwrite(read_buffer, sizeof(uint8_t), pcm_bytes_per_frame, pcmFile);
        }
      #endif

#ifdef MCPS_FRAME_ENCODE
        time_prev_enc_us_2 = bluetooth::common::time_get_os_boottime_us();
#endif

        lhdc_encode_func(a2dp_lhdc_encoder_cb.lhdc_handle, read_buffer, packet, (uint32_t*)&written, (uint32_t*)&out_frames);

#ifdef MCPS_FRAME_ENCODE
#if 0
        // For old frame per packet mechanism
        if (mcpsStat_time_dur_cnt_lhdcv3 > 0) { //begin after statistics initialized
          if (out_frames > 0) {
            time_aft_enc_us_2 = bluetooth::common::time_get_os_boottime_us();
            mcpsStat_all_enc_us_lhdcv3 += (time_aft_enc_us_2 - time_prev_enc_us_2);
            mcpsStat_all_enc_frm_lhdcv3 += out_frames;
          }
        }
#else
        // For new frame per packet mechanism
        if (mcpsStat_time_dur_cnt_lhdcv3 > 0) { //begin after statistics initialized
          time_aft_enc_us_2 = bluetooth::common::time_get_os_boottime_us();
          mcpsStat_all_enc_us_lhdcv3 += (time_aft_enc_us_2 - time_prev_enc_us_2);
          mcpsStat_all_enc_frm_lhdcv3 += 1;
        }
#endif
#endif

        #if defined(_RECODER_FILE_)
        if (RecFile != NULL && written > 0) {
            fwrite(packet, sizeof(uint8_t), written, RecFile);
        }
        #endif

        p_buf->len += written;
        all_send_bytes += written;
        nb_frame--;
        written_frame += out_frames;  // added a frame to the buffer
        log::info (": nb_frame:{}, written:{}, out_frames:{}",  nb_frame, written, out_frames);

      }else{
        log::info ( ": underflow {}",  nb_frame);
        a2dp_lhdc_encoder_cb.lhdc_feeding_state.counter +=
                        nb_frame * samples_per_frame *
                        a2dp_lhdc_encoder_cb.feeding_params.channel_count *
                        a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample / 8;

        // no more pcm to read
        nb_frame = 0;
      }
    } while ((written == 0) && nb_frame);

    if (p_buf->len) {
      /*
       * Timestamp of the media packet header represent the TS of the
       * first frame, i.e the timestamp before including this frame.
       */
      p_buf->layer_specific = a2dp_lhdc_encoder_cb.buf_seq++;
      p_buf->layer_specific <<= 8;
      p_buf->layer_specific |= ( latency | ( written_frame << A2DP_LHDC_HDR_NUM_SHIFT));

      *( ( uint32_t*)( p_buf + 1)) = a2dp_lhdc_encoder_cb.timestamp;
      log::info ( ": Timestamp ({})",  a2dp_lhdc_encoder_cb.timestamp);

      a2dp_lhdc_encoder_cb.timestamp += ( written_frame * samples_per_frame);

      uint8_t done_nb_frame = remain_nb_frame - nb_frame;
      remain_nb_frame = nb_frame;
      log::info( ": nb_frame:{}, remain_nb_frame:{}, done_nb_frame:{}",  nb_frame, remain_nb_frame, done_nb_frame);

      mtu_usage += ((float)p_buf->len) / max_mtu_len;
      mtu_usage_cnt++;

      log::info ( ": Read bytes({})",  a2dp_lhdc_encoder_cb.bytes_read);
      log::info ( ": Send Frame({}), length({})",  written_frame, p_buf->len);
      bytes_read = a2dp_lhdc_encoder_cb.bytes_read;
      a2dp_lhdc_encoder_cb.bytes_read = 0;

      if (!a2dp_lhdc_encoder_cb.enqueue_callback(p_buf, 1, bytes_read))
        return;
    } else {
      // NOTE: Unlike the execution path for other codecs, it is normal for
      // LHDC to NOT write encoded data to the last buffer if there wasn't
      // enough data to write to. That data is accumulated internally by
      // the codec and included in the next iteration. Therefore, here we
      // don't increment the "media_read_total_dropped_packets" counter.
      log::info ( ": free buffer len({})",  p_buf->len);
      osi_free(p_buf);
    }
  }

  // for statistics
  uint64_t time_now_ms = bluetooth::common::time_get_os_boottime_ms();
  if (time_now_ms - time_prev_ms >= 1000 ) {
#ifdef MCPS_FRAME_ENCODE
    mcpsStat_time_dur_cnt_lhdcv3++;

    if (mcpsStat_time_dur_cnt_lhdcv3 <= MCPS_FRAME_ENCODE_DUR) {  //how long we stop statistics
      if (mcpsStat_time_dur_cnt_lhdcv3 <= 1) {
        mcpsStat_all_time_dur_ms_lhdcv3 = 0;
      } else {
        mcpsStat_all_time_dur_ms_lhdcv3 += (time_now_ms - time_prev_ms);
      }

      /*
       * MCPS STATISTICS LOG:
       *  [cnt:{} time_now_ms({})-time_prev_ms({})=({}) {}]:
       *    cnt       : the number of rounds
       *    time_now_ms    : now system time point (ms)
       *    time_prev_ms : previous system time point (ms)
       *  [{} {} {} kbps]: sampleRate, bitDepth, Bitrate
       *  frmPerTheSec: number of frames encoded in "this second"
       *  avgFrmPerSec: so far, the average number of encoded frames per second
       *  avgUsPerfrm:  so far, the average execution time(us) of each encoded frame
       *  avgMCPS:  so far, the average MCPS
       */
      log::info( "[MCPS_STAT]: [cnt:{} time_now_ms({})-time_prev_ms({})=({}) {}] [{} {} {} kbps] "
          "frmPerTheSec({}) avgFrmPerSec({}) avgUsPerfrm:({})us, avgMCPS:({})",
          (uint32_t)mcpsStat_time_dur_cnt_lhdcv3,
          (uint32_t)time_now_ms,
          (uint32_t)time_prev_ms,
          (uint32_t)mcpsStat_all_time_dur_ms_lhdcv3,            //total elapsed time (in ms)
          (float)mcpsStat_all_time_dur_ms_lhdcv3 / 1000,        //total elapsed time (in sec)
          a2dp_lhdc_encoder_cb.feeding_params.sample_rate,      //current sample rate
          a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample,  //current bit depth
          (all_send_bytes * 8) / 1000,                          //current bit rate

          (uint32_t)(mcpsStat_all_enc_frm_lhdcv3 - mcpsStat_enc_frm_per_sec_lhdcv3),
          ((float)mcpsStat_all_enc_frm_lhdcv3 / ((float)mcpsStat_all_time_dur_ms_lhdcv3 / 1000)),
          ((float)mcpsStat_all_enc_us_lhdcv3 / (float)mcpsStat_all_enc_frm_lhdcv3),

          // avgMCPS = (avgUsPerfrm / STATIS_MILLION_UNIT) * CPU_Freq_MHz * avgFrmPerSec
          (((float)mcpsStat_all_enc_us_lhdcv3 / (float)mcpsStat_all_enc_frm_lhdcv3) * STATIS_CPU_FREQ_MHZ *
            ((float)mcpsStat_all_enc_frm_lhdcv3 / ((float)mcpsStat_all_time_dur_ms_lhdcv3 / 1000)) /
            STATIS_MILLION_UNIT)
          );
    }
    mcpsStat_enc_frm_per_sec_lhdcv3 = mcpsStat_all_enc_frm_lhdcv3;
#endif
    log::info (": Current data rate about {} kbps, packet usage {}%%", 
        (all_send_bytes * 8) / 1000, (mtu_usage * 100)/mtu_usage_cnt);
      all_send_bytes = 0;
      mtu_usage_cnt = 0;
      mtu_usage = 0;
      time_prev_ms = time_now_ms;
  }
}

static bool a2dp_lhdcv3_read_feeding(uint8_t* read_buffer, uint32_t *bytes_read) {
    uint32_t bytes_per_sample = a2dp_lhdc_encoder_cb.feeding_params.channel_count * a2dp_lhdc_encoder_cb.feeding_params.bits_per_sample / 8;
  uint32_t read_size = 0;
  int32_t read_size_tmp = lhdc_get_block_size(a2dp_lhdc_encoder_cb.lhdc_handle);
  if (read_size_tmp <= 0) {
    log::error (": lhdc_get_block_size error!");
    return false;
  }
  read_size = read_size_tmp * bytes_per_sample;

  a2dp_lhdc_encoder_cb.stats.media_read_total_expected_reads_count++;
  a2dp_lhdc_encoder_cb.stats.media_read_total_expected_read_bytes += read_size;

  /* Read Data from UIPC channel */
  uint32_t nb_byte_read =
      a2dp_lhdc_encoder_cb.read_callback(read_buffer, read_size);
  log::info( ": want to read size {}, read byte number {}",
                     read_size, nb_byte_read);
  if ((nb_byte_read % bytes_per_sample) != 0) {
	  log::info( ": PCM data not alignment. The audio sample is shfit {} bytes.", (nb_byte_read % bytes_per_sample));
  }
  a2dp_lhdc_encoder_cb.stats.media_read_total_actual_read_bytes += nb_byte_read;

  if (nb_byte_read < read_size) {
    if (nb_byte_read == 0) return false;

    /* Fill the unfilled part of the read buffer with silence (0) */
    memset(((uint8_t*)read_buffer) + nb_byte_read, 0, read_size - nb_byte_read);
    nb_byte_read = read_size;
  }
  a2dp_lhdc_encoder_cb.stats.media_read_total_actual_reads_count++;

    *bytes_read = nb_byte_read;

  return true;
}

// library index mapping: quality mode index
static std::string quality_mode_index_to_name(int quality_mode_index) {
  switch (quality_mode_index) {
    case LHDCBT_QUALITY_AUTO:
      return "ABR";
    case LHDCBT_QUALITY_HIGH:
      return "HIGH";
    case LHDCBT_QUALITY_MID:
      return "MID";
    case LHDCBT_QUALITY_LOW:
      return "LOW";
    case LHDCBT_QUALITY_LOW4:
      return "LOW_320";
    case LHDCBT_QUALITY_LOW3:
      return "LOW_256";
    case LHDCBT_QUALITY_LOW2:
      return "LOW_192";
    case LHDCBT_QUALITY_LOW1:
      return "LOW_128";
    case LHDCBT_QUALITY_LOW0:
      return "LOW_64";
    default:
      return "Unknown";
  }
}

void a2dp_vendor_lhdcv3_set_transmit_queue_length(size_t transmit_queue_length) {
  a2dp_lhdc_encoder_cb.TxQueueLength = transmit_queue_length;
  tA2DP_LHDC_ENCODER_PARAMS* p_encoder_params = &a2dp_lhdc_encoder_cb.lhdc_encoder_params;
  if (p_encoder_params->quality_mode_index == LHDCBT_QUALITY_AUTO) {
      if (lhdc_auto_adjust_bitrate != NULL) {
          lhdc_auto_adjust_bitrate(a2dp_lhdc_encoder_cb.lhdc_handle, transmit_queue_length);
      }
  }
}

#if 0
uint64_t A2dpCodecConfigLhdcV3::encoderIntervalMs() const {
  return a2dp_vendor_lhdcv3_get_encoder_interval_ms();
}
#endif

#if 0
int A2dpCodecConfigLhdcV3::getEffectiveMtu() const {
  return a2dp_lhdc_encoder_cb.TxAaMtuSize;
}
#endif

void A2dpCodecConfigLhdcV3::debug_codec_dump(int fd) {
  a2dp_lhdc_encoder_stats_t* stats = &a2dp_lhdc_encoder_cb.stats;
  tA2DP_LHDC_ENCODER_PARAMS* p_encoder_params =
      &a2dp_lhdc_encoder_cb.lhdc_encoder_params;

  A2dpCodecConfig::debug_codec_dump(fd);

  dprintf(fd,
          "  Packet counts (expected/dropped)                        : %zu / "
          "%zu\n",
          stats->media_read_total_expected_packets,
          stats->media_read_total_dropped_packets);

  dprintf(fd,
          "  PCM read counts (expected/actual)                       : %zu / "
          "%zu\n",
          stats->media_read_total_expected_reads_count,
          stats->media_read_total_actual_reads_count);

  dprintf(fd,
          "  PCM read bytes (expected/actual)                        : %zu / "
          "%zu\n",
          stats->media_read_total_expected_read_bytes,
          stats->media_read_total_actual_read_bytes);

  dprintf(
      fd, "  LHDC quality mode                                       : %s\n",
      quality_mode_index_to_name(p_encoder_params->quality_mode_index).c_str());

  dprintf(fd,
          "  LHDC transmission bitrate (Kbps)                        : %d\n",
          lhdc_get_bitrate(a2dp_lhdc_encoder_cb.lhdc_handle));

  dprintf(fd,
          "  LHDC saved transmit queue length                        : %zu\n",
          a2dp_lhdc_encoder_cb.TxQueueLength);
/*
  if (a2dp_lhdc_encoder_cb.has_lhdc_abr_handle) {
    dprintf(fd,
            "  LHDC adaptive bit rate encode quality mode index        : %d\n",
            a2dp_lhdc_encoder_cb.last_lhdc_abr_eqmid);
    dprintf(fd,
            "  LHDC adaptive bit rate adjustments                      : %zu\n",
            a2dp_lhdc_encoder_cb.lhdc_abr_adjustments);
  }
  */
}

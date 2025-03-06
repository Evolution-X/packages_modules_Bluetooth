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

/******************************************************************************
 *
 *  Utility functions to help build and parse the LHDCV5 Codec Information
 *  Element and Media Payload.
 *
 ******************************************************************************/


#define LOG_TAG "bluetooth-a2dp"

#include "a2dp_vendor_lhdcv5.h"

#include <bluetooth/log.h>
#include <string.h>

#include "a2dp_vendor.h"
#include "a2dp_vendor_lhdcv5_encoder.h"
#include "a2dp_vendor_lhdcv5_decoder.h"

#include "btif/include/btif_av_co.h"
#include "internal_include/bt_trace.h"

#include "os/log.h"
#include "osi/include/osi.h"
#include "stack/include/bt_hdr.h"


using namespace bluetooth;



// data type for the LHDC Codec Information Element
typedef struct {
  uint32_t vendorId;                                    /* Vendor ID */
  uint16_t codecId;                                     /* Codec ID */
  uint8_t sampleRate;                                   /* Sampling Frequency Type */
  uint8_t bitsPerSample;                                /* Bits Per Sample Type */
  uint8_t channelMode;                                  /* Channel Mode */
  uint8_t version;                                      /* Codec SubVersion Number */
  uint8_t frameLenType;                                 /* Frame Length Type */
  uint8_t maxTargetBitrate;                             /* Max Target Bit Rate Type */
  uint8_t minTargetBitrate;                             /* Min Target Bit Rate Type */
  bool hasFeatureAR;                                    /* FeatureSupported: AR */
  bool hasFeatureJAS;                                   /* FeatureSupported: JAS */
  bool hasFeatureMETA;                                  /* FeatureSupported: META */
  bool hasFeatureLL;                                    /* FeatureSupported: Low Latency */
  bool hasFeatureLLESS48K;                              /* FeatureSupported: Lossless enable/disable (standard 48 KHz) */
  bool hasFeatureLLESS24Bit;                            /* Lossless extended configurable: 24 bit-per-sample */
  bool hasFeatureLLESS96K;                              /* Lossless extended configurable: 96 KHz */
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  bool hasFeatureLLESSRaw;                              /* FeatureSupported: Lossless Raw mode (standard 48 KHz) */
#endif
} tA2DP_LHDCV5_CIE;

// source capabilities
static const tA2DP_LHDCV5_CIE a2dp_lhdcv5_source_caps = {
    A2DP_LHDC_VENDOR_ID,  // vendorId
    A2DP_LHDCV5_CODEC_ID, // codecId
    // Sampling Frequency
    (A2DP_LHDCV5_SAMPLING_FREQ_44100 | A2DP_LHDCV5_SAMPLING_FREQ_48000 | A2DP_LHDCV5_SAMPLING_FREQ_96000 | A2DP_LHDCV5_SAMPLING_FREQ_192000),
    // Bits Per Sample
    (A2DP_LHDCV5_BIT_FMT_16 | A2DP_LHDCV5_BIT_FMT_24),
    // Channel Mode
    A2DP_LHDCV5_CHANNEL_MODE_STEREO,
    // Codec SubVersion Number
    A2DP_LHDCV5_VER_1,
    // Encoded Frame Length
    A2DP_LHDCV5_FRAME_LEN_5MS,
    // Max Target Bit Rate Type
    A2DP_LHDCV5_MAX_BIT_RATE_1000K,
    // Min Target Bit Rate Type
    A2DP_LHDCV5_MIN_BIT_RATE_64K,
    // FeatureSupported: AR
    false,
    // FeatureSupported: JAS
    false,
    // FeatureSupported: META
    false,
    // FeatureSupported: Low Latency
    true,
    // FeatureSupported: Lossless (standard 48 KHz)
    false,
    // Lossless extended configurable: 24 bit-per-sample
    false,
    // Lossless extended configurable: 96 KHz
    false,
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    // FeatureSupported: Lossless Raw mode (standard 48 KHz)
    false,
#endif
};

// default source capabilities for best select
static const tA2DP_LHDCV5_CIE a2dp_lhdcv5_source_default_caps = {
    A2DP_LHDC_VENDOR_ID,  // vendorId
    A2DP_LHDCV5_CODEC_ID, // codecId
    // Sampling Frequency
    A2DP_LHDCV5_SAMPLING_FREQ_48000,
    // Bits Per Sample
    A2DP_LHDCV5_BIT_FMT_24,
    // Channel Mode
    A2DP_LHDCV5_CHANNEL_MODE_STEREO,
    // Codec Version Number
    A2DP_LHDCV5_VER_1,
    // Encoded Frame Length
    A2DP_LHDCV5_FRAME_LEN_5MS,
    // Max Target Bit Rate Type
    A2DP_LHDCV5_MAX_BIT_RATE_1000K,
    // Min Target Bit Rate Type
    A2DP_LHDCV5_MIN_BIT_RATE_64K,
    // FeatureSupported: AR
    false,
    // FeatureSupported: JAS
    false,
    // FeatureSupported: META
    false,
    // FeatureSupported: Low Latency
    true,
    // FeatureSupported: Lossless (standard 48 KHz)
    false,
    // Lossless extended configurable: 24 bit-per-sample
    false,
    // Lossless extended configurable: 96 KHz
    false,
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    // FeatureSupported: Lossless Raw mode (standard 48 KHz)
    false,
#endif
};

// sink capabilities
static const tA2DP_LHDCV5_CIE a2dp_lhdcv5_sink_caps = {
    A2DP_LHDC_VENDOR_ID,  // vendorId
    A2DP_LHDCV5_CODEC_ID, // codecId
    // Sampling Frequency
    (A2DP_LHDCV5_SAMPLING_FREQ_44100 | A2DP_LHDCV5_SAMPLING_FREQ_48000 | A2DP_LHDCV5_SAMPLING_FREQ_96000 | A2DP_LHDCV5_SAMPLING_FREQ_192000),
    // Bits Per Sample
    (A2DP_LHDCV5_BIT_FMT_16 | A2DP_LHDCV5_BIT_FMT_24 | A2DP_LHDCV5_BIT_FMT_32),
    // Channel Mode
    A2DP_LHDCV5_CHANNEL_MODE_STEREO,
    // Codec Version Number
    A2DP_LHDCV5_VER_1,
    // Encoded Frame Length
    A2DP_LHDCV5_FRAME_LEN_5MS,
    // Max Target Bit Rate Type
    A2DP_LHDCV5_MAX_BIT_RATE_1000K,
    // Min Target Bit Rate Type
    A2DP_LHDCV5_MIN_BIT_RATE_64K,
    // FeatureSupported: AR
    false,
    // FeatureSupported: JAS
    false,
    // FeatureSupported: META
    false,
    // FeatureSupported: Low Latency
    true,
    // FeatureSupported: Lossless (standard 48 KHz)
    false,
    // Lossless extended configurable: 24 bit-per-sample
    false,
    // Lossless extended configurable: 96 KHz
    false,
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    // FeatureSupported: Lossless Raw mode (standard 48 KHz)
    false,
#endif
};

// default sink capabilities
UNUSED_ATTR static const tA2DP_LHDCV5_CIE a2dp_lhdcv5_sink_default_caps = {
    A2DP_LHDC_VENDOR_ID,  // vendorId
    A2DP_LHDCV5_CODEC_ID, // codecId
    // Sampling Frequency
    A2DP_LHDCV5_SAMPLING_FREQ_48000,
    // Bits Per Sample
    A2DP_LHDCV5_BIT_FMT_24,
    // Channel Mode
    A2DP_LHDCV5_CHANNEL_MODE_STEREO,
    // Codec Version Number
    A2DP_LHDCV5_VER_1,
    // Encoded Frame Length
    A2DP_LHDCV5_FRAME_LEN_5MS,
    // Max Target Bit Rate Type
    A2DP_LHDCV5_MAX_BIT_RATE_1000K,
    // Min Target Bit Rate Type
    A2DP_LHDCV5_MIN_BIT_RATE_64K,
    // FeatureSupported: AR
    false,
    // FeatureSupported: JAS
    false,
    // FeatureSupported: META
    false,
    // FeatureSupported: Low Latency
    true,
    // FeatureSupported: Lossless (standard 48 KHz)
    false,
    // Lossless extended configurable: 24 bit-per-sample
    false,
    // Lossless extended configurable: 96 KHz
    false,
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    // FeatureSupported: Lossless Raw mode (standard 48 KHz)
    false,
#endif
};

//
// Utilities for LHDC configuration on A2DP specifics - START
//
typedef struct {
  btav_a2dp_codec_config_t *_codec_config_;
  btav_a2dp_codec_config_t *_codec_capability_;
  btav_a2dp_codec_config_t *_codec_local_capability_;
  btav_a2dp_codec_config_t *_codec_selectable_capability_;
  btav_a2dp_codec_config_t *_codec_user_config_;
  btav_a2dp_codec_config_t *_codec_audio_config_;
}tA2DP_CODEC_CONFIGS_PACK;

typedef struct {
  uint8_t   featureCode;  /* code of LHDC features */
  uint8_t   inSpecBank;   /* target specific to store the feature flag */
  uint8_t   bitPos;       /* the bit index(0~63) of the specific(int64_t) that bit store */
  int64_t   value;        /* real value of the bit position written to the target specific */
}tA2DP_LHDC_FEATURE_POS;

// default settings of LHDC features configuration on specifics
// info of feature: JAS
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_JAS = {
    LHDCV5_FEATURE_CODE_JAS,
    LHDCV5_FEATURE_ON_A2DP_SPECIFIC_3,
    LHDCV5_FEATURE_JAS_SPEC_BIT_POS,
    (0x1ULL << LHDCV5_FEATURE_JAS_SPEC_BIT_POS),
};

// info of feature: AR
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_AR = {
    LHDCV5_FEATURE_CODE_AR,
    LHDCV5_FEATURE_ON_A2DP_SPECIFIC_3,
    LHDCV5_FEATURE_AR_SPEC_BIT_POS,
    (0x1ULL << LHDCV5_FEATURE_AR_SPEC_BIT_POS),
};

// info of feature: META
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_META = {
    LHDCV5_FEATURE_CODE_META,
    LHDCV5_FEATURE_ON_A2DP_SPECIFIC_3,
    LHDCV5_FEATURE_META_SPEC_BIT_POS,
    (0x1ULL << LHDCV5_FEATURE_META_SPEC_BIT_POS),
};

// info of feature: Low Latency
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_LL = {
    LHDCV5_FEATURE_CODE_LL,
    LHDCV5_FEATURE_ON_A2DP_SPECIFIC_2,
    LHDCV5_FEATURE_LL_SPEC_BIT_POS,
    (0x1ULL << LHDCV5_FEATURE_LL_SPEC_BIT_POS),
};

// info of feature: LossLess
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_LLESS = {
    LHDCV5_FEATURE_CODE_LLESS,
    LHDCV5_FEATURE_ON_A2DP_SPECIFIC_3,
    LHDCV5_FEATURE_LLESS_SPEC_BIT_POS,
    (0x1ULL << LHDCV5_FEATURE_LLESS_SPEC_BIT_POS),
};

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
// info of feature: LossLess Raw
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_LLESS_RAW = {
    LHDCV5_FEATURE_CODE_LLESS_RAW,
    LHDCV5_FEATURE_ON_A2DP_SPECIFIC_3,
    LHDCV5_FEATURE_LLESS_RAW_SPEC_BIT_POS,
    (0x1ULL << LHDCV5_FEATURE_LLESS_RAW_SPEC_BIT_POS),
};
#endif


UNUSED_ATTR static const tA2DP_LHDC_FEATURE_POS a2dp_lhdcv5_source_spec_all[] = {
    a2dp_lhdcv5_source_spec_JAS,
    a2dp_lhdcv5_source_spec_AR,
    a2dp_lhdcv5_source_spec_META,
    a2dp_lhdcv5_source_spec_LL,
    a2dp_lhdcv5_source_spec_LLESS,
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    a2dp_lhdcv5_source_spec_LLESS_RAW,
#endif
};

// to check if target feature bit is set in codec_user_config_
static bool A2DP_IsFeatureInUserConfigLhdcV5(tA2DP_CODEC_CONFIGS_PACK* cfgsPtr, uint8_t featureCode) {
  bool ret = false;

  if (cfgsPtr == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  switch (featureCode) {
  case LHDCV5_FEATURE_CODE_JAS:
  {
    ret = LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_,
        a2dp_lhdcv5_source_spec_JAS.inSpecBank, a2dp_lhdcv5_source_spec_JAS.value);
    return ret;
  } break;
  case LHDCV5_FEATURE_CODE_AR:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_,
        a2dp_lhdcv5_source_spec_AR.inSpecBank, a2dp_lhdcv5_source_spec_AR.value);
  } break;
  case LHDCV5_FEATURE_CODE_META:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_,
        a2dp_lhdcv5_source_spec_META.inSpecBank, a2dp_lhdcv5_source_spec_META.value);
  } break;
  case LHDCV5_FEATURE_CODE_LL:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_,
        a2dp_lhdcv5_source_spec_LL.inSpecBank, a2dp_lhdcv5_source_spec_LL.value);
  } break;
  case LHDCV5_FEATURE_CODE_LLESS:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_,
        a2dp_lhdcv5_source_spec_LLESS.inSpecBank, a2dp_lhdcv5_source_spec_LLESS.value);
  } break;
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  case LHDCV5_FEATURE_CODE_LLESS_RAW:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_,
        a2dp_lhdcv5_source_spec_LLESS_RAW.inSpecBank, a2dp_lhdcv5_source_spec_LLESS_RAW.value);
  } break;
#endif
  default:
    break;
  }

  return false;
}

// to check if target feature bit is set in codec_config_
static bool A2DP_IsFeatureInCodecConfigLhdcV5(tA2DP_CODEC_CONFIGS_PACK* cfgsPtr, uint8_t featureCode) {
  if (cfgsPtr == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  switch(featureCode) {
  case LHDCV5_FEATURE_CODE_JAS:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_,
        a2dp_lhdcv5_source_spec_JAS.inSpecBank, a2dp_lhdcv5_source_spec_JAS.value);
  } break;
  case LHDCV5_FEATURE_CODE_AR:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_,
        a2dp_lhdcv5_source_spec_AR.inSpecBank, a2dp_lhdcv5_source_spec_AR.value);
  } break;
  case LHDCV5_FEATURE_CODE_META:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_,
        a2dp_lhdcv5_source_spec_META.inSpecBank, a2dp_lhdcv5_source_spec_META.value);
  } break;
  case LHDCV5_FEATURE_CODE_LL:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_,
        a2dp_lhdcv5_source_spec_LL.inSpecBank, a2dp_lhdcv5_source_spec_LL.value);
  } break;
  case LHDCV5_FEATURE_CODE_LLESS:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_,
        a2dp_lhdcv5_source_spec_LLESS.inSpecBank, a2dp_lhdcv5_source_spec_LLESS.value);
  } break;
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  case LHDCV5_FEATURE_CODE_LLESS_RAW:
  {
    return LHDCV5_CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_,
        a2dp_lhdcv5_source_spec_LLESS_RAW.inSpecBank, a2dp_lhdcv5_source_spec_LLESS_RAW.value);
  } break;
#endif
  default:
    break;
  }

  return false;
}

static void A2DP_UpdateFeatureToSpecLhdcV5(tA2DP_CODEC_CONFIGS_PACK* cfgsPtr,
    uint16_t toCodecCfg, bool hasFeature, uint8_t toSpec, int64_t value) {
  if (cfgsPtr == nullptr) {
    log::error( ": nullptr input");
    return;
  }

  if (toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_CONFIG_) {
    LHDC_SETUP_A2DP_SPEC(cfgsPtr->_codec_config_, toSpec, hasFeature, value);
  }
  if (toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_CAP_) {
    LHDC_SETUP_A2DP_SPEC(cfgsPtr->_codec_capability_, toSpec, hasFeature, value);
  }
  if (toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_LOCAL_CAP_) {
    LHDC_SETUP_A2DP_SPEC(cfgsPtr->_codec_local_capability_, toSpec, hasFeature, value);
  }
  if (toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_) {
    LHDC_SETUP_A2DP_SPEC(cfgsPtr->_codec_selectable_capability_, toSpec, hasFeature, value);
  }
  if (toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_USER_) {
    LHDC_SETUP_A2DP_SPEC(cfgsPtr->_codec_user_config_, toSpec, hasFeature, value);
  }
  if (toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_AUDIO_) {
    LHDC_SETUP_A2DP_SPEC(cfgsPtr->_codec_audio_config_, toSpec, hasFeature, value);
  }
}

// to update feature bit value to target codec config's specific
static void A2DP_UpdateFeatureToA2dpConfigLhdcV5(tA2DP_CODEC_CONFIGS_PACK *cfgsPtr,
    uint8_t featureCode,  uint16_t toCodecCfg, bool hasFeature) {
  if (cfgsPtr == nullptr) {
    log::error( ": nullptr input");
    return;
  }

  switch(featureCode) {
  case LHDCV5_FEATURE_CODE_JAS:
    A2DP_UpdateFeatureToSpecLhdcV5(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdcv5_source_spec_JAS.inSpecBank, a2dp_lhdcv5_source_spec_JAS.value);
    break;
  case LHDCV5_FEATURE_CODE_AR:
    A2DP_UpdateFeatureToSpecLhdcV5(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdcv5_source_spec_AR.inSpecBank, a2dp_lhdcv5_source_spec_AR.value);
    break;
  case LHDCV5_FEATURE_CODE_META:
    A2DP_UpdateFeatureToSpecLhdcV5(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdcv5_source_spec_META.inSpecBank, a2dp_lhdcv5_source_spec_META.value);
    break;
  case LHDCV5_FEATURE_CODE_LL:
    A2DP_UpdateFeatureToSpecLhdcV5(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdcv5_source_spec_LL.inSpecBank, a2dp_lhdcv5_source_spec_LL.value);
    break;
  case LHDCV5_FEATURE_CODE_LLESS:
    A2DP_UpdateFeatureToSpecLhdcV5(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdcv5_source_spec_LLESS.inSpecBank, a2dp_lhdcv5_source_spec_LLESS.value);
    break;
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  case LHDCV5_FEATURE_CODE_LLESS_RAW:
    A2DP_UpdateFeatureToSpecLhdcV5(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdcv5_source_spec_LLESS_RAW.inSpecBank, a2dp_lhdcv5_source_spec_LLESS_RAW.value);
    break;
#endif
  default:
    break;
  }
}
//
// Utilities for LHDC configuration on A2DP specifics - END

static const tA2DP_ENCODER_INTERFACE a2dp_encoder_interface_lhdcv5 = {
    a2dp_vendor_lhdcv5_encoder_init,
    a2dp_vendor_lhdcv5_encoder_cleanup,
    a2dp_vendor_lhdcv5_feeding_reset,
    a2dp_vendor_lhdcv5_feeding_flush,
    a2dp_vendor_lhdcv5_get_encoder_interval_ms,
    a2dp_vendor_lhdcv5_get_effective_frame_size,
    a2dp_vendor_lhdcv5_send_frames,
    a2dp_vendor_lhdcv5_set_transmit_queue_length,
};

static const tA2DP_DECODER_INTERFACE a2dp_decoder_interface_lhdcv5 = {
    a2dp_vendor_lhdcv5_decoder_init,
    a2dp_vendor_lhdcv5_decoder_cleanup,
    a2dp_vendor_lhdcv5_decoder_decode_packet,
    a2dp_vendor_lhdcv5_decoder_start,
    a2dp_vendor_lhdcv5_decoder_suspend,
    a2dp_vendor_lhdcv5_decoder_configure,
};

UNUSED_ATTR static tA2DP_STATUS A2DP_CodecInfoMatchesCapabilityLhdcV5(
    const tA2DP_LHDCV5_CIE* p_cap, const uint8_t* p_codec_info,
    bool is_capability);


// check if target version is supported right now
static bool is_codec_version_supported(uint8_t version, bool is_source) {
  const tA2DP_LHDCV5_CIE* p_a2dp_lhdcv5_caps =
      (is_source) ? &a2dp_lhdcv5_source_caps : &a2dp_lhdcv5_sink_caps;

  if ((version & p_a2dp_lhdcv5_caps->version) != A2DP_LHDCV5_VER_NS) {
    return true;
  }

  log::info( ": versoin unsupported! peer:{} local:{}",
       version, p_a2dp_lhdcv5_caps->version);
  return false;
}

// Builds the LHDC Media Codec Capabilities byte sequence beginning from the
// LOSC octet. |media_type| is the media type |AVDT_MEDIA_TYPE_*|.
// |p_ie| is a pointer to the LHDC Codec Information Element information.
// The result is stored in |p_result|. Returns A2DP_SUCCESS on success,
// otherwise the corresponding A2DP error status code.
static tA2DP_STATUS A2DP_BuildInfoLhdcV5(uint8_t media_type,
    const tA2DP_LHDCV5_CIE* p_ie,
    uint8_t* p_result) {

  const uint8_t* tmpInfo = p_result;
  uint8_t para = 0;

  if (p_ie == nullptr || p_result == nullptr) {
    log::error( ": nullptr input");
    return A2DP_INVALID_CODEC_PARAMETER;
  }

  *p_result++ = A2DP_LHDCV5_CODEC_LEN;  //H0
  *p_result++ = (media_type << 4);      //H1
  *p_result++ = A2DP_MEDIA_CT_NON_A2DP; //H2

  // Vendor ID(P0-P3) and Codec ID(P4-P5)
  *p_result++ = (uint8_t)(p_ie->vendorId & 0x000000FF);
  *p_result++ = (uint8_t)((p_ie->vendorId & 0x0000FF00) >> 8);
  *p_result++ = (uint8_t)((p_ie->vendorId & 0x00FF0000) >> 16);
  *p_result++ = (uint8_t)((p_ie->vendorId & 0xFF000000) >> 24);
  *p_result++ = (uint8_t)(p_ie->codecId & 0x00FF);
  *p_result++ = (uint8_t)((p_ie->codecId & 0xFF00) >> 8);

  para = 0;
  // P6[5:0] Sampling Frequency
  if ((p_ie->sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_MASK) != A2DP_LHDCV5_SAMPLING_FREQ_NS) {
    para |= (p_ie->sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_MASK);
  } else {
    log::error( ": invalid sample rate (0x{:02x})",  p_ie->sampleRate);
    return A2DP_INVALID_CODEC_PARAMETER;
  }
  // update P6
  *p_result++ = para; para = 0;

  // P7[2:0] Bit Depth
  if ((p_ie->bitsPerSample & A2DP_LHDCV5_BIT_FMT_MASK) != A2DP_LHDCV5_BIT_FMT_NS) {
    para |= (p_ie->bitsPerSample & A2DP_LHDCV5_BIT_FMT_MASK);
  } else {
    log::error( ": invalid bits per sample (0x{:02x})",  p_ie->bitsPerSample);
    return A2DP_INVALID_CODEC_PARAMETER;
  }
  // P7[5:4] Max Target Bit Rate
  para |= (p_ie->maxTargetBitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK);
  // P7[7:6] Min Target Bit Rate
  para |= (p_ie->minTargetBitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK);
  // update P7
  *p_result++ = para; para = 0;

  // P8[3:0] Codec SubVersion
  if ((p_ie->version & A2DP_LHDCV5_VERSION_MASK) != A2DP_LHDCV5_VER_NS) {
    para = para | (p_ie->version & A2DP_LHDCV5_VERSION_MASK);
  } else {
    log::error( ": invalid codec subversion (0x{:02x})",  p_ie->version);
    return A2DP_INVALID_CODEC_PARAMETER;
  }
  // P8[5:4] Frame Length Type
  if ((p_ie->frameLenType & A2DP_LHDCV5_FRAME_LEN_MASK) != A2DP_LHDCV5_FRAME_LEN_NS) {
    para = para | (p_ie->frameLenType & A2DP_LHDCV5_FRAME_LEN_MASK);
  } else {
    log::error( ": invalid frame length type (0x{:02x})",  p_ie->frameLenType);
    return A2DP_INVALID_CODEC_PARAMETER;
  }
  // update P8
  *p_result++ = para; para = 0;

  // P9[0] HasAR
  // P9[1] HasJAS
  // P9[2] HasMeta
  // P9[4] HasLossless96K
  // P9[5] HasLossless24Bit
  // P9[6] HasLL
  // P9[7] HasLossless48K
  if (p_ie->hasFeatureAR) {
    para |= A2DP_LHDCV5_FEATURE_AR;
  }
  if (p_ie->hasFeatureJAS) {
    para |= A2DP_LHDCV5_FEATURE_JAS;
  }
  if (p_ie->hasFeatureMETA) {
    para |= A2DP_LHDCV5_FEATURE_META;
  }
  if (p_ie->hasFeatureLL) {
    para |= A2DP_LHDCV5_FEATURE_LL;
  }
  if (p_ie->hasFeatureLLESS48K) {
    para |= A2DP_LHDCV5_FEATURE_LLESS48K;
  }
  if (p_ie->hasFeatureLLESS24Bit) {
    para |= A2DP_LHDCV5_FEATURE_LLESS24BIT;
  }
  if (p_ie->hasFeatureLLESS96K) {
    para |= A2DP_LHDCV5_FEATURE_LLESS96K;
  }
  // update P9
  *p_result++ = para; para = 0;

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  // P10[7] HaslosslessRaw
  if (p_ie->hasFeatureLLESSRaw) {
    para |= A2DP_LHDCV5_FEATURE_LLESS_RAW;
  }
#endif
  // update P10
  *p_result++ = para; para = 0;

  log::info( ": codec info built = H0-H2:{} {} {} P0-P3:{} "
      "{} {} {} P4-P5:{} {} P6:{} P7:{} P8:{} P9:{} P10:{}", 
      tmpInfo[0], tmpInfo[1], tmpInfo[2], tmpInfo[3], tmpInfo[4], tmpInfo[5], tmpInfo[6], tmpInfo[7],
      tmpInfo[8], tmpInfo[9], tmpInfo[10], tmpInfo[11], tmpInfo[12], tmpInfo[A2DP_LHDCV5_CODEC_LEN]);

  return A2DP_SUCCESS;
}

// Parses the LHDC Media Codec Capabilities byte sequence beginning from the
// LOSC octet. The result is stored in |p_ie|. The byte sequence to parse is
// |p_codec_info|. If |is_capability| is true, the byte sequence is
// codec capabilities, otherwise is codec configuration.
// Returns A2DP_SUCCESS on success, otherwise the corresponding A2DP error
// status code.
static tA2DP_STATUS A2DP_ParseInfoLhdcV5(tA2DP_LHDCV5_CIE* p_ie,
    const uint8_t* p_codec_info,
    bool is_capability,
    bool is_source) {
  uint8_t losc;
  uint8_t media_type;
  tA2DP_CODEC_TYPE codec_type;
  const uint8_t* tmpInfo = p_codec_info;
  const uint8_t* p_codec_Info_save = p_codec_info;

  if (p_ie == nullptr || p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  // Codec capability length
  losc = *p_codec_info++;
  if (losc != A2DP_LHDCV5_CODEC_LEN) {
    log::error( ": wrong length {}",  losc);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  media_type = (*p_codec_info++) >> 4;
  codec_type = static_cast<tA2DP_CODEC_TYPE>(*p_codec_info++);

  // Media Type and Media Codec Type
  if (media_type != AVDT_MEDIA_TYPE_AUDIO ||
      codec_type != A2DP_MEDIA_CT_NON_A2DP) {
    log::error( ": invalid media type 0x{:02x} codec_type 0x{:02x}",  media_type, codec_type);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  // Vendor ID(P0-P3) and Codec ID(P4-P5)
  p_ie->vendorId = (*p_codec_info & 0x000000FF) |
      (*(p_codec_info + 1) << 8 & 0x0000FF00) |
      (*(p_codec_info + 2) << 16 & 0x00FF0000) |
      (*(p_codec_info + 3) << 24 & 0xFF000000);
  p_codec_info += 4;
  p_ie->codecId = (*p_codec_info & 0x00FF) | (*(p_codec_info + 1) << 8 & 0xFF00);
  p_codec_info += 2;
  if (p_ie->vendorId != A2DP_LHDC_VENDOR_ID ||
      p_ie->codecId != A2DP_LHDCV5_CODEC_ID) {
    log::error( ": invalid vendorId 0x{:02x} codecId 0x{:02x}",
        p_ie->vendorId, p_ie->codecId);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  // P6[5:0] Sampling Frequency
  p_ie->sampleRate = (*p_codec_info & A2DP_LHDCV5_SAMPLING_FREQ_MASK);
  if (p_ie->sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_NS) {
    log::error( ": invalid sample rate 0x{:02x}",  p_ie->sampleRate);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }
  p_codec_info += 1;

  // P7[2:0] Bits Per Sample
  p_ie->bitsPerSample = (*p_codec_info & A2DP_LHDCV5_BIT_FMT_MASK);
  if (p_ie->bitsPerSample == A2DP_LHDCV5_BIT_FMT_NS) {
    log::error( ": invalid bit per sample 0x{:02x}",  p_ie->bitsPerSample);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }
  // P7[5:4] Max Target Bit Rate
  p_ie->maxTargetBitrate = (*p_codec_info & A2DP_LHDCV5_MAX_BIT_RATE_MASK);
  // P7[7:6] Min Target Bit Rate
  p_ie->minTargetBitrate = (*p_codec_info & A2DP_LHDCV5_MIN_BIT_RATE_MASK);
  p_codec_info += 1;

  // Channel Mode: stereo only
  p_ie->channelMode = A2DP_LHDCV5_CHANNEL_MODE_STEREO;

  // P8[3:0] Codec SubVersion
  p_ie->version = (*p_codec_info & A2DP_LHDCV5_VERSION_MASK);
  if (p_ie->version == A2DP_LHDCV5_VER_NS) {
    log::error( ": invalid version 0x{:02x}",  p_ie->version);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  } else {
    if (!is_codec_version_supported(p_ie->version, is_source)) {
      log::error( ": unsupported version 0x{:02x}",  p_ie->version);
      return AVDTP_UNSUPPORTED_CONFIGURATION;
    }
  }
  // P8[5:4] Frame Length Type
  p_ie->frameLenType = (*p_codec_info & A2DP_LHDCV5_FRAME_LEN_MASK);
  if (p_ie->frameLenType == A2DP_LHDCV5_FRAME_LEN_NS) {
    log::error( ": invalid frame length mode 0x{:02x}",  p_ie->frameLenType);
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }
  p_codec_info += 1;

  // Features:
  // P9[0] HasAR
  // P9[1] HasJAS
  // P9[2] HasMeta
  // P9[4] HasLossless96K
  // P9[5] HasLossless24bit
  // P9[6] HasLL
  // P9[7] HasLossless
  p_ie->hasFeatureAR = ((*p_codec_info & A2DP_LHDCV5_FEATURE_AR) != 0) ? true : false;
  p_ie->hasFeatureJAS = ((*p_codec_info & A2DP_LHDCV5_FEATURE_JAS) != 0) ? true : false;
  p_ie->hasFeatureMETA = ((*p_codec_info & A2DP_LHDCV5_FEATURE_META) != 0) ? true : false;
  p_ie->hasFeatureLL = ((*p_codec_info & A2DP_LHDCV5_FEATURE_LL) != 0) ? true : false;
  p_ie->hasFeatureLLESS48K = ((*p_codec_info & A2DP_LHDCV5_FEATURE_LLESS48K) != 0) ? true : false;
  p_ie->hasFeatureLLESS24Bit = ((*p_codec_info & A2DP_LHDCV5_FEATURE_LLESS24BIT) != 0) ? true : false;
  p_ie->hasFeatureLLESS96K = ((*p_codec_info & A2DP_LHDCV5_FEATURE_LLESS96K) != 0) ? true : false;
  p_codec_info += 1;

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  // P10[7] HasLosslessRaw
  p_ie->hasFeatureLLESSRaw = ((*p_codec_info & A2DP_LHDCV5_FEATURE_LLESS_RAW) != 0) ? true : false;
#endif

  log::info( ": codec info parsed = H0-H2:{} {} {} P0-P3:{} "
      "{} {} {} P4-P5:{} {} P6:{} P7:{} P8:{} P9:{} P10:{}", 
      tmpInfo[0], tmpInfo[1], tmpInfo[2], tmpInfo[3], tmpInfo[4], tmpInfo[5], tmpInfo[6], tmpInfo[7],
      tmpInfo[8], tmpInfo[9], tmpInfo[10], tmpInfo[11], tmpInfo[12], tmpInfo[A2DP_LHDCV5_CODEC_LEN]);

  log::info( ":  isCap{} SR{} BPS{} Ver{} FL{} "
      "MBR{} mBR{} FeatureAR({}) JAS({}) META({}) LL({}) LLESS({}) LLESS24({}) LLESS96K({}) LLESSRaw({})",
      
      (is_source?"SRC":"SNK"),
      is_capability,
      p_ie->sampleRate,
      p_ie->bitsPerSample,
      p_ie->version,
      p_ie->frameLenType,
      p_ie->maxTargetBitrate,
      p_ie->minTargetBitrate,
      p_ie->hasFeatureAR,
      p_ie->hasFeatureJAS,
      p_ie->hasFeatureMETA,
      p_ie->hasFeatureLL,
      p_ie->hasFeatureLLESS48K,
      p_ie->hasFeatureLLESS24Bit,
      p_ie->hasFeatureLLESS96K,
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
      p_ie->hasFeatureLLESSRaw);
#else
      -1);
#endif

  //save decoder needed parameters
#if 1
  if (!is_source) {
    if (!a2dp_lhdcv5_decoder_save_codec_info(p_codec_Info_save)) {
      log::info( ": save decoder parameters error");
    }
  }
#endif

  return A2DP_SUCCESS;
}

bool A2DP_IsVendorSourceCodecValidLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE cfg_cie;
  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, false, IS_SRC) == A2DP_SUCCESS) ||
      (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, true, IS_SRC) == A2DP_SUCCESS);
}
bool A2DP_IsVendorSinkCodecValidLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE cfg_cie;
  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, false, IS_SNK) == A2DP_SUCCESS) ||
      (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, true, IS_SNK) == A2DP_SUCCESS);
}

bool A2DP_IsVendorPeerSinkCodecValidLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE cfg_cie;
  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, false, IS_SRC) == A2DP_SUCCESS) ||
      (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, true, IS_SRC) == A2DP_SUCCESS);
}
bool A2DP_IsVendorPeerSourceCodecValidLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE cfg_cie;
  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, false, IS_SNK) == A2DP_SUCCESS) ||
      (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, true, IS_SNK) == A2DP_SUCCESS);
}

// NOTE: Should be done only for local Sink codec
tA2DP_STATUS A2DP_IsVendorSinkCodecSupportedLhdcV5(const uint8_t* p_codec_info) {
  return A2DP_CodecInfoMatchesCapabilityLhdcV5(&a2dp_lhdcv5_sink_caps, p_codec_info, false);
}
// NOTE: Should be done only for local Sink codec
bool A2DP_IsPeerSourceCodecSupportedLhdcV5(const uint8_t* p_codec_info) {
  return (A2DP_CodecInfoMatchesCapabilityLhdcV5(&a2dp_lhdcv5_sink_caps, p_codec_info,
      true) == A2DP_SUCCESS);
}

// Checks whether A2DP LHDC codec configuration matches with a device's codec
// capabilities.
//  |p_cap| is the LHDC local codec capabilities.
//  |p_codec_info| is peer's codec capabilities acting as an A2DP source.
// If |is_capability| is true, the byte sequence is codec capabilities,
// otherwise is codec configuration.
// Returns A2DP_SUCCESS if the codec configuration matches with capabilities,
// otherwise the corresponding A2DP error status code.
static tA2DP_STATUS A2DP_CodecInfoMatchesCapabilityLhdcV5(
    const tA2DP_LHDCV5_CIE* p_cap, const uint8_t* p_codec_info, bool is_capability) {
  tA2DP_STATUS status;
  tA2DP_LHDCV5_CIE cfg_cie;

  if (p_cap == nullptr || p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return A2DP_INVALID_CODEC_PARAMETER;
  }

  // parse configuration
  status = A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, is_capability, IS_SNK);
  if (status != A2DP_SUCCESS) {
    log::error( ": parsing failed {}",  status);
    return status;
  }

  // verify that each parameter is in range
  log::info( ": FREQ peer: 0x{:02x}, capability 0x{:02x}",
      cfg_cie.sampleRate, p_cap->sampleRate);

  log::info( ": BIT_FMT peer: 0x{:02x}, capability 0x{:02x}",
      cfg_cie.bitsPerSample, p_cap->bitsPerSample);

  // sampling frequency
  if ((cfg_cie.sampleRate & p_cap->sampleRate) == 0) return A2DP_NOT_SUPPORTED_SAMPLING_FREQUENCY;

  // bits per sample
  if ((cfg_cie.bitsPerSample & p_cap->bitsPerSample) == 0) return A2DP_NOT_SUPPORTED_BIT_RATE;

  return A2DP_SUCCESS;
}

bool A2DP_VendorUsesRtpHeaderLhdcV5(UNUSED_ATTR bool content_protection_enabled,
    UNUSED_ATTR const uint8_t* p_codec_info) {
  // TODO: Is this correct? The RTP header is always included?
  return true;
}

const char* A2DP_VendorCodecNameLhdcV5(UNUSED_ATTR const uint8_t* p_codec_info) {
  return "LHDC V5";
}

bool A2DP_VendorCodecTypeEqualsLhdcV5(const uint8_t* p_codec_info_a,
    const uint8_t* p_codec_info_b) {
  tA2DP_LHDCV5_CIE lhdc_cie_a;
  tA2DP_LHDCV5_CIE lhdc_cie_b;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info_a == nullptr || p_codec_info_b == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status =
      A2DP_ParseInfoLhdcV5(&lhdc_cie_a, p_codec_info_a, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie_b, p_codec_info_b, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }

  return true;
}

bool A2DP_VendorCodecEqualsLhdcV5(const uint8_t* p_codec_info_a,
    const uint8_t* p_codec_info_b) {
  tA2DP_LHDCV5_CIE lhdc_cie_a;
  tA2DP_LHDCV5_CIE lhdc_cie_b;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info_a == nullptr || p_codec_info_b == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status =
      A2DP_ParseInfoLhdcV5(&lhdc_cie_a, p_codec_info_a, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information of a: {}", 
        a2dp_status);
    return false;
  }
  a2dp_status =
      A2DP_ParseInfoLhdcV5(&lhdc_cie_b, p_codec_info_b, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information of b: {}", 
        a2dp_status);
    return false;
  }

  // exam items that require to update codec config with peer if different
  return (lhdc_cie_a.sampleRate == lhdc_cie_b.sampleRate) &&
      (lhdc_cie_a.bitsPerSample == lhdc_cie_b.bitsPerSample) &&
      (lhdc_cie_a.channelMode == lhdc_cie_b.channelMode) &&
      (lhdc_cie_a.frameLenType == lhdc_cie_b.frameLenType) &&
      (lhdc_cie_a.hasFeatureLL == lhdc_cie_b.hasFeatureLL) &&
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
      (lhdc_cie_a.hasFeatureLLESS48K == lhdc_cie_b.hasFeatureLLESS48K &&
      (lhdc_cie_a.hasFeatureLLESSRaw == lhdc_cie_b.hasFeatureLLESSRaw));
#else
      (lhdc_cie_a.hasFeatureLLESS48K == lhdc_cie_b.hasFeatureLLESS48K);
#endif
}

int A2DP_VendorGetBitRateLhdcV5(UNUSED_ATTR const uint8_t* p_codec_info) {

  A2dpCodecConfig* current_codec = bta_av_get_a2dp_current_codec();
  btav_a2dp_codec_config_t codec_config_ = current_codec->getCodecConfig();
  uint8_t bitRateIndex = 0;

  if ((codec_config_.codec_specific_1 & A2DP_LHDC_VENDOR_CMD_MASK) ==
      A2DP_LHDC_QUALITY_MAGIC_NUM) {
    bitRateIndex = codec_config_.codec_specific_1 & A2DP_LHDC_QUALITY_MASK;
    switch (bitRateIndex) {
      case A2DP_LHDC_QUALITY_LOW0:
        return 64000;
      case A2DP_LHDC_QUALITY_LOW1:
        return 160000;
      case A2DP_LHDC_QUALITY_LOW2:
        return 192000;
      case A2DP_LHDC_QUALITY_LOW3:
        return 256000;
      case A2DP_LHDC_QUALITY_LOW4:
        return 320000;
      case A2DP_LHDC_QUALITY_LOW:
        return 400000;
      case A2DP_LHDC_QUALITY_MID:
        return 500000;
      case A2DP_LHDC_QUALITY_HIGH:
        return 900000;
      case A2DP_LHDC_QUALITY_HIGH1:
        return 1000000;
      case A2DP_LHDC_QUALITY_ABR:
        return 9999999;
      default:
        log::info(": non-supported bitrate index ({})",  bitRateIndex);
        return -1;
    }
  }
  return 400000;
}

int A2DP_VendorGetTrackSampleRateLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return -1;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return -1;
  }

  switch (lhdc_cie.sampleRate) {
  case A2DP_LHDCV5_SAMPLING_FREQ_44100:
    return 44100;
  case A2DP_LHDCV5_SAMPLING_FREQ_48000:
    return 48000;
  case A2DP_LHDCV5_SAMPLING_FREQ_96000:
    return 96000;
  case A2DP_LHDCV5_SAMPLING_FREQ_192000:
    return 192000;
  }

  return -1;
}

int A2DP_VendorGetTrackBitsPerSampleLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return -1;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return -1;
  }

  switch (lhdc_cie.bitsPerSample) {
  case A2DP_LHDCV5_BIT_FMT_16:
    return 16;
  case A2DP_LHDCV5_BIT_FMT_24:
    return 24;
  case A2DP_LHDCV5_BIT_FMT_32:
    return 32;
  }

  return -1;
}

int A2DP_VendorGetTrackChannelCountLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return -1;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return -1;
  }

  switch (lhdc_cie.channelMode) {
  case A2DP_LHDCV5_CHANNEL_MODE_MONO:
    return 1;
  case A2DP_LHDCV5_CHANNEL_MODE_DUAL:
    return 2;
  case A2DP_LHDCV5_CHANNEL_MODE_STEREO:
    return 2;
  }

  return -1;
}

int A2DP_VendorGetSinkTrackChannelTypeLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return -1;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SNK);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return -1;
  }

  switch (lhdc_cie.channelMode) {
  case A2DP_LHDCV5_CHANNEL_MODE_MONO:
    return 1;
  case A2DP_LHDCV5_CHANNEL_MODE_DUAL:
    return 3;
  case A2DP_LHDCV5_CHANNEL_MODE_STEREO:
    return 3;
  }

  return -1;
}

int A2DP_VendorGetChannelModeCodeLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return -1;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return -1;
  }

  switch (lhdc_cie.channelMode) {
  case A2DP_LHDCV5_CHANNEL_MODE_MONO:
  case A2DP_LHDCV5_CHANNEL_MODE_DUAL:
  case A2DP_LHDCV5_CHANNEL_MODE_STEREO:
    return lhdc_cie.channelMode;
  default:
    break;
  }

  return -1;
}

bool A2DP_VendorGetPacketTimestampLhdcV5(UNUSED_ATTR const uint8_t* p_codec_info,
    const uint8_t* p_data,
    uint32_t* p_timestamp) {
  if (p_codec_info == nullptr || p_data == nullptr || p_timestamp == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // TODO: Is this function really codec-specific?
  *p_timestamp = *(const uint32_t*)p_data;
  return true;
}

bool A2DP_VendorBuildCodecHeaderLhdcV5(UNUSED_ATTR const uint8_t* p_codec_info,
    BT_HDR* p_buf,
    uint16_t frames_per_packet) {
  uint8_t* p;

  if (p_codec_info == nullptr || p_buf == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  p_buf->offset -= A2DP_LHDC_MPL_HDR_LEN;
  p = (uint8_t*)(p_buf + 1) + p_buf->offset;
  p_buf->len += A2DP_LHDC_MPL_HDR_LEN;

  // Not support fragmentation
  p[0] = ( uint8_t)( frames_per_packet & 0xff);
  p[1] = ( uint8_t)( ( frames_per_packet >> 8) & 0xff);

  return true;
}

void A2DP_VendorDumpCodecInfoLhdcV5(const uint8_t* p_codec_info) {
  tA2DP_STATUS a2dp_status;
  tA2DP_LHDCV5_CIE lhdc_cie;

  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": A2DP_ParseInfoLhdcV5 fail:{}",  a2dp_status);
    return;
  }

  log::info( "\tsamp_freq: 0x{:02x} ", lhdc_cie.sampleRate);
  if (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100) {
    log::info( "\tsamp_freq: (44100)");
  }
  if (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000) {
    log::info( "\tsamp_freq: (48000)");
  }
  if (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000) {
    log::info( "\tsamp_freq: (96000)");
  }
  if (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000) {
    log::info( "\tsamp_freq: (19200)");
  }

  log::info( "\tbitsPerSample: 0x{} ", lhdc_cie.bitsPerSample);
  if (lhdc_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_16) {
    log::info( "\tbit_depth: (16)");
  }
  if (lhdc_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_24) {
    log::info( "\tbit_depth: (24)");
  }
  if (lhdc_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_32) {
    log::info( "\tbit_depth: (32)");
  }

  log::info( "\tchannelMode: 0x{:02x} ", lhdc_cie.channelMode);
  if (lhdc_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_MONO) {
    log::info( "\tchannle_mode: (mono)");
  }
  if (lhdc_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_DUAL) {
    log::info( "\tchannle_mode: (dual)");
  }
  if (lhdc_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_STEREO) {
    log::info( "\tchannle_mode: (stereo)");
  }
}

std::string A2DP_VendorCodecInfoStringLhdcV5(const uint8_t* p_codec_info) {
  std::stringstream res;
  std::string field;
  tA2DP_STATUS a2dp_status;
  tA2DP_LHDCV5_CIE lhdc_cie;

  if (p_codec_info == nullptr) {
    res << "A2DP_VendorCodecInfoStringLhdcV5 nullptr";
    return res.str();
  }

  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    res << "A2DP_ParseInfoLhdcV5 fail: " << loghex(static_cast<uint8_t>(a2dp_status));
    return res.str();
  }

  res << "\tname: LHDC V5\n";

  // Sample frequency
  field.clear();
  AppendField(&field, (lhdc_cie.sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_NS), "NONE");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100),
      "44100");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000),
      "48000");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000),
      "96000");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000),
      "192000");
  res << "\tsamp_freq: " << field << " (" << loghex(lhdc_cie.sampleRate)
                                                                      << ")\n";

  // bits per sample
  field.clear();
  AppendField(&field, (lhdc_cie.bitsPerSample == A2DP_LHDCV5_BIT_FMT_NS), "NONE");
  AppendField(&field, (lhdc_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_16),
      "16");
  AppendField(&field, (lhdc_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_24),
      "24");
  AppendField(&field, (lhdc_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_32),
      "24");
  res << "\tbits_depth: " << field << " bits (" << loghex((int)lhdc_cie.bitsPerSample)
                                                                      << ")\n";

  // Channel mode
  field.clear();
  AppendField(&field, (lhdc_cie.channelMode == A2DP_LHDCV5_CHANNEL_MODE_NS), "NONE");
  AppendField(&field, (lhdc_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_MONO),
      "Mono");
  AppendField(&field, (lhdc_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_DUAL),
      "Dual");
  AppendField(&field, (lhdc_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_STEREO),
      "Stereo");
  res << "\tch_mode: " << field << " (" << loghex(lhdc_cie.channelMode)
                                                                      << ")\n";

  // Version
  field.clear();
  AppendField(&field, (lhdc_cie.version == A2DP_LHDCV5_VER_NS), "NONE");
  AppendField(&field, (lhdc_cie.version == A2DP_LHDCV5_VER_1),
      "LHDC V5 Ver1");
  res << "\tversion: " << field << " (" << loghex(lhdc_cie.version)
                                                                      << ")\n";

  // Max target bit rate...
  field.clear();
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK) == A2DP_LHDCV5_MAX_BIT_RATE_1000K),
      "1000Kbps");
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK) == A2DP_LHDCV5_MAX_BIT_RATE_900K),
      "900Kbps");
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK) == A2DP_LHDCV5_MAX_BIT_RATE_500K),
      "500Kbps");
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK) == A2DP_LHDCV5_MAX_BIT_RATE_400K),
      "400Kbps");
  res << "\tMax target-rate: " << field << " (" << loghex((lhdc_cie.maxTargetBitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK))
                                                                      << ")\n";

  // Min target bit rate...
  field.clear();
  AppendField(&field, ((lhdc_cie.minTargetBitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK) == A2DP_LHDCV5_MIN_BIT_RATE_400K),
      "400Kbps");
  AppendField(&field, ((lhdc_cie.minTargetBitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK) == A2DP_LHDCV5_MIN_BIT_RATE_256K),
      "256Kbps");
  AppendField(&field, ((lhdc_cie.minTargetBitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK) == A2DP_LHDCV5_MIN_BIT_RATE_160K),
      "160Kbps");
  AppendField(&field, ((lhdc_cie.minTargetBitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK) == A2DP_LHDCV5_MIN_BIT_RATE_64K),
      "64Kbps");
  res << "\tMin target-rate: " << field << " (" << loghex((lhdc_cie.minTargetBitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK))
                                                                      << ")\n";

  return res.str();
}

const tA2DP_ENCODER_INTERFACE* A2DP_VendorGetEncoderInterfaceLhdcV5(
    const uint8_t* p_codec_info) {
  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return NULL;
  }

  if (!A2DP_IsVendorSourceCodecValidLhdcV5(p_codec_info)) return NULL;

  return &a2dp_encoder_interface_lhdcv5;
}

const tA2DP_DECODER_INTERFACE* A2DP_VendorGetDecoderInterfaceLhdcV5(
    const uint8_t* p_codec_info) {
  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return NULL;
  }

  if (!A2DP_IsVendorSinkCodecValidLhdcV5(p_codec_info)) return NULL;

  return &a2dp_decoder_interface_lhdcv5;
}

bool A2DP_VendorAdjustCodecLhdcV5(uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE cfg_cie;
  if (p_codec_info == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Nothing to do: just verify the codec info is valid
  if (A2DP_ParseInfoLhdcV5(&cfg_cie, p_codec_info, true, IS_SRC) != A2DP_SUCCESS)
    return false;

  return true;
}

btav_a2dp_codec_index_t A2DP_VendorSourceCodecIndexLhdcV5(
    UNUSED_ATTR const uint8_t* p_codec_info) {
  return BTAV_A2DP_CODEC_INDEX_SOURCE_LHDCV5;
}

btav_a2dp_codec_index_t A2DP_VendorSinkCodecIndexLhdcV5(
    UNUSED_ATTR const uint8_t* p_codec_info) {
  return BTAV_A2DP_CODEC_INDEX_SINK_LHDCV5;
}

const char* A2DP_VendorCodecIndexStrLhdcV5(void) { return "LHDC V5"; }

const char* A2DP_VendorCodecIndexStrLhdcV5Sink(void) { return "LHDC V5 SINK"; }

bool A2DP_VendorInitCodecConfigLhdcV5(AvdtpSepConfig* p_cfg) {
  if (p_cfg == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  if (A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &a2dp_lhdcv5_source_caps,
      p_cfg->codec_info) != A2DP_SUCCESS) {
    return false;
  }

#if (BTA_AV_CO_CP_SCMS_T == TRUE)
  /* Content protection info - support SCMS-T */
  uint8_t* p = p_cfg->protect_info;
  *p++ = AVDT_CP_LOSC;
  UINT16_TO_STREAM(p, AVDT_CP_SCMS_T_ID);
  p_cfg->num_protect = 1;
#endif

  return true;
}

bool A2DP_VendorInitCodecConfigLhdcV5Sink(AvdtpSepConfig* p_cfg) {
  if (p_cfg == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  if (A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &a2dp_lhdcv5_sink_caps,
      p_cfg->codec_info) != A2DP_SUCCESS) {
    return false;
  }

  return true;
}

UNUSED_ATTR static void build_codec_config(const tA2DP_LHDCV5_CIE& config_cie,
    btav_a2dp_codec_config_t* result) {
  if (result == nullptr) {
    log::error( ": nullptr input");
    return;
  }

  // sample rate
  result->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
  if (config_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  if (config_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  if (config_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
  if (config_cie.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_192000;

  // bits per sample
  result->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
  if (config_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_16)
    result->bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
  if (config_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_24)
    result->bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
  if (config_cie.bitsPerSample & A2DP_LHDCV5_BIT_FMT_32)
    result->bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;

  // channel mode
  result->channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
  if (config_cie.channelMode & A2DP_LHDCV5_CHANNEL_MODE_MONO)
    result->channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
  if (config_cie.channelMode &
      (A2DP_LHDCV5_CHANNEL_MODE_DUAL | A2DP_LHDCV5_CHANNEL_MODE_STEREO)) {
    result->channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }
}

A2dpCodecConfigLhdcV5Source::A2dpCodecConfigLhdcV5Source(
    btav_a2dp_codec_priority_t codec_priority)
: A2dpCodecConfigLhdcV5Base(BTAV_A2DP_CODEC_INDEX_SOURCE_LHDCV5,
    A2DP_VendorCodecIndexStrLhdcV5(),
    codec_priority, true) {

  // Compute the local capability
  codec_local_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
  if (a2dp_lhdcv5_source_caps.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  }
  if (a2dp_lhdcv5_source_caps.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  }
  if (a2dp_lhdcv5_source_caps.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
  }
  if (a2dp_lhdcv5_source_caps.sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000) {
    codec_local_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_192000;
  }

  codec_local_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
  if (a2dp_lhdcv5_source_caps.bitsPerSample & A2DP_LHDCV5_BIT_FMT_16) {
    codec_local_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
  }
  if (a2dp_lhdcv5_source_caps.bitsPerSample & A2DP_LHDCV5_BIT_FMT_24) {
    codec_local_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
  }
  if (a2dp_lhdcv5_source_caps.bitsPerSample & A2DP_LHDCV5_BIT_FMT_32) {
    codec_local_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;
  }

  codec_local_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
  if (a2dp_lhdcv5_source_caps.channelMode & A2DP_LHDCV5_CHANNEL_MODE_MONO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_MONO;
  }
  if (a2dp_lhdcv5_source_caps.channelMode & A2DP_LHDCV5_CHANNEL_MODE_DUAL) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }
  if (a2dp_lhdcv5_source_caps.channelMode & A2DP_LHDCV5_CHANNEL_MODE_STEREO) {
    codec_local_capability_.channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  }
}

A2dpCodecConfigLhdcV5Source::~A2dpCodecConfigLhdcV5Source() {}

bool A2dpCodecConfigLhdcV5Source::init() {
  // Load the encoder
  if (!A2DP_VendorLoadEncoderLhdcV5()) {
    log::error( ": cannot load the encoder");
    return false;
  }

  return true;
}

bool A2dpCodecConfigLhdcV5Source::useRtpHeaderMarkerBit() const { return false; }

//
// Selects the best sample rate from |sampleRate|.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_sample_rate(uint8_t sampleRate,
    tA2DP_LHDCV5_CIE* p_result,
    btav_a2dp_codec_config_t* p_codec_config) {
  if (p_codec_config == nullptr || p_result == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // LHDC V5 priority: 48K > 44.1K > 96K > 192K > others(min to max)
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000) {
    p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    return true;
  }
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100) {
    p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_44100;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    return true;
  }
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000) {
    p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_96000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    return true;
  }
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000) {
    p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_192000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_192000;
    return true;
  }
  return false;
}

//
// Selects the audio sample rate from |p_codec_audio_config|.
// |sampleRate| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_audio_sample_rate(
    const btav_a2dp_codec_config_t* p_codec_audio_config, uint8_t sampleRate,
    tA2DP_LHDCV5_CIE* p_result, btav_a2dp_codec_config_t* p_codec_config) {
  if (p_codec_audio_config == nullptr || p_result == nullptr || p_codec_config == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // LHDC V5 priority: 48K > 44.1K > 96K > 192K > others(min to max)
  switch (p_codec_audio_config->sample_rate) {
  case BTAV_A2DP_CODEC_SAMPLE_RATE_48000:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000) {
      p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
      p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_44100:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100) {
      p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_44100;
      p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_96000:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000) {
      p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_96000;
      p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_192000:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000) {
      p_result->sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_192000;
      p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_192000;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_16000:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_24000:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_176400:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_88200:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_NONE:
    break;
  }
  return false;
}

//
// Selects the best bits per sample from |bitsPerSample|.
// |bitsPerSample| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_bits_per_sample(
    uint8_t bitsPerSample, tA2DP_LHDCV5_CIE* p_result,
    btav_a2dp_codec_config_t* p_codec_config) {

  if (p_result == nullptr || p_codec_config == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // LHDC V5 priority: 24 > 16 > 32
  if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_24) {
    p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
    p_result->bitsPerSample = A2DP_LHDCV5_BIT_FMT_24;
    return true;
  }
  if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_16) {
    p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
    p_result->bitsPerSample = A2DP_LHDCV5_BIT_FMT_16;
    return true;
  }
  if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_32) {
    p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;
    p_result->bitsPerSample = A2DP_LHDCV5_BIT_FMT_32;
    return true;
  }
  return false;
}

//
// Selects the audio bits per sample from |p_codec_audio_config|.
// |bitsPerSample| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_audio_bits_per_sample(
    const btav_a2dp_codec_config_t* p_codec_audio_config,
    uint8_t bitsPerSample, tA2DP_LHDCV5_CIE* p_result,
    btav_a2dp_codec_config_t* p_codec_config) {

  if (p_codec_audio_config == nullptr || p_result == nullptr || p_codec_config == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // LHDC V5 priority: 24 > 16 > 32
  switch (p_codec_audio_config->bits_per_sample) {
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_24) {
      p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
      p_result->bitsPerSample = A2DP_LHDCV5_BIT_FMT_24;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_16) {
      p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      p_result->bitsPerSample = A2DP_LHDCV5_BIT_FMT_16;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32:
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_32) {
      p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;
      p_result->bitsPerSample = A2DP_LHDCV5_BIT_FMT_32;
      return true;
    }
    break;
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE:
    break;
  }
  return false;
}

static bool A2DP_MaxBitRatetoQualityLevelLhdcV5(uint8_t *mode, uint8_t bitrate) {
  if (mode == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  switch (bitrate & A2DP_LHDCV5_MAX_BIT_RATE_MASK) {
  case A2DP_LHDCV5_MAX_BIT_RATE_1000K:
    *mode = A2DP_LHDC_QUALITY_HIGH1;
    return true;
  case A2DP_LHDCV5_MAX_BIT_RATE_900K:
    *mode = A2DP_LHDC_QUALITY_HIGH;
    return true;
  case A2DP_LHDCV5_MAX_BIT_RATE_500K:
    *mode = A2DP_LHDC_QUALITY_MID;
    return true;
  case A2DP_LHDCV5_MAX_BIT_RATE_400K:
    *mode = A2DP_LHDC_QUALITY_LOW;
    return true;
  }
  return false;
}

static bool A2DP_MinBitRatetoQualityLevelLhdcV5(uint8_t *mode, uint8_t bitrate) {
  if (mode == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  switch (bitrate & A2DP_LHDCV5_MIN_BIT_RATE_MASK) {
  case A2DP_LHDCV5_MIN_BIT_RATE_400K:
    *mode = A2DP_LHDC_QUALITY_LOW;
    return true;
  case A2DP_LHDCV5_MIN_BIT_RATE_256K:
    *mode = A2DP_LHDC_QUALITY_LOW3;
    return true;
  case A2DP_LHDCV5_MIN_BIT_RATE_160K:
    *mode = A2DP_LHDC_QUALITY_LOW1;
    return true;
  case A2DP_LHDCV5_MIN_BIT_RATE_64K:
    *mode = A2DP_LHDC_QUALITY_LOW0;
    return true;
  }
  return false;
}

static std::string lhdcV5_sampleRate_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDCV5_SAMPLING_FREQ_44100:
    return "44.1 KHz";
  case A2DP_LHDCV5_SAMPLING_FREQ_48000:
    return "48 KHz";
  case A2DP_LHDCV5_SAMPLING_FREQ_96000:
    return "96 KHz";
  case A2DP_LHDCV5_SAMPLING_FREQ_192000:
    return "192 KHz";
  default:
    return "Unknown Sample Rate";
  }
}

static std::string lhdcV5_bitPerSample_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDCV5_BIT_FMT_16:
    return "16";
  case A2DP_LHDCV5_BIT_FMT_24:
    return "24";
  case A2DP_LHDCV5_BIT_FMT_32:
    return "32";
  default:
    return "Unknown Bit Per Sample";
  }
}

static std::string lhdcV5_frameLenType_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDCV5_FRAME_LEN_5MS:
    return "5ms";
  default:
    return "Unknown frame length type";
  }
}

static std::string lhdcV5_MaxTargetBitRate_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDCV5_MAX_BIT_RATE_900K:
    return "900Kbps";
  case A2DP_LHDCV5_MAX_BIT_RATE_500K:
    return "500Kbps";
  case A2DP_LHDCV5_MAX_BIT_RATE_400K:
    return "400Kbps";
  case A2DP_LHDCV5_MAX_BIT_RATE_1000K:
    return "1000Kbps";
  default:
    return "Unknown Max Bit Rate";
  }
}

static std::string lhdcV5_MinTargetBitRate_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDCV5_MIN_BIT_RATE_400K:
    return "400Kbps";
  case A2DP_LHDCV5_MIN_BIT_RATE_256K:
    return "256Kbps";
  case A2DP_LHDCV5_MIN_BIT_RATE_160K:
    return "160Kbps";
  case A2DP_LHDCV5_MIN_BIT_RATE_64K:
    return "64Kbps";
  default:
    return "Unknown Min Bit Rate";
  }
}

static std::string lhdcV5_QualityModeBitRate_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDC_QUALITY_ABR:
    return "ABR";
  case A2DP_LHDC_QUALITY_HIGH1:
    return "HIGH 1 (1000 Kbps)";
  case A2DP_LHDC_QUALITY_HIGH:
    return "HIGH (900 Kbps)";
  case A2DP_LHDC_QUALITY_MID:
    return "MID (500 Kbps)";
  case A2DP_LHDC_QUALITY_LOW:
    return "LOW (400 Kbps)";
  case A2DP_LHDC_QUALITY_LOW4:
    return "LOW 4 (320 Kbps)";
  case A2DP_LHDC_QUALITY_LOW3:
    return "LOW 3 (256 Kbps)";
  case A2DP_LHDC_QUALITY_LOW2:
    return "LOW 2 (192 Kbps)";
  case A2DP_LHDC_QUALITY_LOW1:
    return "LOW 1 (160 Kbps)";
  case A2DP_LHDC_QUALITY_LOW0:
    return "LOW 0 (64 Kbps)";
  default:
    return "Unknown Bit Rate Mode";
  }
}


tA2DP_STATUS A2dpCodecConfigLhdcV5Base::setCodecConfig(const uint8_t* p_peer_codec_info,
    bool is_capability,
    uint8_t* p_result_codec_config) {
  std::lock_guard<std::recursive_mutex> lock(codec_mutex_);
  tA2DP_LHDCV5_CIE sink_info_cie;
  tA2DP_LHDCV5_CIE result_config_cie;
  uint8_t sampleRate = 0;
  uint8_t bitsPerSample = 0;
  bool hasFeature = false;
  bool hasUserSet = false;
  uint8_t qualityMode = 0;
  uint8_t bitRateQmode = 0;
  tA2DP_STATUS status;

  const tA2DP_LHDCV5_CIE* p_a2dp_lhdcv5_caps =
      (is_source_) ? &a2dp_lhdcv5_source_caps : &a2dp_lhdcv5_sink_caps;

  // Save the internal state
  btav_a2dp_codec_config_t saved_codec_config = codec_config_;
  btav_a2dp_codec_config_t saved_codec_capability = codec_capability_;
  btav_a2dp_codec_config_t saved_codec_selectable_capability =
      codec_selectable_capability_;
  btav_a2dp_codec_config_t saved_codec_user_config = codec_user_config_;
  btav_a2dp_codec_config_t saved_codec_audio_config = codec_audio_config_;
  uint8_t saved_ota_codec_config[AVDT_CODEC_SIZE];
  uint8_t saved_ota_codec_peer_capability[AVDT_CODEC_SIZE];
  uint8_t saved_ota_codec_peer_config[AVDT_CODEC_SIZE];
  memcpy(saved_ota_codec_config, ota_codec_config_, sizeof(ota_codec_config_));
  memcpy(saved_ota_codec_peer_capability, ota_codec_peer_capability_,
      sizeof(ota_codec_peer_capability_));
  memcpy(saved_ota_codec_peer_config, ota_codec_peer_config_,
      sizeof(ota_codec_peer_config_));

  tA2DP_CODEC_CONFIGS_PACK allCfgPack;
  allCfgPack._codec_config_ = &codec_config_;
  allCfgPack._codec_capability_ = &codec_capability_;
  allCfgPack._codec_local_capability_ = &codec_local_capability_;
  allCfgPack._codec_selectable_capability_ = &codec_selectable_capability_;
  allCfgPack._codec_user_config_ = &codec_user_config_;
  allCfgPack._codec_audio_config_ = &codec_audio_config_;

  if (p_peer_codec_info == nullptr || p_result_codec_config == nullptr) {
    log::error( ": nullptr input");
    status = AVDTP_UNSUPPORTED_CONFIGURATION;
    goto fail;
  }

  status = A2DP_ParseInfoLhdcV5(&sink_info_cie, p_peer_codec_info, is_capability, IS_SRC);
  if (status != A2DP_SUCCESS) {
    log::error( ": can't parse peer's Sink capabilities: error = {}",
         status);
    goto fail;
  }

  //
  // Build the preferred configuration
  //
  memset(&result_config_cie, 0, sizeof(result_config_cie));
  result_config_cie.vendorId = p_a2dp_lhdcv5_caps->vendorId;
  result_config_cie.codecId = p_a2dp_lhdcv5_caps->codecId;
  result_config_cie.version = sink_info_cie.version;

  //
  // Select the sample frequency
  //
  sampleRate = p_a2dp_lhdcv5_caps->sampleRate & sink_info_cie.sampleRate;
  log::info( ": sampleRate:peer:0x{:02x} local:0x{:02x} cap:0x{:02x} user:0x{:02x}",
       sink_info_cie.sampleRate, p_a2dp_lhdcv5_caps->sampleRate,
      sampleRate, codec_user_config_.sample_rate);

  codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
  switch (codec_user_config_.sample_rate) {
  case BTAV_A2DP_CODEC_SAMPLE_RATE_44100:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100) {
      result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_44100;
      codec_capability_.sample_rate = codec_user_config_.sample_rate;
      codec_config_.sample_rate = codec_user_config_.sample_rate;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_48000:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000) {
      result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
      codec_capability_.sample_rate = codec_user_config_.sample_rate;
      codec_config_.sample_rate = codec_user_config_.sample_rate;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_96000:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000) {
      result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_96000;
      codec_capability_.sample_rate = codec_user_config_.sample_rate;
      codec_config_.sample_rate = codec_user_config_.sample_rate;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_192000:
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000) {
      result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_192000;
      codec_capability_.sample_rate = codec_user_config_.sample_rate;
      codec_config_.sample_rate = codec_user_config_.sample_rate;
    }
    break;
  case BTAV_A2DP_CODEC_SAMPLE_RATE_16000:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_24000:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_88200:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_176400:
  case BTAV_A2DP_CODEC_SAMPLE_RATE_NONE:
    codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
    codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
    break;
  }

  // Select the sample frequency if there is no user preference
  do {
    // Compute the selectable capability
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100)
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000)
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000)
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000)
      codec_selectable_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_192000;

    if (codec_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_NONE) {
      log::info( ": sample rate configured by UI successfully 0x{:02x}",
           result_config_cie.sampleRate);
      break;
    }
    // Ignore follows if codec config is setup, otherwise pick a best one from default rules

    // Compute the common capability
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_192000;

    // No user preference - try the codec audio config
    if (select_audio_sample_rate(&codec_audio_config_, sampleRate,
        &result_config_cie, &codec_config_)) {
      log::info( ": select sample rate from audio: 0x{:02x}",
          result_config_cie.sampleRate);
      break;
    }

    // No user preference - try the default config
    if (select_best_sample_rate(
        a2dp_lhdcv5_source_default_caps.sampleRate & sink_info_cie.sampleRate,
        &result_config_cie, &codec_config_)) {
      log::info( ": select sample rate from default: 0x{:02x}",
          result_config_cie.sampleRate);
      break;
    }

    // No user preference - use the best match
    if (select_best_sample_rate(sampleRate, &result_config_cie,
        &codec_config_)) {
      log::info( ": select sample rate from best match: 0x{:02x}",
          result_config_cie.sampleRate);
      break;
    }
  } while (false);

  if (codec_config_.sample_rate == BTAV_A2DP_CODEC_SAMPLE_RATE_NONE) {
    log::error(
        ": cannot match sample frequency: local caps = 0x{:02x} "
        "peer info = 0x{:02x}",
         p_a2dp_lhdcv5_caps->sampleRate, sink_info_cie.sampleRate);
    goto fail;
  }
  codec_user_config_.sample_rate = codec_config_.sample_rate;
  log::info( ": => sample rate(0x{:02x}) = {}",
      result_config_cie.sampleRate,
      lhdcV5_sampleRate_toString(result_config_cie.sampleRate).c_str());

  //
  // Select the bits per sample
  //
  bitsPerSample = p_a2dp_lhdcv5_caps->bitsPerSample & sink_info_cie.bitsPerSample;
  log::info( ": bitsPerSample:peer:0x{:02x} local:0x{:02x} cap:0x{:02x} user:0x{:02x}",
       sink_info_cie.bitsPerSample, p_a2dp_lhdcv5_caps->bitsPerSample,
      bitsPerSample, codec_user_config_.bits_per_sample);

  codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
  switch (codec_user_config_.bits_per_sample) {
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_16) {
      result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_16;
      codec_capability_.bits_per_sample = codec_user_config_.bits_per_sample;
      codec_config_.bits_per_sample = codec_user_config_.bits_per_sample;
    }
    break;
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_24) {
      result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_24;
      codec_capability_.bits_per_sample = codec_user_config_.bits_per_sample;
      codec_config_.bits_per_sample = codec_user_config_.bits_per_sample;
    }
    break;
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32:
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_32) {
      result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_32;
      codec_capability_.bits_per_sample = codec_user_config_.bits_per_sample;
      codec_config_.bits_per_sample = codec_user_config_.bits_per_sample;
    }
    break;
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE:
    result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_NS;
    codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
    codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
    break;
  }

  // Select the bits per sample if there is no user preference
  do {
    // Compute the selectable capability
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_16)
      codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_24)
      codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_32)
      codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;

    if (codec_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE) {
      log::info( ": bit_per_sample configured by UI successfully 0x{:02x}",
           result_config_cie.bitsPerSample);
      break;
    }
    // Ignore follows if codec config is setup, otherwise pick a best one from default rules

    // Compute the common capability
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_16)
      codec_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_24)
      codec_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
    if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_32)
      codec_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;

    // No user preference - the the codec audio config
    if (select_audio_bits_per_sample(&codec_audio_config_, bitsPerSample,
        &result_config_cie, &codec_config_)) {
      log::info( ": select bit per sample from audio: 0x{:02x}",
          result_config_cie.bitsPerSample);
      break;
    }

    // No user preference - try the default config
    if (select_best_bits_per_sample(
        a2dp_lhdcv5_source_default_caps.bitsPerSample & sink_info_cie.bitsPerSample,
        &result_config_cie, &codec_config_)) {
      log::info( ": select bit per sample from default: 0x{:02x}",
          result_config_cie.bitsPerSample);
      break;
    }

    // No user preference - use the best match
    if (select_best_bits_per_sample(bitsPerSample, &result_config_cie,
        &codec_config_)) {
      log::info( ": select sample rate from best match: 0x{:02x}",
          result_config_cie.bitsPerSample);
      break;
    }
  } while (false);

  if (codec_config_.bits_per_sample == BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE) {
    log::error(
        ": cannot match bits per sample: local caps = 0x{:02x} "
        "peer info = 0x{:02x}",
         p_a2dp_lhdcv5_caps->bitsPerSample,
        sink_info_cie.bitsPerSample);
    goto fail;
  }
  codec_user_config_.bits_per_sample = codec_config_.bits_per_sample;
  log::info( ": => bit per sample(0x{:02x}) = {}",
      result_config_cie.bitsPerSample,
      lhdcV5_bitPerSample_toString(result_config_cie.bitsPerSample).c_str());

  // Select the channel mode
  codec_user_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  codec_selectable_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  codec_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  codec_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  log::info( ": channelMode = Only supported stereo");

  // Update frameLenType
  result_config_cie.frameLenType = sink_info_cie.frameLenType;
  log::info( ": => frame length type(0x{:02x}) = {}",
      result_config_cie.frameLenType,
      lhdcV5_frameLenType_toString(result_config_cie.frameLenType).c_str());

  // Update maxTargetBitrate
  result_config_cie.maxTargetBitrate = sink_info_cie.maxTargetBitrate;
  log::info( ": => peer Max Bit Rate(0x{:02x}) = {}",
      result_config_cie.maxTargetBitrate,
      lhdcV5_MaxTargetBitRate_toString(result_config_cie.maxTargetBitrate).c_str());

  // Update minTargetBitrate
  result_config_cie.minTargetBitrate = sink_info_cie.minTargetBitrate;
  log::info( ": => peer Min Bit Rate(0x{:02x}) = {}",
      result_config_cie.minTargetBitrate,
      lhdcV5_MinTargetBitRate_toString(result_config_cie.minTargetBitrate).c_str());

  //
  // Update Feature/Capabilities to A2DP specifics
  //
  /*******************************************
   * for features that can be enabled by user-control, exam features tag on the specific.
   * current user-control enabling features:
   *    Feature: AR
   *    Feature: Lossless
   *******************************************/
  //features on specific 3
  if ((codec_user_config_.codec_specific_3 & A2DP_LHDC_VENDOR_FEATURE_MASK) != A2DP_LHDCV5_FEATURE_MAGIC_NUM)
  {
    // reset the specific and apply tag
    codec_user_config_.codec_specific_3 = A2DP_LHDCV5_FEATURE_MAGIC_NUM;

    // get previous status of user-control enabling features from codec_config, then restore to user settings
    //
    // Feature: LL
    hasUserSet = A2DP_IsFeatureInCodecConfigLhdcV5(&allCfgPack, LHDCV5_FEATURE_CODE_LL);
    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_LL,
        A2DP_LHDC_TO_A2DP_CODEC_USER_,
        (hasUserSet?true:false));
    log::info( ": LHDC features tag check fail, default UI status[LL] => {}",  hasUserSet?"true":"false");

    // Feature: AR (default UI: OFF)
    hasUserSet = false;
    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_AR,
        A2DP_LHDC_TO_A2DP_CODEC_USER_,
        (hasUserSet?true:false));
    log::info( ": LHDC features tag check fail, default UI status[AR] => {}",  hasUserSet?"true":"false");

    // Feature: Lossless (default UI: OFF)
    hasUserSet = false;
    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
      &allCfgPack,
      LHDCV5_FEATURE_CODE_LLESS,
      A2DP_LHDC_TO_A2DP_CODEC_USER_,
      (hasUserSet?true:false));
    log::info( ": LHDC features tag check fail, default UI status[LLESS] => {}",  hasUserSet?"true":"false");

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    // Feature: Lossless Raw(default UI: OFF)
    hasUserSet = false;
    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
      &allCfgPack,
      LHDCV5_FEATURE_CODE_LLESS_RAW,
      A2DP_LHDC_TO_A2DP_CODEC_USER_,
      (hasUserSet?true:false));
    log::info( ": LHDC features tag check fail, default UI status[LLESS Raw] => {}",  hasUserSet?"true":"false");
#endif
  }

  /*************************************************
   *  quality mode initialize
   *************************************************/
  if ((codec_user_config_.codec_specific_1 & A2DP_LHDC_VENDOR_CMD_MASK) != A2DP_LHDC_QUALITY_MAGIC_NUM) {
    codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_VENDOR_CMD_MASK | A2DP_LHDC_QUALITY_MASK);
    codec_user_config_.codec_specific_1 |= (A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_ABR);
    log::info( ": tag not match, use default Quality Mode: ABR");
  }
  qualityMode = (uint8_t)codec_user_config_.codec_specific_1 & A2DP_LHDC_QUALITY_MASK;

  /*******************************************
   *  JAS: caps-control enabling
   *******************************************/
  {
    hasFeature = (p_a2dp_lhdcv5_caps->hasFeatureJAS & sink_info_cie.hasFeatureJAS);
    // reset first
    result_config_cie.hasFeatureJAS = false;
    hasUserSet = true;  //caps-control enabling case => always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_JAS,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
            false);
    // update
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureJAS = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_JAS,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }
    log::info( ": featureJAS: enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureJAS?"Y":"N"),
        sink_info_cie.hasFeatureJAS,
        p_a2dp_lhdcv5_caps->hasFeatureJAS);
  }

  /*******************************************
   *  AR: user-control/peer-OTA control enabling
   *******************************************/
  {
    hasFeature = (p_a2dp_lhdcv5_caps->hasFeatureAR & sink_info_cie.hasFeatureAR);
    // reset first
    result_config_cie.hasFeatureAR = false;
    hasUserSet = A2DP_IsFeatureInUserConfigLhdcV5(&allCfgPack, LHDCV5_FEATURE_CODE_AR);

    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_AR,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);
    // update
    // default AR turning on condition: (customizable)
    //  1. both sides have the capabilities
    //  2. (UI on SRC side turns on) || (SNK set AR_ON in codec info)
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureAR = true;  //decide to turn on feature in encoder
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_AR,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }
    log::info( ": featureAR: enabled? <{}> Peer:0x{:02x} Local:0x{:02x} User:{}",
        (result_config_cie.hasFeatureAR?"Y":"N"),
        sink_info_cie.hasFeatureAR,
        p_a2dp_lhdcv5_caps->hasFeatureAR,
        (hasUserSet?"Y":"N"));
  }

  /*******************************************
   *  META: caps-control enabling
   *******************************************/
  {
    hasFeature = (p_a2dp_lhdcv5_caps->hasFeatureMETA & sink_info_cie.hasFeatureMETA);
    // reset first
    result_config_cie.hasFeatureMETA = false;
    hasUserSet = true;  //caps-control enabling, always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_META,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);
    // update
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureMETA = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_META,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }
    log::info( ": featureMETA: enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureMETA?"Y":"N"),
        sink_info_cie.hasFeatureMETA,
        p_a2dp_lhdcv5_caps->hasFeatureMETA);
  }

  /*******************************************
   *  Low Latency: user-control enabling
   *******************************************/
  {
    hasFeature = (p_a2dp_lhdcv5_caps->hasFeatureLL & sink_info_cie.hasFeatureLL);
    // reset first
    result_config_cie.hasFeatureLL = false;
    hasUserSet = A2DP_IsFeatureInUserConfigLhdcV5(&allCfgPack, LHDCV5_FEATURE_CODE_LL);

    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_LL,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);
    // update
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureLL = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_LL,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }
    log::info( ": featureLL: enabled? <{}> Peer:0x{:02x} Local:0x{:02x} User:{}",
        (result_config_cie.hasFeatureLL?"Y":"N"),
        sink_info_cie.hasFeatureLL,
        p_a2dp_lhdcv5_caps->hasFeatureLL,
        (hasUserSet?"Y":"N"));
  }

  /*******************************************
   *  LLESS (normal mode): user-control enabling
   *******************************************/
  {
    hasFeature = (p_a2dp_lhdcv5_caps->hasFeatureLLESS48K & sink_info_cie.hasFeatureLLESS48K);
    // reset first
    result_config_cie.hasFeatureLLESS48K = false;
    result_config_cie.hasFeatureLLESS24Bit = false;
    result_config_cie.hasFeatureLLESS96K = false;
    hasUserSet = A2DP_IsFeatureInUserConfigLhdcV5(&allCfgPack, LHDCV5_FEATURE_CODE_LLESS);  //UI-control

    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_LLESS,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);

    // update
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureLLESS48K = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_LLESS,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);

      if (p_a2dp_lhdcv5_caps->hasFeatureLLESS24Bit & sink_info_cie.hasFeatureLLESS24Bit) {
        result_config_cie.hasFeatureLLESS24Bit = true;
      }

      if (p_a2dp_lhdcv5_caps->hasFeatureLLESS96K & sink_info_cie.hasFeatureLLESS96K) {
        result_config_cie.hasFeatureLLESS96K = true;
      }
    }

    log::info( ": featureLLESS: enabled? <{}> Peer:0x{:02x} Local:0x{:02x} User:{} 24bit:{} 96K:{}",
        (result_config_cie.hasFeatureLLESS48K?"Y":"N"),
        sink_info_cie.hasFeatureLLESS48K,
        p_a2dp_lhdcv5_caps->hasFeatureLLESS48K,
        (hasUserSet?"Y":"N"),
        (result_config_cie.hasFeatureLLESS24Bit?"Y":"N"),
        (result_config_cie.hasFeatureLLESS96K?"Y":"N"));
  }

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
  /*******************************************
   *  LLESS (RAW mode): user-control enabling
   *******************************************/
  {
    hasFeature = (p_a2dp_lhdcv5_caps->hasFeatureLLESSRaw & sink_info_cie.hasFeatureLLESSRaw);
    // reset first
    result_config_cie.hasFeatureLLESSRaw = false;
    hasUserSet = A2DP_IsFeatureInUserConfigLhdcV5(&allCfgPack, LHDCV5_FEATURE_CODE_LLESS_RAW);  //UI-control

    A2DP_UpdateFeatureToA2dpConfigLhdcV5(
        &allCfgPack,
        LHDCV5_FEATURE_CODE_LLESS_RAW,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);

    // update, lossless must be enabled first for raw mode
    if (hasFeature && hasUserSet && result_config_cie.hasFeatureLLESS48K) {
      result_config_cie.hasFeatureLLESSRaw = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_LLESS_RAW,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }

    log::info( ": featureLLESSRaw: enabled? <{}> Peer:0x{:02x} Local:0x{:02x} User:{}",
        (result_config_cie.hasFeatureLLESSRaw?"Y":"N"),
        sink_info_cie.hasFeatureLLESSRaw,
        p_a2dp_lhdcv5_caps->hasFeatureLLESSRaw,
        (hasUserSet?"Y":"N"));
  }
#endif


  log::info(": current quality_mode = 0x{:02x}",  qualityMode);

  /*******************************************
   * Update LHDC Peer Max Bitrate Index to specific 1
   *******************************************/
  // store peer cap: max target bitrate index for UI reference
  codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_PEER_MAX_BITRATE_MASK);
  switch(result_config_cie.maxTargetBitrate)
  {
    case A2DP_LHDCV5_MAX_BIT_RATE_400K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MAX_BIT_RATE_400K << 4);
      break;
    }
    case A2DP_LHDCV5_MAX_BIT_RATE_500K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MAX_BIT_RATE_500K << 4);
      break;
    }
    case A2DP_LHDCV5_MAX_BIT_RATE_900K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MAX_BIT_RATE_900K << 4);
      break;
    }
    case A2DP_LHDCV5_MAX_BIT_RATE_1000K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MAX_BIT_RATE_1000K << 4);
      break;
    }
  }

  /*******************************************
   * Update LHDC Peer Min Bitrate Index to specific 1
   *******************************************/
  // store peer cap: max target bitrate index for UI reference
  codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_PEER_MIN_BITRATE_MASK);
  switch(result_config_cie.minTargetBitrate)
  {
    case A2DP_LHDCV5_MIN_BIT_RATE_64K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MIN_BIT_RATE_64K << 12);
      break;
    }
    case A2DP_LHDCV5_MIN_BIT_RATE_160K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MIN_BIT_RATE_160K << 12);
      break;
    }
    case A2DP_LHDCV5_MIN_BIT_RATE_256K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MIN_BIT_RATE_256K << 12);
      break;
    }
    case A2DP_LHDCV5_MIN_BIT_RATE_400K:
    {
      codec_user_config_.codec_specific_1 |= (A2DP_LHDCV5_MIN_BIT_RATE_400K << 12);
      break;
    }
  }


  //
  // special rules: Lossless enable/disable re-adjustion
  //
  if(result_config_cie.hasFeatureLLESS48K == true) {
    //Rule: (lossless enabled): only runs at "no limit" maxTargetBitrate
    //    reason:  operational bit rate of lossless requires at least 900kbps.
    if (!A2DP_MaxBitRatetoQualityLevelLhdcV5(&bitRateQmode, result_config_cie.maxTargetBitrate)) {
      log::error( ": get quality mode from maxTargetBitrate error");
      goto fail;
    }
    //HIGH1 ==> A2DP_LHDC_MAX_BIT_RATE_1000K, equivalent to "no upper limit"
    if (bitRateQmode != A2DP_LHDC_QUALITY_HIGH1) {
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_LLESS,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              false);
      result_config_cie.hasFeatureLLESS48K = false;
      result_config_cie.hasFeatureLLESS24Bit = false;
      result_config_cie.hasFeatureLLESS96K = false;
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_LLESS_RAW,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              false);
      result_config_cie.hasFeatureLLESSRaw = false;
#endif
      log::info( ": disable lossless due to peer Max Bit Rate not NO LIMIT");
    }
    // Rule: AR feature is mutex with lossless feature (lossless prior to AR)
    if (result_config_cie.hasFeatureLLESS48K == true && result_config_cie.hasFeatureAR) {
      result_config_cie.hasFeatureAR = false;
      A2DP_UpdateFeatureToA2dpConfigLhdcV5(
          &allCfgPack,
          LHDCV5_FEATURE_CODE_AR,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              false);
    }
  }

  //
  // special rules: sample rate re-adjustion
  //
  if(result_config_cie.hasFeatureAR == true) {
    // set (48KHz sample rate) when AR
    if (codec_user_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_48000) {
      codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
      log::info( ": set 48KHz sample Rate for running AR");
    }
  }

  if (result_config_cie.hasFeatureLLESS48K == true) {
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    if (result_config_cie.hasFeatureLLESSRaw == true) {
      // lossless (raw mode)
      //   => 48 or 96KHz for lossless raw mode
      if (result_config_cie.hasFeatureLLESS96K == false) {
        // only have capability of 48KHz lossless raw mode (not have 96KHz lossless)
        codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
        log::info( ": (lossless raw) set 48KHz sample rate");
      } else {
        // have 96KHz lossless capabilities
        if (codec_user_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_48000 &&
            codec_user_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_96000) {
          //  => set default 48KHz when user-configured sample rate is not supported
          codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
          codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
          codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
          result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
          log::info( ": (lossless raw) default 48KHz sample rate");
        } else {
          //  48 or 96KHz => use sample rate from user
          log::info( ": (lossless raw) apply  sample rate", 
              lhdcV5_sampleRate_toString(result_config_cie.sampleRate).c_str());
        }
      }
    } else {
#endif
      // lossless (normal mode):
      if (result_config_cie.hasFeatureLLESS96K == false) {
        // only have capability of 48KHz lossless (not have 96KHz lossless)
        //   => fixed 48KHz
        codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
        log::info( ": (lossless) fixed 48KHz sample rate");
      } else {
        // user configure other sample rates(not 48KHz and 96KHz)
        //  => default 48KHz
        if (codec_user_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_48000 &&
            codec_user_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_96000) {
          //  => set default 48KHz when user-configured sample rate is not supported
          codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
          codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
          codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
          result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;
          log::info( ": (lossless) default 48KHz sample rate");
        } else {
          // user configure 48/96KHz
          //  => use sample rate from user
          log::info( ": (lossless) apply  sample rate", 
              lhdcV5_sampleRate_toString(result_config_cie.sampleRate).c_str());
        }
      }
#ifdef LHDC_LOSSLESS_RAW_SUPPORT
    }
#endif
  }

  //
  // special rules: bit per sample re-adjustion
  //
  if (result_config_cie.hasFeatureAR == true) {
    // set (default 24 bits per sample) if not in 16 or 24 when AR
    if (codec_user_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16 &&
        codec_user_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
      codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
      codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
      codec_user_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
      result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_24;
      log::info( ": set 24 bits per sample for running AR");
    }
  }

  if (result_config_cie.hasFeatureLLESS48K == true) {
    if (result_config_cie.hasFeatureLLESS24Bit == true) {
      if (codec_user_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16 &&
          codec_user_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
        // check bit-per-sample capability
        bitsPerSample = p_a2dp_lhdcv5_caps->bitsPerSample & sink_info_cie.bitsPerSample;

        if (bitsPerSample & A2DP_LHDCV5_BIT_FMT_24) {
          // unsupported bitrate, use 24bit as default
          codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
          codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
          codec_user_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
          result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_24;
          log::info( ": (lossless-24bit): use default 24 mode");
        } else {
          // unsupported bitrate, use 16bit as default
          codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
          codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
          codec_user_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
          result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_16;
          log::info( ": (lossless-24bit): use default 16 bit mode");
        }
      } else {
        // bitrate supported, adopt user's configuration
        if (codec_user_config_.bits_per_sample == BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
          log::info( ": (lossless-24bit): use 24 bit mode");
        } else {
          log::info( ": (lossless-24bit): use 16 bit mode");
        }
      }
    } else {
      // hasFeatureLLESS24Bit not supported, always use 16bit
      codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      codec_user_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_16;
      log::info( ": (lossless-16bit): 16 bit mode only");
    }
  }

  // lossless and lossless raw mode 96KHz case:
  // Current lossless and lossless raw mode support 96KHz 24bit only.
  //   reconfigure to other config if user configures (96KHz + 16bit)
  if ((result_config_cie.hasFeatureLLESS48K == true) &&
      (result_config_cie.hasFeatureLLESS96K == true) &&
      (result_config_cie.sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_96000)) {
    if (result_config_cie.hasFeatureLLESS24Bit == true) {
      if (result_config_cie.bitsPerSample == A2DP_LHDCV5_BIT_FMT_24) {
        log::info( ": (lossless 96KHz) : 24bit OK");
      } else {
        codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
        codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
        codec_user_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
        result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_24;
        log::info( ": (lossless 96KHz) : unsupported bit depth but 24bit capable, reconfigure to 24bit");
      }
    } else {
      codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      result_config_cie.sampleRate = A2DP_LHDCV5_SAMPLING_FREQ_48000;

      codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      codec_user_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      result_config_cie.bitsPerSample = A2DP_LHDCV5_BIT_FMT_16;
      log::info( ": (lossless 96KHz) : 16 and 24bit not capable, reconfigure to 48KHz 16bit");
    }
  }

  //
  // special rules: quality mode re-adjustion
  //
  if (result_config_cie.hasFeatureLLESS48K == true) {
    // (lossless enabled): re-adjusting by maxTargetBitrate, minTargetBitrate is not supported now
    // (lossless enabled): can only run in ABR mode
    if ((qualityMode != A2DP_LHDC_QUALITY_ABR)) {
      log::info( ": reset non-supported quality mode () to () for lossless", 
          lhdcV5_QualityModeBitRate_toString(qualityMode).c_str(),
          lhdcV5_QualityModeBitRate_toString(A2DP_LHDC_QUALITY_ABR).c_str());
      codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_VENDOR_CMD_MASK | A2DP_LHDC_QUALITY_MASK);
      codec_user_config_.codec_specific_1 |= (A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_ABR);
      qualityMode = A2DP_LHDC_QUALITY_ABR;
    }
  } else {
    // (lossless disabled):
    // non-ABR qualityMode re-adjusting
    if (qualityMode != A2DP_LHDC_QUALITY_ABR) {
      // get corresponding quality mode of the max target bit rate
      if (!A2DP_MaxBitRatetoQualityLevelLhdcV5(&bitRateQmode, result_config_cie.maxTargetBitrate)) {
        log::error( ": get quality mode from maxTargetBitrate error");
        goto fail;
      }
      // downgrade audio quality according to the max target bit rate
      if (qualityMode > bitRateQmode) {
        codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_VENDOR_CMD_MASK | A2DP_LHDC_QUALITY_MASK);
        codec_user_config_.codec_specific_1 |= (A2DP_LHDC_QUALITY_MAGIC_NUM | bitRateQmode);
        qualityMode = bitRateQmode;
        log::info( ": downgrade quality mode to 0x{:02x}",  qualityMode);
      }

      // get corresponding quality mode of the min target bit rate
      if (!A2DP_MinBitRatetoQualityLevelLhdcV5(&bitRateQmode, result_config_cie.minTargetBitrate)) {
        log::error( ": get quality mode from minTargetBitrate error");
        goto fail;
      }
      // upgrade audio quality according to the min target bit rate
      if (qualityMode < bitRateQmode) {
        codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_VENDOR_CMD_MASK | A2DP_LHDC_QUALITY_MASK);
        codec_user_config_.codec_specific_1 |= (A2DP_LHDC_QUALITY_MAGIC_NUM | bitRateQmode);
        qualityMode = bitRateQmode;
        log::info( ": upgrade quality mode to 0x{:02x}",  qualityMode);
      }

      // rule: if (sample rate >= 96KHz) fixed bitrate must be at least 256kbps(LOW3),
      if (result_config_cie.sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_96000 ||
          result_config_cie.sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_192000) {
        if (qualityMode < A2DP_LHDC_QUALITY_LOW3) {
          codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_VENDOR_CMD_MASK | A2DP_LHDC_QUALITY_MASK);
          codec_user_config_.codec_specific_1 |= (A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW3);
          qualityMode = A2DP_LHDC_QUALITY_LOW3;
          log::info( ": upgrade quality mode to 0x{:02x} due to higher sample rates",  qualityMode);
        }
      }

      // rule: if (sample rate <= 48KHz) fixed bitrate must be at most 900kbps(HIGH),
      if (result_config_cie.sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_44100 ||
          result_config_cie.sampleRate == A2DP_LHDCV5_SAMPLING_FREQ_48000) {
        if (qualityMode >= A2DP_LHDC_QUALITY_HIGH1 &&
            qualityMode != A2DP_LHDC_QUALITY_ABR) {
          codec_user_config_.codec_specific_1 &= ~(A2DP_LHDC_VENDOR_CMD_MASK | A2DP_LHDC_QUALITY_MASK);
          codec_user_config_.codec_specific_1 = (A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_HIGH);
          qualityMode = A2DP_LHDC_QUALITY_HIGH;
          log::info( ": downgrade quality mode to 0x{:02x} due to lower sample rates",  qualityMode);
        }
      }
    }
  }

  log::info( ": => final quality mode(0x{:02x}) = {}",
      qualityMode,
      lhdcV5_QualityModeBitRate_toString(qualityMode).c_str());

  /* Setup final nego result config to peer */
  if (A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &result_config_cie,
      p_result_codec_config) != A2DP_SUCCESS) {
    log::error( ": A2DP build info fail");
    goto fail;
  }

  //
  // Copy the codec-specific fields if they are not zero
  //
  if (codec_user_config_.codec_specific_1 != 0)
    codec_config_.codec_specific_1 = codec_user_config_.codec_specific_1;
  if (codec_user_config_.codec_specific_2 != 0)
    codec_config_.codec_specific_2 = codec_user_config_.codec_specific_2;
  if (codec_user_config_.codec_specific_3 != 0)
    codec_config_.codec_specific_3 = codec_user_config_.codec_specific_3;
  if (codec_user_config_.codec_specific_4 != 0)
    codec_config_.codec_specific_4 = codec_user_config_.codec_specific_4;

  // Create a local copy of the peer codec capability, and the
  // result codec config.
  if (is_capability) {
    status = A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
        ota_codec_peer_capability_);
  } else {
    status = A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
        ota_codec_peer_config_);
  }
  CHECK(status == A2DP_SUCCESS);

  status = A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &result_config_cie,
      ota_codec_config_);
  CHECK(status == A2DP_SUCCESS);
  return A2DP_SUCCESS;

  fail:
  // Restore the internal state
  codec_config_ = saved_codec_config;
  codec_capability_ = saved_codec_capability;
  codec_selectable_capability_ = saved_codec_selectable_capability;
  codec_user_config_ = saved_codec_user_config;
  codec_audio_config_ = saved_codec_audio_config;
  memcpy(ota_codec_config_, saved_ota_codec_config, sizeof(ota_codec_config_));
  memcpy(ota_codec_peer_capability_, saved_ota_codec_peer_capability,
      sizeof(ota_codec_peer_capability_));
  memcpy(ota_codec_peer_config_, saved_ota_codec_peer_config,
      sizeof(ota_codec_peer_config_));
  return status;
}

bool A2dpCodecConfigLhdcV5Base::setPeerCodecCapabilities(
    const uint8_t* p_peer_codec_capabilities) {
  std::lock_guard<std::recursive_mutex> lock(codec_mutex_);
  tA2DP_LHDCV5_CIE peer_info_cie;
  uint8_t sampleRate;
  uint8_t bits_per_sample;
  tA2DP_STATUS status;
  const tA2DP_LHDCV5_CIE* p_a2dp_lhdcv5_caps =
      (is_source_) ? &a2dp_lhdcv5_source_caps : &a2dp_lhdcv5_sink_caps;

  // Save the internal state
  btav_a2dp_codec_config_t saved_codec_selectable_capability =
      codec_selectable_capability_;
  uint8_t saved_ota_codec_peer_capability[AVDT_CODEC_SIZE];
  memcpy(saved_ota_codec_peer_capability, ota_codec_peer_capability_,
      sizeof(ota_codec_peer_capability_));

  if (p_peer_codec_capabilities == nullptr) {
    log::error( ": nullptr input");
    status = AVDTP_UNSUPPORTED_CONFIGURATION;
    goto fail;
  }

  status = A2DP_ParseInfoLhdcV5(&peer_info_cie, p_peer_codec_capabilities, true, IS_SRC);
  if (status != A2DP_SUCCESS) {
    log::error( ": can't parse peer's capabilities: error = {}",
         status);
    goto fail;
  }

  // Compute the selectable capability - sample rate
  sampleRate = p_a2dp_lhdcv5_caps->sampleRate & peer_info_cie.sampleRate;
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_44100) {
    codec_selectable_capability_.sample_rate |=
        BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  }
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_48000) {
    codec_selectable_capability_.sample_rate |=
        BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  }
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_96000) {
    codec_selectable_capability_.sample_rate |=
        BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
  }
  if (sampleRate & A2DP_LHDCV5_SAMPLING_FREQ_192000) {
    codec_selectable_capability_.sample_rate |=
        BTAV_A2DP_CODEC_SAMPLE_RATE_192000;
  }

  // Compute the selectable capability - bits per sample
  bits_per_sample = p_a2dp_lhdcv5_caps->bitsPerSample & peer_info_cie.bitsPerSample;
  if (bits_per_sample & A2DP_LHDCV5_BIT_FMT_16) {
    codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
  }
  if (bits_per_sample & A2DP_LHDCV5_BIT_FMT_24) {
    codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
  }
  if (bits_per_sample & A2DP_LHDCV5_BIT_FMT_32) {
    codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32;
  }

  // Compute the selectable capability - channel mode
  codec_selectable_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;

  status = A2DP_BuildInfoLhdcV5(AVDT_MEDIA_TYPE_AUDIO, &peer_info_cie,
      ota_codec_peer_capability_);
  CHECK(status == A2DP_SUCCESS);
  return A2DP_SUCCESS;

  fail:
  // Restore the internal state
  codec_selectable_capability_ = saved_codec_selectable_capability;
  memcpy(ota_codec_peer_capability_, saved_ota_codec_peer_capability,
      sizeof(ota_codec_peer_capability_));
  return status;
}

////////
//    class implementation for LHDC V5 Sink
////////
A2dpCodecConfigLhdcV5Sink::A2dpCodecConfigLhdcV5Sink(
    btav_a2dp_codec_priority_t codec_priority)
: A2dpCodecConfigLhdcV5Base(BTAV_A2DP_CODEC_INDEX_SINK_LHDCV5,
    A2DP_VendorCodecIndexStrLhdcV5Sink(),
    codec_priority, false) {}

A2dpCodecConfigLhdcV5Sink::~A2dpCodecConfigLhdcV5Sink() {}

bool A2dpCodecConfigLhdcV5Sink::init() {
  // Load the decoder
  if (!A2DP_VendorLoadDecoderLhdcV5()) {
    log::error( ": cannot load the decoder");
    return false;
  }

  return true;
}

bool A2dpCodecConfigLhdcV5Sink::useRtpHeaderMarkerBit() const {
  // TODO: This method applies only to Source codecs
  return false;
}

#if 0
bool A2dpCodecConfigLhdcV5Sink::updateEncoderUserConfig(
    UNUSED_ATTR const tA2DP_ENCODER_INIT_PEER_PARAMS* p_peer_params,
    UNUSED_ATTR bool* p_restart_input, UNUSED_ATTR bool* p_restart_output,
    UNUSED_ATTR bool* p_config_updated) {
  // TODO: This method applies only to Source codecs
  return false;
}
#endif

#if 0
uint64_t A2dpCodecConfigLhdcV5Sink::encoderIntervalMs() const {
  // TODO: This method applies only to Source codecs
  return 0;
}
#endif

#if 0
int A2dpCodecConfigLhdcV5Sink::getEffectiveMtu() const {
  // TODO: This method applies only to Source codecs
  return 0;
}
#endif

////////
//    APIs for calling from encoder/decoder module - START
////////
bool A2DP_VendorGetMaxBitRateLhdcV5(uint32_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }

  switch (lhdc_cie.maxTargetBitrate) {
  case A2DP_LHDCV5_MAX_BIT_RATE_1000K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_HIGH1;
    return true;
  case A2DP_LHDCV5_MAX_BIT_RATE_900K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_HIGH;
    return true;
  case A2DP_LHDCV5_MAX_BIT_RATE_500K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_MID;
    return true;
  case A2DP_LHDCV5_MAX_BIT_RATE_400K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_LOW;
    return true;
  }

  return false;
}

bool A2DP_VendorGetMinBitRateLhdcV5(uint32_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, true, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }

  switch (lhdc_cie.minTargetBitrate) {
  case A2DP_LHDCV5_MIN_BIT_RATE_400K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_LOW;
    return true;
  case A2DP_LHDCV5_MIN_BIT_RATE_256K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_LOW3;
    return true;
  case A2DP_LHDCV5_MIN_BIT_RATE_160K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_LOW1;
    return true;
  case A2DP_LHDCV5_MIN_BIT_RATE_64K:
    *retval = (uint32_t)A2DP_LHDC_QUALITY_LOW0;
    return true;
  }

  return false;
}

bool A2DP_VendorGetVersionLhdcV5(uint32_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }

  *retval = (uint32_t)lhdc_cie.version;

  return true;
}

bool A2DP_VendorGetBitPerSampleLhdcV5(uint8_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }

  *retval = (uint32_t)lhdc_cie.bitsPerSample;

  return true;
}

bool A2DP_VendorHasJASFlagLhdcV5(uint8_t *retval, const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  *retval = lhdc_cie.hasFeatureJAS ? 1 : 0;

  return true;
}

bool A2DP_VendorHasARFlagLhdcV5(uint8_t *retval, const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  *retval = lhdc_cie.hasFeatureAR ? 1 : 0;

  return true;
}

bool A2DP_VendorHasMETAFlagLhdcV5(uint8_t *retval, const uint8_t* p_codec_info) {
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  *retval = lhdc_cie.hasFeatureMETA ? 1 : 0;

  return true;
}

//orig A2DP_VendorGetLowLatencyStateLhdcV5
bool A2DP_VendorHasLLFlagLhdcV5(uint8_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  *retval = lhdc_cie.hasFeatureLL ? 1 : 0;

  return true;
}

bool A2DP_VendorHasLLessFlagLhdcV5(uint8_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  *retval = lhdc_cie.hasFeatureLLESS48K ? 1 : 0;

  return true;
}

#ifdef LHDC_LOSSLESS_RAW_SUPPORT
bool A2DP_VendorHasLLessRawFlagLhdcV5(uint8_t *retval, const uint8_t* p_codec_info){
  tA2DP_LHDCV5_CIE lhdc_cie;
  tA2DP_STATUS a2dp_status;

  if (p_codec_info == nullptr || retval == nullptr) {
    log::error( ": nullptr input");
    return false;
  }

  // Check whether the codec info contains valid data
  a2dp_status = A2DP_ParseInfoLhdcV5(&lhdc_cie, p_codec_info, false, IS_SRC);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error( ": cannot decode codec information: {}", 
        a2dp_status);
    return false;
  }
  *retval = lhdc_cie.hasFeatureLLESSRaw ? 1 : 0;

  return true;
}
#endif
////////
//    APIs for calling from encoder/decoder module - END
////////

/******************************************************************************
 *
 *  Copyright 2002-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  Utility functions to help build and parse SBC Codec Information Element
 *  and Media Payload.
 *
 ******************************************************************************/

#define LOG_TAG "bluetooth-a2dp"
#include "bt_target.h"

#include "a2dp_vendor_lhdc_constants.h"
#include "a2dp_vendor_lhdcv3.h"
#include "a2dp_vendor_lhdcv3_dec.h"

#include <string.h>

#include <base/logging.h>
#include "a2dp_vendor.h"
#include "a2dp_vendor_lhdcv3_decoder.h"
#include "btif/include/btif_av_co.h"
#include "internal_include/bt_trace.h"
#include "stack/include/bt_hdr.h"
#include "osi/include/osi.h"
#include <bluetooth/log.h>
#include "os/log.h"
using namespace bluetooth;



typedef struct {
  btav_a2dp_codec_config_t *_codec_config_;
  btav_a2dp_codec_config_t *_codec_capability_;
  btav_a2dp_codec_config_t *_codec_local_capability_;
  btav_a2dp_codec_config_t *_codec_selectable_capability_;
  btav_a2dp_codec_config_t *_codec_user_config_;
  btav_a2dp_codec_config_t *_codec_audio_config_;
}tA2DP_CODEC_CONFIGS_PACK;

typedef struct {
  uint8_t   featureCode;    /* code definition for LHDC API ex: LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE */
  uint8_t   inSpecBank;     /* in which specific bank */
  uint8_t   bitPos;         /* at which bit index number of the specific bank */
}tA2DP_LHDC_FEATURE_POS;

/* source side metadata of JAS feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_JAS = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_JAS_SPEC_BIT_POS,
};
/* source side metadata of AR feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_AR = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_AR_SPEC_BIT_POS
};
/* source side metadata of LLAC feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_LLAC = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_LLAC_SPEC_BIT_POS
};
/* source side metadata of META feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_META = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_META_SPEC_BIT_POS
};
/* source side metadata of MBR feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_MBR = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_MBR_SPEC_BIT_POS
};
/* source side metadata of LARC feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_LARC = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_LARC_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_LARC_SPEC_BIT_POS
};
/* source side metadata of LHDCV4 feature */
static const tA2DP_LHDC_FEATURE_POS a2dp_lhdc_source_caps_LHDCV4 = {
    LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE,
    LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX,
    A2DP_LHDC_V4_SPEC_BIT_POS
};

static const UNUSED_ATTR tA2DP_LHDC_FEATURE_POS a2dp_lhdc_sink_caps_all[] = {
    a2dp_lhdc_source_caps_JAS,
    a2dp_lhdc_source_caps_AR,
    a2dp_lhdc_source_caps_LLAC,
    a2dp_lhdc_source_caps_META,
    a2dp_lhdc_source_caps_MBR,
    a2dp_lhdc_source_caps_LARC,
    a2dp_lhdc_source_caps_LHDCV4,
};

// data type for the LHDC Codec Information Element */
// NOTE: bits_per_sample is needed only for LHDC encoder initialization.
typedef struct {
  uint32_t vendorId;
  uint16_t codecId;    /* Codec ID for LHDC */
  uint8_t sampleRate;  /* Sampling Frequency */
  uint8_t llac_sampleRate;  /* Sampling Frequency for LLAC */
  btav_a2dp_codec_bits_per_sample_t bits_per_sample;
  uint8_t channelSplitMode;
  uint8_t version;
  uint8_t maxTargetBitrate;
  bool isLLSupported;
  //uint8_t supportedBitrate;
  bool hasFeatureJAS;
  bool hasFeatureAR;
  bool hasFeatureLLAC;
  bool hasFeatureMETA;
  bool hasFeatureMinBitrate;
  bool hasFeatureLARC;
  bool hasFeatureLHDCV4;
} tA2DP_LHDCV3_SINK_CIE;

/* LHDC Sink codec capabilities */
static const tA2DP_LHDCV3_SINK_CIE a2dp_lhdcv3_sink_caps = {
    A2DP_LHDC_VENDOR_ID,  // vendorId
    A2DP_LHDCV3_CODEC_ID,   // codecId
    // sampleRate
    //(A2DP_LHDC_SAMPLING_FREQ_48000),
    (A2DP_LHDC_SAMPLING_FREQ_44100 | A2DP_LHDC_SAMPLING_FREQ_48000 | A2DP_LHDC_SAMPLING_FREQ_96000),
    (A2DP_LHDC_SAMPLING_FREQ_48000),
    // bits_per_sample
    (BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16 | BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24),
    //Channel Separation
    //A2DP_LHDC_CH_SPLIT_NONE | A2DP_LHDC_CH_SPLIT_TWS,
	  A2DP_LHDC_CH_SPLIT_NONE,
    //Version number
    A2DP_LHDC_VER3,
    //Target bit Rate
    A2DP_LHDC_MAX_BIT_RATE_900K,
    //LL supported ?
    true,

    /*******************************
     *  LHDC features/capabilities:
     *  hasFeatureJAS
     *  hasFeatureAR
     *  hasFeatureLLAC
     *  hasFeatureMETA
     *  hasFeatureMinBitrate
     *  hasFeatureLARC
     *  hasFeatureLHDCV4
     *******************************/
    //bool hasFeatureJAS;
    false,

    //bool hasFeatureAR;
    false,

    //bool hasFeatureLLAC;
    true,

    //bool hasFeatureMETA;
    false,

    //bool hasFeatureMinBitrate;
    true,

    //bool hasFeatureLARC;
    false,

    //bool hasFeatureLHDCV4;
    true,
};

/* Default LHDC codec configuration */
static const tA2DP_LHDCV3_SINK_CIE a2dp_lhdcv3_sink_default_config = {
    A2DP_LHDC_VENDOR_ID,                // vendorId
    A2DP_LHDCV3_CODEC_ID,                 // codecId
    A2DP_LHDC_SAMPLING_FREQ_48000,      // sampleRate
    A2DP_LHDC_SAMPLING_FREQ_48000,      // LLAC default best sampleRate
    BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24,  // bits_per_sample
    A2DP_LHDC_CH_SPLIT_NONE,
    A2DP_LHDC_VER3,
    A2DP_LHDC_MAX_BIT_RATE_900K,
    //LL supported ?
    true,

    //bool hasFeatureJAS;
    false,

    //bool hasFeatureAR;
    false,

    //bool hasFeatureLLAC;
    true,

    //bool hasFeatureMETA;
    false,

    //bool hasFeatureMinBitrate;
    true,

    //bool hasFeatureLARC;
    false,

    //bool hasFeatureLHDCV4;
    true,
};

static const tA2DP_DECODER_INTERFACE a2dp_decoder_interface_lhdcv3 = {
    a2dp_vendor_lhdcv3_decoder_init,
    a2dp_vendor_lhdcv3_decoder_cleanup,
    a2dp_vendor_lhdcv3_decoder_decode_packet,
    a2dp_vendor_lhdcv3_decoder_start,
    a2dp_vendor_lhdcv3_decoder_suspend,
    a2dp_vendor_lhdcv3_decoder_configure,
};

static std::string lhdcV3_QualityModeBitRate_toString(uint32_t value) {
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
    return "LOW 1 (128 Kbps)";
  case A2DP_LHDC_QUALITY_LOW0:
    return "LOW 0 (64 Kbps)";
  default:
    return "Unknown Bit Rate Mode";
  }
}

static tA2DP_STATUS A2DP_CodecInfoMatchesCapabilityLhdcV3Sink(
    const tA2DP_LHDCV3_SINK_CIE* p_cap, const uint8_t* p_codec_info,
    bool is_capability);


// Builds the LHDC Media Codec Capabilities byte sequence beginning from the
// LOSC octet. |media_type| is the media type |AVDT_MEDIA_TYPE_*|.
// |p_ie| is a pointer to the LHDC Codec Information Element information.
// The result is stored in |p_result|. Returns A2DP_SUCCESS on success,
// otherwise the corresponding A2DP error status code.
static tA2DP_STATUS A2DP_BuildInfoLhdcV3Sink(uint8_t media_type,
                                       const tA2DP_LHDCV3_SINK_CIE* p_ie,
                                       uint8_t* p_result) {

  const uint8_t* tmpInfo = p_result;
  if (p_ie == NULL || p_result == NULL) {
    return A2DP_INVALID_CODEC_PARAMETER;
  }

  *p_result++ = A2DP_LHDCV3_CODEC_LEN;    //0
  *p_result++ = (media_type << 4);      //1
  *p_result++ = A2DP_MEDIA_CT_NON_A2DP; //2

  // Vendor ID and Codec ID
  *p_result++ = (uint8_t)(p_ie->vendorId & 0x000000FF); //3
  *p_result++ = (uint8_t)((p_ie->vendorId & 0x0000FF00) >> 8);  //4
  *p_result++ = (uint8_t)((p_ie->vendorId & 0x00FF0000) >> 16); //5
  *p_result++ = (uint8_t)((p_ie->vendorId & 0xFF000000) >> 24); //6
  *p_result++ = (uint8_t)(p_ie->codecId & 0x00FF);  //7
  *p_result++ = (uint8_t)((p_ie->codecId & 0xFF00) >> 8);   //8

  // Sampling Frequency & Bits per sample
  uint8_t para = 0;

  // sample rate bit0 ~ bit2
  para = (uint8_t)(p_ie->sampleRate & A2DP_LHDC_SAMPLING_FREQ_MASK);

  if (p_ie->bits_per_sample == (BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24 | BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16)) {
      para = para | (A2DP_LHDC_BIT_FMT_24 | A2DP_LHDC_BIT_FMT_16);
  }else if(p_ie->bits_per_sample == BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24){
      para = para | A2DP_LHDC_BIT_FMT_24;
  }else if(p_ie->bits_per_sample == BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16){
      para = para | A2DP_LHDC_BIT_FMT_16;
  }

  if (p_ie->hasFeatureJAS)
  {
    para |= A2DP_LHDC_FEATURE_JAS;
  }

  if (p_ie->hasFeatureAR)
  {
    para |= A2DP_LHDC_FEATURE_AR;
  }

  // Save octet 9
  *p_result++ = para;   //9

  para = p_ie->version;

  para |= p_ie->maxTargetBitrate;

  para |= p_ie->isLLSupported ? A2DP_LHDC_LL_SUPPORTED : A2DP_LHDC_LL_NONE;

  if (p_ie->hasFeatureLLAC)
  {
    para |= A2DP_LHDC_FEATURE_LLAC;
  }

  // Save octet 10
  *p_result++ = para;   //a

  //Save octet 11
  para = p_ie->channelSplitMode;

  if (p_ie->hasFeatureMETA)
  {
    para |= A2DP_LHDC_FEATURE_META;
  }

  if (p_ie->hasFeatureMinBitrate)
  {
    para |= A2DP_LHDC_FEATURE_MIN_BR;
  }

  if (p_ie->hasFeatureLARC)
  {
    para |= A2DP_LHDC_FEATURE_LARC;
  }

  if (p_ie->hasFeatureLHDCV4)
  {
    para |= A2DP_LHDC_FEATURE_LHDCV4;
  }

  *p_result++ = para;   //b

  //Save octet 12
  //para = p_ie->supportedBitrate;
  //*p_result++ = para;   //c

  log::info(": Info build result = [0]:0x{:02x}, [1]:0x{:02x}, [2]:0x{:02x}, [3]:0x{:02x}, "
                     "[4]:0x{:02x}, [5]:0x{:02x}, [6]:0x{:02x}, [7]:0x{:02x}, [8]:0x{:02x}, [9]:0x{:02x}, [10]:0x{:02x}, [11]:0x{:02x}",
      tmpInfo[0], tmpInfo[1], tmpInfo[2], tmpInfo[3],
                    tmpInfo[4], tmpInfo[5], tmpInfo[6], tmpInfo[7], tmpInfo[8], tmpInfo[9], tmpInfo[10], tmpInfo[11]);
  return A2DP_SUCCESS;
}

// Parses the LHDC Media Codec Capabilities byte sequence beginning from the
// LOSC octet. The result is stored in |p_ie|. The byte sequence to parse is
// |p_codec_info|. If |is_capability| is true, the byte sequence is
// codec capabilities, otherwise is codec configuration.
// Returns A2DP_SUCCESS on success, otherwise the corresponding A2DP error
// status code.
static tA2DP_STATUS A2DP_ParseInfoLhdcV3Sink(tA2DP_LHDCV3_SINK_CIE* p_ie,
                                       const uint8_t* p_codec_info,
                                       bool is_capability) {
  uint8_t losc;
  uint8_t media_type;
  tA2DP_CODEC_TYPE codec_type;
  const uint8_t* tmpInfo = p_codec_info;
  const uint8_t* p_codec_Info_save = p_codec_info;

  //log::info(": p_ie = {}, p_codec_info = {}", p_ie, p_codec_info);
  if (p_ie == NULL || p_codec_info == NULL) return A2DP_INVALID_CODEC_PARAMETER;

  // Check the codec capability length
  losc = *p_codec_info++;

  if (losc != A2DP_LHDCV3_CODEC_LEN) return AVDTP_UNSUPPORTED_CONFIGURATION;

  media_type = (*p_codec_info++) >> 4;
  codec_type = static_cast<tA2DP_CODEC_TYPE>(*p_codec_info++);
    //log::info(": media_type = {}, codec_type = {}", media_type, codec_type);
  /* Check the Media Type and Media Codec Type */
  if (media_type != AVDT_MEDIA_TYPE_AUDIO ||
      codec_type != A2DP_MEDIA_CT_NON_A2DP) {
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  // Check the Vendor ID and Codec ID */
  p_ie->vendorId = (*p_codec_info & 0x000000FF) |
                   (*(p_codec_info + 1) << 8 & 0x0000FF00) |
                   (*(p_codec_info + 2) << 16 & 0x00FF0000) |
                   (*(p_codec_info + 3) << 24 & 0xFF000000);
  p_codec_info += 4;
  p_ie->codecId =
      (*p_codec_info & 0x00FF) | (*(p_codec_info + 1) << 8 & 0xFF00);
  p_codec_info += 2;
  log::info(":Vendor(0x{:02x}), Codec(0x{:02x})", p_ie->vendorId, p_ie->codecId);
  if (p_ie->vendorId != A2DP_LHDC_VENDOR_ID ||
      p_ie->codecId != A2DP_LHDCV3_CODEC_ID) {
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  p_ie->sampleRate = *p_codec_info & A2DP_LHDC_SAMPLING_FREQ_MASK;
  if ((*p_codec_info & A2DP_LHDC_BIT_FMT_MASK) == 0) {
    return AVDTP_UNSUPPORTED_CONFIGURATION;
  }

  p_ie->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
  if (*p_codec_info & A2DP_LHDC_BIT_FMT_24)
    p_ie->bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
  if (*p_codec_info & A2DP_LHDC_BIT_FMT_16) {
    p_ie->bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
  }

  p_ie->hasFeatureJAS = ((*p_codec_info & A2DP_LHDC_FEATURE_JAS) != 0) ? true : false;

  p_ie->hasFeatureAR = ((*p_codec_info & A2DP_LHDC_FEATURE_AR) != 0) ? true : false;

  p_codec_info += 1;

  p_ie->version = (*p_codec_info) & A2DP_LHDC_VERSION_MASK;
  //p_ie->version = 1;

  p_ie->maxTargetBitrate = (*p_codec_info) & A2DP_LHDC_MAX_BIT_RATE_MASK;
  //p_ie->maxTargetBitrate = A2DP_LHDC_MAX_BIT_RATE_900K;

  p_ie->isLLSupported = ((*p_codec_info & A2DP_LHDC_LL_MASK) != 0)? true : false;
  //p_ie->isLLSupported = false;

  p_ie->hasFeatureLLAC = ((*p_codec_info & A2DP_LHDC_FEATURE_LLAC) != 0) ? true : false;

  p_codec_info += 1;

  p_ie->channelSplitMode = (*p_codec_info) & A2DP_LHDC_CH_SPLIT_MSK;

  p_ie->hasFeatureMETA = ((*p_codec_info & A2DP_LHDC_FEATURE_META) != 0) ? true : false;

  p_ie->hasFeatureMinBitrate = ((*p_codec_info & A2DP_LHDC_FEATURE_MIN_BR) != 0) ? true : false;

  p_ie->hasFeatureLARC = ((*p_codec_info & A2DP_LHDC_FEATURE_LARC) != 0) ? true : false;

  p_ie->hasFeatureLHDCV4 = ((*p_codec_info & A2DP_LHDC_FEATURE_LHDCV4) != 0) ? true : false;

  log::info( ":Has LL({}) JAS({}) AR({}) META({}) LLAC({}) MBR({}) LARC({}) V4({})",
      p_ie->isLLSupported,
      p_ie->hasFeatureJAS,
      p_ie->hasFeatureAR,
      p_ie->hasFeatureMETA,
      p_ie->hasFeatureLLAC,
      p_ie->hasFeatureMinBitrate,
      p_ie->hasFeatureLARC,
      p_ie->hasFeatureLHDCV4);

  log::info( ": codec info = [0]:0x{:02x}, [1]:0x{:02x}, [2]:0x{:02x}, [3]:0x{:02x}, [4]:0x{:02x}, [5]:0x{:02x}, [6]:0x{:02x}, [7]:0x{:02x}, [8]:0x{:02x}, [9]:0x{:02x}, [10]:0x{:02x}, [11]:0x{:02x}",
             tmpInfo[0], tmpInfo[1], tmpInfo[2], tmpInfo[3], tmpInfo[4], tmpInfo[5], tmpInfo[6],
                        tmpInfo[7], tmpInfo[8], tmpInfo[9], tmpInfo[10], tmpInfo[11]);

  if (is_capability) return A2DP_SUCCESS;

  if (A2DP_BitsSet(p_ie->sampleRate) != A2DP_SET_ONE_BIT)
    return A2DP_INVALID_SAMPLING_FREQUENCY;

  save_codec_info (p_codec_Info_save);

  return A2DP_SUCCESS;
}

//
// Selects the best sample rate from |sampleRate|.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_sample_rate(uint8_t sampleRate,
                                    tA2DP_LHDCV3_SINK_CIE* p_result,
                                    btav_a2dp_codec_config_t* p_codec_config) {
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000) {
    p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_96000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    return true;
  }
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200) {
    p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_88200;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_88200;
    return true;
  }
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000) {
    p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_48000;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    return true;
  }
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100) {
    p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_44100;
    p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
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
    tA2DP_LHDCV3_SINK_CIE* p_result, btav_a2dp_codec_config_t* p_codec_config) {
  switch (p_codec_audio_config->sample_rate) {
    case BTAV_A2DP_CODEC_SAMPLE_RATE_44100:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100) {
        p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_44100;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_48000:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000) {
        p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_48000;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_88200:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200) {
        p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_88200;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_88200;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_96000:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000) {
        p_result->sampleRate = A2DP_LHDC_SAMPLING_FREQ_96000;
        p_codec_config->sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_16000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_24000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_176400:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_192000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_NONE:
      break;
  }
  return false;
}

//
// Selects the best bits per sample from |bits_per_sample|.
// |bits_per_sample| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_best_bits_per_sample(
    btav_a2dp_codec_bits_per_sample_t bits_per_sample, tA2DP_LHDCV3_SINK_CIE* p_result,
    btav_a2dp_codec_config_t* p_codec_config) {
  if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
    p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
    p_result->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
    return true;
  }
  if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16) {
    p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
    p_result->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
    return true;
  }
  return false;
}

//
// Selects the audio bits per sample from |p_codec_audio_config|.
// |bits_per_sample| contains the capability.
// The result is stored in |p_result| and |p_codec_config|.
// Returns true if a selection was made, otherwise false.
//
static bool select_audio_bits_per_sample(
    const btav_a2dp_codec_config_t* p_codec_audio_config,
    btav_a2dp_codec_bits_per_sample_t bits_per_sample, tA2DP_LHDCV3_SINK_CIE* p_result,
    btav_a2dp_codec_config_t* p_codec_config) {
  switch (p_codec_audio_config->bits_per_sample) {
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
      if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16) {
        p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
        p_result->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
      if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
        p_codec_config->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
        p_result->bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
        return true;
      }
      break;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32:
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE:
      break;
  }
  return false;
}

static std::string lhdcV3_sampleRate_toString(uint8_t value) {
  switch((int)value)
  {
  case A2DP_LHDC_SAMPLING_FREQ_44100:
    return "44100";
  case A2DP_LHDC_SAMPLING_FREQ_48000:
    return "48000";
  case A2DP_LHDC_SAMPLING_FREQ_96000:
    return "96000";
  default:
    return "Unknown Sample Rate";
  }
}

static std::string lhdcV3_bitPerSample_toString(uint8_t value) {
  switch((int)value)
  {
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
    return "16";
  case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
    return "24";
  default:
    return "Unknown Bit Per Sample";
  }
}


static bool A2DP_IsFeatureInUserConfigLhdcV3(tA2DP_CODEC_CONFIGS_PACK* cfgsPtr, uint8_t featureCode)
{
  switch(featureCode)
  {
    case LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_JAS.inSpecBank, A2DP_LHDC_JAS_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_AR.inSpecBank, A2DP_LHDC_AR_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_META.inSpecBank, A2DP_LHDC_META_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_LLAC.inSpecBank, A2DP_LHDC_LLAC_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_MBR.inSpecBank, A2DP_LHDC_MBR_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_LARC_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_LARC.inSpecBank, A2DP_LHDC_LARC_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_user_config_, a2dp_lhdc_source_caps_LHDCV4.inSpecBank, A2DP_LHDC_V4_ENABLED);
      }
      break;

  default:
    break;
  }

  return false;
}
static bool A2DP_IsFeatureInCodecConfigLhdcV3(tA2DP_CODEC_CONFIGS_PACK* cfgsPtr, uint8_t featureCode)
{
  switch(featureCode)
  {
    case LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_JAS.inSpecBank, A2DP_LHDC_JAS_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_AR.inSpecBank, A2DP_LHDC_AR_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_META.inSpecBank, A2DP_LHDC_META_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_LLAC.inSpecBank, A2DP_LHDC_LLAC_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_MBR.inSpecBank, A2DP_LHDC_MBR_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_LARC_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_LARC.inSpecBank, A2DP_LHDC_LARC_ENABLED);
      }
      break;
    case LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE:
      {
        CHECK_IN_A2DP_SPEC(cfgsPtr->_codec_config_, a2dp_lhdc_source_caps_LHDCV4.inSpecBank, A2DP_LHDC_V4_ENABLED);
      }
      break;

  default:
    break;
  }

  return false;
}

static void A2DP_UpdateFeatureToSpecLhdcV3(tA2DP_CODEC_CONFIGS_PACK* cfgsPtr,
    uint16_t toCodecCfg, bool hasFeature, uint8_t toSpec, int64_t value)
{
  if(toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_CONFIG_)
  {
    SETUP_A2DP_SPEC(cfgsPtr->_codec_config_, toSpec, hasFeature, value);
  }
  if(toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_CAP_)
  {
    SETUP_A2DP_SPEC(cfgsPtr->_codec_capability_, toSpec, hasFeature, value);
  }
  if(toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_LOCAL_CAP_)
  {
    SETUP_A2DP_SPEC(cfgsPtr->_codec_local_capability_, toSpec, hasFeature, value);
  }
  if(toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_)
  {
    SETUP_A2DP_SPEC(cfgsPtr->_codec_selectable_capability_, toSpec, hasFeature, value);
  }
  if(toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_USER_)
  {
    SETUP_A2DP_SPEC(cfgsPtr->_codec_user_config_, toSpec, hasFeature, value);
  }
  if(toCodecCfg & A2DP_LHDC_TO_A2DP_CODEC_AUDIO_)
  {
    SETUP_A2DP_SPEC(cfgsPtr->_codec_audio_config_, toSpec, hasFeature, value);
  }
}

static void A2DP_UpdateFeatureToA2dpConfigLhdcV3(tA2DP_CODEC_CONFIGS_PACK *cfgsPtr,
    uint8_t featureCode,  uint16_t toCodecCfg, bool hasFeature)
{

  //log::info( ": featureCode:0x{} toCfgs:0x{}, toSet:{}", featureCode, toCodecCfg, hasFeature);

  switch(featureCode)
  {
  case LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_JAS.inSpecBank, A2DP_LHDC_JAS_ENABLED);
    break;
  case LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_AR.inSpecBank, A2DP_LHDC_AR_ENABLED);
    break;
  case LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_META.inSpecBank, A2DP_LHDC_META_ENABLED);
    break;
  case LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_LLAC.inSpecBank, A2DP_LHDC_LLAC_ENABLED);
    break;
  case LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_MBR.inSpecBank, A2DP_LHDC_MBR_ENABLED);
    break;
  case LHDC_EXTEND_FUNC_A2DP_LHDC_LARC_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_LARC.inSpecBank, A2DP_LHDC_LARC_ENABLED);
    break;
  case LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE:
    A2DP_UpdateFeatureToSpecLhdcV3(cfgsPtr, toCodecCfg, hasFeature,
        a2dp_lhdc_source_caps_LHDCV4.inSpecBank, A2DP_LHDC_V4_ENABLED);
    break;

  default:
    break;
  }
}


static uint32_t A2DP_MaxBitRatetoQualityLevelLhdcV3(uint8_t maxTargetBitrate)
{
  switch (maxTargetBitrate & A2DP_LHDC_MAX_BIT_RATE_MASK)
  {
  case A2DP_LHDC_MAX_BIT_RATE_900K:
    return A2DP_LHDC_QUALITY_HIGH;
  case A2DP_LHDC_MAX_BIT_RATE_500K:
    return A2DP_LHDC_QUALITY_MID;
  case A2DP_LHDC_MAX_BIT_RATE_400K:
    return A2DP_LHDC_QUALITY_LOW;
  default:
    return (0xFF);
  }
}

const char* A2DP_VendorCodecNameLhdcV3Sink(UNUSED_ATTR const uint8_t* p_codec_info) {
  return "LHDC V3 Sink";
}

bool A2DP_IsVendorSinkCodecValidLhdcV3(const uint8_t* p_codec_info) {
  tA2DP_LHDCV3_SINK_CIE cfg_cie;

  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoLhdcV3Sink(&cfg_cie, p_codec_info, false) == A2DP_SUCCESS) ||
         (A2DP_ParseInfoLhdcV3Sink(&cfg_cie, p_codec_info, true) == A2DP_SUCCESS);
}


bool A2DP_IsVendorPeerSourceCodecValidLhdcV3(const uint8_t* p_codec_info) {
  tA2DP_LHDCV3_SINK_CIE cfg_cie;

  /* Use a liberal check when parsing the codec info */
  return (A2DP_ParseInfoLhdcV3Sink(&cfg_cie, p_codec_info, false) == A2DP_SUCCESS) ||
         (A2DP_ParseInfoLhdcV3Sink(&cfg_cie, p_codec_info, true) == A2DP_SUCCESS);
}


tA2DP_STATUS A2DP_IsVendorSinkCodecSupportedLhdcV3(const uint8_t* p_codec_info) {
  return A2DP_CodecInfoMatchesCapabilityLhdcV3Sink(&a2dp_lhdcv3_sink_caps, p_codec_info, false);
}

bool A2DP_IsPeerSourceCodecSupportedLhdcV3(const uint8_t* p_codec_info) {
  return (A2DP_CodecInfoMatchesCapabilityLhdcV3Sink(&a2dp_lhdcv3_sink_caps, p_codec_info,
                                             true) == A2DP_SUCCESS);
}

void A2DP_InitDefaultCodecLhdcV3Sink(uint8_t* p_codec_info) {
  log::info(": enter");
  if (A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &a2dp_lhdcv3_sink_default_config,
                        p_codec_info) != A2DP_SUCCESS) {
    log::error(": A2DP_BuildInfoSbc failed");
  }
}

// Checks whether A2DP SBC codec configuration matches with a device's codec
// capabilities. |p_cap| is the SBC codec configuration. |p_codec_info| is
// the device's codec capabilities. |is_capability| is true if
// |p_codec_info| contains A2DP codec capability.
// Returns A2DP_SUCCESS if the codec configuration matches with capabilities,
// otherwise the corresponding A2DP error status code.
static tA2DP_STATUS A2DP_CodecInfoMatchesCapabilityLhdcV3Sink(
    const tA2DP_LHDCV3_SINK_CIE* p_cap, const uint8_t* p_codec_info,
    bool is_capability) {
  tA2DP_STATUS status;
  tA2DP_LHDCV3_SINK_CIE cfg_cie;

  /* parse configuration */
  status = A2DP_ParseInfoLhdcV3Sink(&cfg_cie, p_codec_info, is_capability);
  if (status != A2DP_SUCCESS) {
    log::error(": parsing failed {}", status);
    return status;
  }

  /* verify that each parameter is in range */

  log::info(": FREQ peer: 0x{:02x}, capability 0x{:02x}",
            cfg_cie.sampleRate, p_cap->sampleRate);

  log::info(": BIT_FMT peer: 0x{:02x}, capability 0x{:02x}",
            cfg_cie.bits_per_sample, p_cap->bits_per_sample);

  /* sampling frequency */
  if ((cfg_cie.sampleRate & p_cap->sampleRate) == 0) return A2DP_NOT_SUPPORTED_SAMPLING_FREQUENCY;

  /* bit per sample */
  if ((cfg_cie.bits_per_sample & p_cap->bits_per_sample) == 0) return A2DP_NOT_SUPPORTED_CHANNEL_MODE;

  return A2DP_SUCCESS;
}

bool A2DP_VendorCodecTypeEqualsLhdcV3Sink(const uint8_t* p_codec_info_a,
                                    const uint8_t* p_codec_info_b) {
  tA2DP_LHDCV3_SINK_CIE lhdc_cie_a;
  tA2DP_LHDCV3_SINK_CIE lhdc_cie_b;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status =
      A2DP_ParseInfoLhdcV3Sink(&lhdc_cie_a, p_codec_info_a, true);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return false;
  }
  a2dp_status = A2DP_ParseInfoLhdcV3Sink(&lhdc_cie_b, p_codec_info_b, true);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return false;
  }

  return true;
}

bool A2DP_VendorCodecEqualsLhdcV3Sink(const uint8_t* p_codec_info_a,
                                const uint8_t* p_codec_info_b) {
  tA2DP_LHDCV3_SINK_CIE lhdc_cie_a;
  tA2DP_LHDCV3_SINK_CIE lhdc_cie_b;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status =
      A2DP_ParseInfoLhdcV3Sink(&lhdc_cie_a, p_codec_info_a, true);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return false;
  }
  a2dp_status = A2DP_ParseInfoLhdcV3Sink(&lhdc_cie_b, p_codec_info_b, true);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return false;
  }

  return (lhdc_cie_a.sampleRate == lhdc_cie_b.sampleRate) &&
         (lhdc_cie_a.bits_per_sample == lhdc_cie_b.bits_per_sample) &&
         /*(lhdc_cie_a.supportedBitrate == lhdc_cie_b.supportedBitrate) &&*/
         (lhdc_cie_a.isLLSupported == lhdc_cie_b.isLLSupported);
}


int A2DP_VendorGetTrackSampleRateLhdcV3Sink(const uint8_t* p_codec_info) {
  tA2DP_LHDCV3_SINK_CIE lhdc_cie;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoLhdcV3Sink(&lhdc_cie, p_codec_info, false);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return -1;
  }

  switch (lhdc_cie.sampleRate) {
    case A2DP_LHDC_SAMPLING_FREQ_44100:
      return 44100;
    case A2DP_LHDC_SAMPLING_FREQ_48000:
      return 48000;
    case A2DP_LHDC_SAMPLING_FREQ_88200:
      return 88200;
    case A2DP_LHDC_SAMPLING_FREQ_96000:
      return 96000;
  }

  return -1;
}

int A2DP_VendorGetSinkTrackChannelTypeLhdcV3(const uint8_t* p_codec_info) {
  tA2DP_LHDCV3_SINK_CIE lhdc_cie;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoLhdcV3Sink(&lhdc_cie, p_codec_info, false);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return -1;
  }

  return A2DP_LHDC_CHANNEL_MODE_STEREO;
}

int A2DP_VendorGetChannelModeCodeLhdcV3Sink(const uint8_t* p_codec_info) {
  tA2DP_LHDCV3_SINK_CIE lhdc_cie;

  // Check whether the codec info contains valid data
  tA2DP_STATUS a2dp_status = A2DP_ParseInfoLhdcV3Sink(&lhdc_cie, p_codec_info, false);
  if (a2dp_status != A2DP_SUCCESS) {
    log::error(": cannot decode codec information: {}",
              a2dp_status);
    return -1;
  }
  return A2DP_LHDC_CHANNEL_MODE_STEREO;
}

bool A2DP_VendorGetPacketTimestampLhdcV3Sink(UNUSED_ATTR const uint8_t* p_codec_info,
                                       const uint8_t* p_data,
                                       uint32_t* p_timestamp) {
  // TODO: Is this function really codec-specific?
  *p_timestamp = *(const uint32_t*)p_data;
  return true;
}
/*
bool A2DP_VendorBuildCodecHeaderLhdcV3Sink(UNUSED_ATTR const uint8_t* p_codec_info,
                              BT_HDR* p_buf, uint16_t frames_per_packet) {
  uint8_t* p;

  p_buf->offset -= A2DP_SBC_MPL_HDR_LEN;
  p = (uint8_t*)(p_buf + 1) + p_buf->offset;
  p_buf->len += A2DP_SBC_MPL_HDR_LEN;
  A2DP_BuildMediaPayloadHeaderSbc(p, false, false, false,
                                  (uint8_t)frames_per_packet);

  return true;
}
*/
std::string A2DP_VendorCodecInfoStringLhdcV3Sink(const uint8_t* p_codec_info) {
  std::stringstream res;
  std::string field;
  tA2DP_STATUS a2dp_status;
  tA2DP_LHDCV3_SINK_CIE lhdc_cie;

  a2dp_status = A2DP_ParseInfoLhdcV3Sink(&lhdc_cie, p_codec_info, true);
  if (a2dp_status != A2DP_SUCCESS) {
    res << "A2DP_ParseInfoLhdcV3Sink fail: "
        << loghex(static_cast<uint8_t>(a2dp_status));
    return res.str();
  }

  res << "\tname: LHDC\n";

  // Sample frequency
  field.clear();
  AppendField(&field, (lhdc_cie.sampleRate == 0), "NONE");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100),
              "44100");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000),
              "48000");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200),
              "88200");
  AppendField(&field, (lhdc_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000),
              "96000");
  res << "\tsamp_freq: " << field << " (" << loghex(lhdc_cie.sampleRate)
      << ")\n";

  // Channel mode
  field.clear();
  AppendField(&field, 1,
             "Stereo");
  res << "\tch_mode: " << field << " (" << "Only support stereo."
      << ")\n";

  // bits per sample
  field.clear();
  AppendField(&field, (lhdc_cie.bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16),
              "16");
  AppendField(&field, (lhdc_cie.bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24),
              "24");
  res << "\tbits_depth: " << field << " bits (" << loghex((int)lhdc_cie.bits_per_sample)
      << ")\n";

  // Max data rate...
  field.clear();
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDC_MAX_BIT_RATE_MASK) == A2DP_LHDC_MAX_BIT_RATE_900K),
              "900Kbps");
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDC_MAX_BIT_RATE_MASK) == A2DP_LHDC_MAX_BIT_RATE_500K),
              "500Kbps");
  AppendField(&field, ((lhdc_cie.maxTargetBitrate & A2DP_LHDC_MAX_BIT_RATE_MASK) == A2DP_LHDC_MAX_BIT_RATE_400K),
              "400Kbps");
  res << "\tMax target-rate: " << field << " (" << loghex((lhdc_cie.maxTargetBitrate & A2DP_LHDC_MAX_BIT_RATE_MASK))
      << ")\n";

  // Version
  field.clear();
  AppendField(&field, (lhdc_cie.version == A2DP_LHDC_VER3),
              "LHDC V3");
  res << "\tversion: " << field << " (" << loghex(lhdc_cie.version)
      << ")\n";


  /*
  field.clear();
  AppendField(&field, 0, "NONE");
  AppendField(&field, 0,
              "Mono");
  AppendField(&field, 0,
              "Dual");
  AppendField(&field, 1,
              "Stereo");
  res << "\tch_mode: " << field << " (" << loghex(lhdc_cie.channelMode)
      << ")\n";
*/
  return res.str();
}

const tA2DP_DECODER_INTERFACE* A2DP_VendorGetDecoderInterfaceLhdcV3(
    const uint8_t* p_codec_info) {
  if (!A2DP_IsVendorSinkCodecValidLhdcV3(p_codec_info)) return NULL;

  return &a2dp_decoder_interface_lhdcv3;
}

bool A2DP_VendorAdjustCodecLhdcV3Sink(uint8_t* p_codec_info) {
  tA2DP_LHDCV3_SINK_CIE cfg_cie;

  // Nothing to do: just verify the codec info is valid
  if (A2DP_ParseInfoLhdcV3Sink(&cfg_cie, p_codec_info, true) != A2DP_SUCCESS)
    return false;

  return true;
}

btav_a2dp_codec_index_t A2DP_VendorSinkCodecIndexLhdcV3(
    UNUSED_ATTR const uint8_t* p_codec_info) {
  return BTAV_A2DP_CODEC_INDEX_SINK_LHDCV3;
}

const char* A2DP_VendorCodecIndexStrLhdcV3Sink(void) { return "LHDC V3 SINK"; }

bool A2DP_VendorInitCodecConfigLhdcV3Sink(AvdtpSepConfig* p_cfg) {
  log::info(": enter");
  if (A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &a2dp_lhdcv3_sink_caps,
                        p_cfg->codec_info) != A2DP_SUCCESS) {
    return false;
  }

  return true;
}

UNUSED_ATTR static void build_codec_config(const tA2DP_LHDCV3_SINK_CIE& config_cie,
                                           btav_a2dp_codec_config_t* result) {
  if (config_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  if (config_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  if (config_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_88200;
  if (config_cie.sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000)
    result->sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;

  result->bits_per_sample = config_cie.bits_per_sample;

  result->channel_mode |= BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
}





A2dpCodecConfigLhdcV3Sink::A2dpCodecConfigLhdcV3Sink(
    btav_a2dp_codec_priority_t codec_priority)
    : A2dpCodecConfigLhdcV3Base(BTAV_A2DP_CODEC_INDEX_SINK_LHDCV3,
                             A2DP_VendorCodecIndexStrLhdcV3Sink(), codec_priority,
                             false) {}

A2dpCodecConfigLhdcV3Sink::~A2dpCodecConfigLhdcV3Sink() {}

bool A2dpCodecConfigLhdcV3Sink::init() {
  // Load the decoder
  if (!A2DP_VendorLoadDecoderLhdcV3()) {
    log::error(": cannot load the decoder");
    return false;
  }

  return true;
}

bool A2dpCodecConfigLhdcV3Sink::useRtpHeaderMarkerBit() const {
  // TODO: This method applies only to Source codecs
  return false;
}

#if 0
bool A2dpCodecConfigLhdcV3Sink::updateEncoderUserConfig(
    UNUSED_ATTR const tA2DP_ENCODER_INIT_PEER_PARAMS* p_peer_params,
    UNUSED_ATTR bool* p_restart_input, UNUSED_ATTR bool* p_restart_output,
    UNUSED_ATTR bool* p_config_updated) {
  // TODO: This method applies only to Source codecs
  return false;
}
#endif

#if 0
uint64_t A2dpCodecConfigLhdcV3Sink::encoderIntervalMs() const {
  // TODO: This method applies only to Source codecs
  return 0;
}
#endif

#if 0
int A2dpCodecConfigLhdcV3Sink::getEffectiveMtu() const {
  // TODO: This method applies only to Source codecs
  return 0;
}
#endif

tA2DP_STATUS A2dpCodecConfigLhdcV3Base::setCodecConfig(const uint8_t* p_peer_codec_info, bool is_capability,
                      uint8_t* p_result_codec_config) {
  std::lock_guard<std::recursive_mutex> lock(codec_mutex_);
  is_source_ = false;
  tA2DP_LHDCV3_SINK_CIE sink_info_cie;
  tA2DP_LHDCV3_SINK_CIE result_config_cie;
  uint8_t sampleRate;
  bool isLLEnabled;
  bool hasFeature = false;
  bool hasUserSet = false;
  uint32_t quality_mode, maxBitRate_Qmode;
  //uint8_t supportedBitrate;
  btav_a2dp_codec_bits_per_sample_t bits_per_sample;

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

  tA2DP_STATUS status =
      A2DP_ParseInfoLhdcV3Sink(&sink_info_cie, p_peer_codec_info, is_capability);
  if (status != A2DP_SUCCESS) {
    log::error( ": can't parse peer's Sink capabilities: error = {}",
               status);
    goto fail;
  }

  //
  // Build the preferred configuration
  //
  memset(&result_config_cie, 0, sizeof(result_config_cie));
  result_config_cie.vendorId = a2dp_lhdcv3_sink_caps.vendorId;
  result_config_cie.codecId = a2dp_lhdcv3_sink_caps.codecId;


  log::info( ": incoming version: peer(0x{:02x}), host(0x{:02x})",
             sink_info_cie.version, a2dp_lhdcv3_sink_caps.version);

  // 2021/08/19: when sink's version is "V3_NotComapatible(version == A2DP_LHDC_VER6(0x8))",
  //        wrap it to A2DP_LHDC_VER3 to accept and treat as an A2DP_LHDC_VER3 device.
  if(sink_info_cie.version == A2DP_LHDC_VER6) {
    sink_info_cie.version = A2DP_LHDC_VER3;
    log::info( ": wrap V3_NotComapatible sink version to A2DP_LHDC_VER3");
  }

  if ((sink_info_cie.version & a2dp_lhdcv3_sink_caps.version) == 0) {
    log::error( ": Sink versoin unsupported! peer(0x{:02x}), host(0x{:02x})",
               sink_info_cie.version, a2dp_lhdcv3_sink_caps.version);
    goto fail;
  }
  result_config_cie.version = sink_info_cie.version;

  /*******************************************
   * Update Capabilities: LHDC Low Latency
   * to A2DP specifics 2
   *******************************************/
  isLLEnabled = (a2dp_lhdcv3_sink_caps.isLLSupported & sink_info_cie.isLLSupported);
  result_config_cie.isLLSupported = false;
  switch (codec_user_config_.codec_specific_2 & A2DP_LHDC_LL_ENABLED) {
    case A2DP_LHDC_LL_ENABLE:
    if (isLLEnabled) {
      result_config_cie.isLLSupported = true;
      codec_config_.codec_specific_2 |= A2DP_LHDC_LL_ENABLED;
    }
    break;
    case A2DP_LHDC_LL_DISABLE:
    if (!isLLEnabled) {
      result_config_cie.isLLSupported = false;
      codec_config_.codec_specific_2 &= ~A2DP_LHDC_LL_ENABLED;
    }
    break;
  }
  if (isLLEnabled) {
    codec_selectable_capability_.codec_specific_2 |= A2DP_LHDC_LL_ENABLED;
    codec_capability_.codec_specific_2 |= A2DP_LHDC_LL_ENABLED;
  }
  //result_config_cie.isLLSupported = sink_info_cie.isLLSupported;
  log::info( ": isLLSupported, Sink(0x{:02x}) Set(0x{:02x}), result(0x{:02x})",
                                sink_info_cie.isLLSupported,
                                (uint32_t)codec_user_config_.codec_specific_2,
                                result_config_cie.isLLSupported);

  //
  // Select the sample frequency
  //
  sampleRate = a2dp_lhdcv3_sink_caps.sampleRate & sink_info_cie.sampleRate;
  log::info(": sampleRate:peer:0x{:02x} local:0x{:02x} cap:0x{:02x} user:0x{:02x} ",
       sink_info_cie.sampleRate, a2dp_lhdcv3_sink_caps.sampleRate,
      sampleRate, codec_user_config_.sample_rate);

  codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
  switch (codec_user_config_.sample_rate) {
    case BTAV_A2DP_CODEC_SAMPLE_RATE_44100:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100) {
        result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_44100;
        codec_capability_.sample_rate = codec_user_config_.sample_rate;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_48000:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000) {
        result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_48000;
        codec_capability_.sample_rate = codec_user_config_.sample_rate;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_88200:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200) {
        result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_88200;
        codec_capability_.sample_rate = codec_user_config_.sample_rate;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_96000:
      if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000) {
        result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_96000;
        codec_capability_.sample_rate = codec_user_config_.sample_rate;
        codec_config_.sample_rate = codec_user_config_.sample_rate;
      }
      break;
    case BTAV_A2DP_CODEC_SAMPLE_RATE_16000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_24000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_176400:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_192000:
    case BTAV_A2DP_CODEC_SAMPLE_RATE_NONE:
      codec_capability_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
      codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_NONE;
      break;
  }

  // Select the sample frequency if there is no user preference
  do {
    // Compute the selectable capability
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100) {
      codec_selectable_capability_.sample_rate |=
          BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    }
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000) {
      codec_selectable_capability_.sample_rate |=
          BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    }
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200) {
      codec_selectable_capability_.sample_rate |=
          BTAV_A2DP_CODEC_SAMPLE_RATE_88200;
    }
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000) {
      codec_selectable_capability_.sample_rate |=
          BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
    }

    //Above Parts: if codec_config is setup successfully(ie., sampleRate in codec_user_config_ is valid), ignore following parts.
    if (codec_config_.sample_rate != BTAV_A2DP_CODEC_SAMPLE_RATE_NONE) {
      log::info( ": setup sample_rate:0x{:02x} from user_config", codec_config_.sample_rate);
      break;
    }
    //Below Parts: if codec_config is still not setup successfully, test default sample rate or use the best match

    // Compute the common capability
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_88200)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_88200;
    if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000)
      codec_capability_.sample_rate |= BTAV_A2DP_CODEC_SAMPLE_RATE_96000;

    // No user preference - try the codec audio config
    if (select_audio_sample_rate(&codec_audio_config_, sampleRate,
                                 &result_config_cie, &codec_config_)) {
      log::info( ": select audio sample rate:(0x{:02x})", result_config_cie.sampleRate);
      break;
    }

    // No user preference - try the default config
    if (sink_info_cie.hasFeatureLLAC) {
      if (select_best_sample_rate(
              a2dp_lhdcv3_sink_default_config.llac_sampleRate & sink_info_cie.sampleRate,
              &result_config_cie, &codec_config_)) {
      log::info( ": select best sample rate(LLAC default):0x{:02x}", result_config_cie.sampleRate);
        break;
      }
    } else {
      if (select_best_sample_rate(
              a2dp_lhdcv3_sink_default_config.sampleRate & sink_info_cie.sampleRate,
              &result_config_cie, &codec_config_)) {
        log::info( ": select best sample rate(LHDC default):0x{:02x}", result_config_cie.sampleRate);
        break;
      }
    }

    // No user preference - use the best match
    if (select_best_sample_rate(sampleRate, &result_config_cie,
                                &codec_config_)) {
      log::info( ": select best sample rate(best):0x{:02x}", result_config_cie.sampleRate);
      break;
    }
  } while (false);
  if (codec_config_.sample_rate == BTAV_A2DP_CODEC_SAMPLE_RATE_NONE) {
    log::error(
              ": cannot match sample frequency: source caps = 0x{:02x} "
              "sink info = 0x{:02x}",
               a2dp_lhdcv3_sink_caps.sampleRate, sink_info_cie.sampleRate);
    goto fail;
  }
  codec_user_config_.sample_rate = codec_config_.sample_rate;
  log::info( ": => sample rate(0x{:02x}) = {}",
      result_config_cie.sampleRate,
      lhdcV3_sampleRate_toString(result_config_cie.sampleRate).c_str());

  //
  // Select the bits per sample
  //
  // NOTE: this information is NOT included in the LHDC A2DP codec description
  // that is sent OTA.
  bits_per_sample = a2dp_lhdcv3_sink_caps.bits_per_sample & sink_info_cie.bits_per_sample;
  log::info(": bits_per_sample:peer:0x{:02x} local:0x{:02x} cap:0x{:02x} user:0x{:02x}",
       sink_info_cie.bits_per_sample, a2dp_lhdcv3_sink_caps.bits_per_sample,
      bits_per_sample, codec_user_config_.bits_per_sample);
  codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
  switch (codec_user_config_.bits_per_sample) {
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16:
      if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16) {
        result_config_cie.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
        codec_capability_.bits_per_sample = codec_user_config_.bits_per_sample;
        codec_config_.bits_per_sample = codec_user_config_.bits_per_sample;
      }
      break;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24:
      if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
        result_config_cie.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
        codec_capability_.bits_per_sample = codec_user_config_.bits_per_sample;
        codec_config_.bits_per_sample = codec_user_config_.bits_per_sample;
      }
      break;
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_32:
    case BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE:
      result_config_cie.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
      codec_capability_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
      codec_config_.bits_per_sample = BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE;
      break;
  }

  // Select the bits per sample if there is no user preference
  do {
    // Compute the selectable capability
      // Compute the selectable capability
      if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16)
        codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
      if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24)
        codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;

    if (codec_config_.bits_per_sample != BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE){
      log::info( ": setup bit_per_sample:0x{:02x} user_config", codec_config_.bits_per_sample);
      break;
    }

    // Compute the common capability
    if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16)
      codec_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
    if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24)
      codec_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;

    // No user preference - the the codec audio config
    if (select_audio_bits_per_sample(&codec_audio_config_, bits_per_sample,
                                     &result_config_cie, &codec_config_)) {
      log::info( ": select audio bits_per_sample:0x{:02x}", result_config_cie.bits_per_sample);
      break;
    }

    // No user preference - try the default config
    if (select_best_bits_per_sample(a2dp_lhdcv3_sink_default_config.bits_per_sample & sink_info_cie.bits_per_sample,
                                    &result_config_cie, &codec_config_)) {
      log::info( ": select best bits_per_sample(default):0x{:02x}", result_config_cie.bits_per_sample);
      break;
    }

    // No user preference - use the best match
    if (select_best_bits_per_sample(bits_per_sample,
                                    &result_config_cie, &codec_config_)) {
      log::info( ": select best bits_per_sample(best):0x{:02x}", result_config_cie.bits_per_sample);
      break;
    }
  } while (false);
  if (codec_config_.bits_per_sample == BTAV_A2DP_CODEC_BITS_PER_SAMPLE_NONE) {
    log::error(
              ": cannot match bits per sample: default = 0x{:02x} "
              "user preference = 0x{:02x}",
               a2dp_lhdcv3_sink_default_config.bits_per_sample,
              codec_user_config_.bits_per_sample);
    goto fail;
  }
  codec_user_config_.bits_per_sample = codec_config_.bits_per_sample;
  log::info( ": => bit per sample(0x{:02x}) = {}",
      result_config_cie.bits_per_sample,
      lhdcV3_bitPerSample_toString(result_config_cie.bits_per_sample).c_str());

  //
  // Select the channel mode
  //
  log::info( ": channelMode = Only supported stereo");
  codec_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
  switch (codec_user_config_.channel_mode) {
    case BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO:
      codec_capability_.channel_mode = codec_user_config_.channel_mode;
      codec_config_.channel_mode = codec_user_config_.channel_mode;
      break;
    case BTAV_A2DP_CODEC_CHANNEL_MODE_MONO:
    case BTAV_A2DP_CODEC_CHANNEL_MODE_NONE:
      codec_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
      codec_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_NONE;
      break;
  }
  codec_selectable_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  codec_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  codec_config_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;
  if (codec_config_.channel_mode == BTAV_A2DP_CODEC_CHANNEL_MODE_NONE) {
    log::error(": codec_config_.channel_mode != BTAV_A2DP_CODEC_CHANNEL_MODE_NONE or BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO"
              );
    goto fail;
  }

  /*******************************************
   * Update maxTargetBitrate
   *
   *******************************************/
  result_config_cie.maxTargetBitrate = sink_info_cie.maxTargetBitrate;

  log::info( ": Config Max bitrate result(0x{:02x})", result_config_cie.maxTargetBitrate);

  /*******************************************
   * Update channelSplitMode
   *
   *******************************************/
  result_config_cie.channelSplitMode = sink_info_cie.channelSplitMode;
  log::info(": channelSplitMode = {}", result_config_cie.channelSplitMode);


  /*******************************************
   * quality mode: magic num check and reconfigure
   * to specific 1
   *******************************************/
  if ((codec_user_config_.codec_specific_1 & A2DP_LHDC_VENDOR_CMD_MASK) != A2DP_LHDC_QUALITY_MAGIC_NUM) {
      codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_ABR;
      log::info(": use default quality_mode:ABR");
  }
  quality_mode = codec_user_config_.codec_specific_1 & 0xff;

  // (1000K) does not supported in V3, reset to 900K
  if (quality_mode == A2DP_LHDC_QUALITY_HIGH1) {
    codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_HIGH;
    quality_mode = A2DP_LHDC_QUALITY_HIGH;
    log::info(": reset non-supported quality_mode to ",
        lhdcV3_QualityModeBitRate_toString(quality_mode).c_str());
  }

  /*******************************************
   * LHDC features: safety tag check
   * to specific 3
   *******************************************/
  if ((codec_user_config_.codec_specific_3 & A2DP_LHDC_VENDOR_FEATURE_MASK) != A2DP_LHDC_FEATURE_MAGIC_NUM)
  {
      log::info( ": LHDC feature tag not matched! use old feature settings");

      /* *
       * Magic num does not match:
       * 1. add tag
       * 2. Re-adjust previous feature(which refers to codec_user_config)'s state(in codec_config_) to codec_user_config_:
       *  AR(has UI)
       * */
      // clean entire specific and set safety tag
      codec_user_config_.codec_specific_3 = A2DP_LHDC_FEATURE_MAGIC_NUM;

      // Feature: AR
      hasUserSet = A2DP_IsFeatureInCodecConfigLhdcV3(&allCfgPack, LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE);
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE,
          A2DP_LHDC_TO_A2DP_CODEC_USER_,
          (hasUserSet?true:false));
      log::info( ": LHDC features tag check fail, reset UI status[AR] => {}", hasUserSet?"true":"false");
  }

  /*******************************************
   *  LLAC: caps-control enabling
   *******************************************/
  {
    hasFeature = (a2dp_lhdcv3_sink_caps.hasFeatureLLAC & sink_info_cie.hasFeatureLLAC);
    result_config_cie.hasFeatureLLAC = false;
    hasUserSet = true;  //caps-control enabling case => always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV3(
        &allCfgPack,
        LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_| A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);

    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureLLAC = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
          true);
    }
    log::info( ": featureLLAC: enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureLLAC?"Y":"N"),
        sink_info_cie.hasFeatureLLAC,
        a2dp_lhdcv3_sink_caps.hasFeatureLLAC);
  }

  /*******************************************
   *  LHDCV4: caps-control enabling
   *******************************************/
  {
    hasFeature = (a2dp_lhdcv3_sink_caps.hasFeatureLHDCV4 & sink_info_cie.hasFeatureLHDCV4);
    result_config_cie.hasFeatureLHDCV4 = false;
    hasUserSet = true;  //caps-control enabling case => always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV3(
        &allCfgPack,
        LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_| A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);

    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureLHDCV4 = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
          true);
    }
    log::info( ": featureV4: enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureLHDCV4?"Y":"N"),
        sink_info_cie.hasFeatureLHDCV4,
        a2dp_lhdcv3_sink_caps.hasFeatureLHDCV4);
  }

  /*******************************************
   *  JAS: caps-control enabling
   *******************************************/
  {
    //result_config_cie.hasFeatureJAS = sink_info_cie.hasFeatureJAS;
    hasFeature = (a2dp_lhdcv3_sink_caps.hasFeatureJAS & sink_info_cie.hasFeatureJAS);
    result_config_cie.hasFeatureJAS = false;
    hasUserSet = true;  //caps-control enabling case => always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV3(
        &allCfgPack,
        LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_| A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);

    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureJAS = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
          true);
    }
    log::info( ": featureJAS: enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureJAS?"Y":"N"),
        sink_info_cie.hasFeatureJAS,
        a2dp_lhdcv3_sink_caps.hasFeatureJAS);
  }

  /*******************************************
   * AR: user-control control enabling
   *******************************************/
  {
    hasFeature = (a2dp_lhdcv3_sink_caps.hasFeatureAR & sink_info_cie.hasFeatureAR);
    result_config_cie.hasFeatureAR = false;
    hasUserSet = A2DP_IsFeatureInUserConfigLhdcV3(&allCfgPack, LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE);

    A2DP_UpdateFeatureToA2dpConfigLhdcV3(
        &allCfgPack,
        LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);

    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureAR = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
          true);

      /* Special Rule: AR only enable at the the sample rate of 48KHz */
      if(codec_user_config_.sample_rate > BTAV_A2DP_CODEC_SAMPLE_RATE_48000) {
        log::info( ": AR ON, adjust sample rate to 48KHz  ");
        codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
        result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_48000;
      }
    }
    log::info( ": featureAR: enabled? <{}> Peer:0x{:02x} Local:0x{:02x} User:{}",
        (result_config_cie.hasFeatureAR?"Y":"N"),
        sink_info_cie.hasFeatureAR,
        a2dp_lhdcv3_sink_caps.hasFeatureAR,
        (hasUserSet?"Y":"N"));
  }

  /*******************************************
   * META: caps-control enabling
   *******************************************/
  {
    hasFeature = (a2dp_lhdcv3_sink_caps.hasFeatureMETA & sink_info_cie.hasFeatureMETA);
    result_config_cie.hasFeatureMETA = false;
    hasUserSet = true;  //caps-control enabling, always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV3(
        &allCfgPack,
        LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);
    // update
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureMETA = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }
    log::info( ": featureMETA:enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureMETA?"Y":"N"),
        sink_info_cie.hasFeatureMETA,
        a2dp_lhdcv3_sink_caps.hasFeatureMETA);
  }

  /*******************************************
   * Min BitRate (MBR): caps-control enabling
   *******************************************/
  {
    hasFeature = (a2dp_lhdcv3_sink_caps.hasFeatureMinBitrate & sink_info_cie.hasFeatureMinBitrate);
    result_config_cie.hasFeatureMinBitrate = false;
    hasUserSet = true;  //caps-control enabling, always true

    A2DP_UpdateFeatureToA2dpConfigLhdcV3(
        &allCfgPack,
        LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE,
        (A2DP_LHDC_TO_A2DP_CODEC_CONFIG_ | A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
            A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
        false);
    // update
    if (hasFeature && hasUserSet) {
      result_config_cie.hasFeatureMinBitrate = true;
      A2DP_UpdateFeatureToA2dpConfigLhdcV3(
          &allCfgPack,
          LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE,
          (A2DP_LHDC_TO_A2DP_CODEC_CAP_ |
              A2DP_LHDC_TO_A2DP_CODEC_SELECT_CAP_ | A2DP_LHDC_TO_A2DP_CODEC_USER_),
              true);
    }
    log::info( ": featureMBR: enabled? <{}> Peer:0x{:02x} Local:0x{:02x}",
        (result_config_cie.hasFeatureMinBitrate?"Y":"N"),
        sink_info_cie.hasFeatureMinBitrate,
        a2dp_lhdcv3_sink_caps.hasFeatureMinBitrate);
  }

  /*******************************************
   * Update Feature/Capabilities: LARC
   * to A2DP specifics
   *******************************************/
  // LARC is not supported

  /*******************************************
   * quality mode: re-adjust according to maxTargetBitrate(smaller one adopted)
   *******************************************/
  if ( (result_config_cie.hasFeatureLLAC && result_config_cie.hasFeatureLHDCV4) &&
     (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_96000) &&
     (quality_mode != A2DP_LHDC_QUALITY_ABR))
  {
    //In this case, max bit rate mechanism is disabled(set to 900k)
    result_config_cie.maxTargetBitrate = A2DP_LHDC_MAX_BIT_RATE_900K;
    log::info(": [LLAC + LHDC V4]: set MBR (0x{:02x})", result_config_cie.maxTargetBitrate);

    //dont re-adjust quality mode in this case
    log::info(": do not adjust quality_mode in this case");
  }
  else
  {
    maxBitRate_Qmode = A2DP_MaxBitRatetoQualityLevelLhdcV3(result_config_cie.maxTargetBitrate);
    if(maxBitRate_Qmode < 0xFF) {
      if(quality_mode != A2DP_LHDC_QUALITY_ABR && quality_mode > maxBitRate_Qmode){
        log::info(": adjust quality_mode:0x{:02x} to 0x{:02x} by maxTargetBitrate:0x{:02x}",
            quality_mode, maxBitRate_Qmode, result_config_cie.maxTargetBitrate);
        quality_mode = maxBitRate_Qmode;
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | quality_mode;
      }
    }
  }

  /*
   * Final Custom Rules of resolving conflict between capabilities and version
   */
  if (result_config_cie.hasFeatureLLAC && result_config_cie.hasFeatureLHDCV4) {
    //LHDCV4 + LLAC
    if (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_96000) {

      if (quality_mode == A2DP_LHDC_QUALITY_ABR) {
    result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_48000;
    codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
    codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000; //also set UI settings
    result_config_cie.hasFeatureLHDCV4 = false;
    codec_config_.codec_specific_3 &= ~A2DP_LHDC_V4_ENABLED;
    log::info(": [LLAC + LHDC V4]: LLAC, reset sampleRate (0x{:02x})", result_config_cie.sampleRate);
      } else {
        result_config_cie.hasFeatureLLAC = false;
        codec_config_.codec_specific_3 &= ~A2DP_LHDC_LLAC_ENABLED;
        log::info(": [LLAC + LHDC V4]: LHDC");

        //result_config_cie.maxTargetBitrate = A2DP_LHDC_MAX_BIT_RATE_900K;
        //log::info(": [LLAC + LHDC V4]: set MBR (0x{})", result_config_cie.maxTargetBitrate);

        if (result_config_cie.hasFeatureMinBitrate) {
          if (quality_mode < A2DP_LHDC_QUALITY_MID) {
            codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_MID;
            quality_mode = A2DP_LHDC_QUALITY_MID;
            log::info(": [LLAC + LHDC V4]: LHDC 96KSR, reset Qmode (0x{:02x})", quality_mode);
          }
        } else {
        if (quality_mode < A2DP_LHDC_QUALITY_LOW) {
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW;
        quality_mode = A2DP_LHDC_QUALITY_LOW;
        log::info(": [LLAC + LHDC V4]: LHDC 96KSR, reset Qmode (0x{:02x})", quality_mode);
      }
        }
      }
    } else if (
        (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_48000 && (quality_mode > A2DP_LHDC_QUALITY_LOW && quality_mode != A2DP_LHDC_QUALITY_ABR)) ||
        (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_44100 && (quality_mode > A2DP_LHDC_QUALITY_LOW && quality_mode != A2DP_LHDC_QUALITY_ABR))
       ) {
      result_config_cie.hasFeatureLLAC = false;
      codec_config_.codec_specific_3 &= ~A2DP_LHDC_LLAC_ENABLED;
      log::info(": [LLAC + LHDC V4]: LHDC");
    } else if (
       (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_48000 && (quality_mode <= A2DP_LHDC_QUALITY_LOW || quality_mode == A2DP_LHDC_QUALITY_ABR)) ||
       (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_44100 && (quality_mode <= A2DP_LHDC_QUALITY_LOW || quality_mode == A2DP_LHDC_QUALITY_ABR))
      ) {
      result_config_cie.hasFeatureLHDCV4 = false;
      codec_config_.codec_specific_3 &= ~A2DP_LHDC_V4_ENABLED;
      log::info(": [LLAC + LHDC V4]: LLAC");

      /* LLAC: prevent quality mode using 64kbps */
      if (result_config_cie.hasFeatureMinBitrate) {
      if (quality_mode < A2DP_LHDC_QUALITY_LOW1) {
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW1;
        quality_mode = A2DP_LHDC_QUALITY_LOW1;
        log::info(": [LLAC + LHDC V4]: LLAC, reset Qmode (0x{:02x})", quality_mode);
      }
      }
    } else {
      log::error(": [LLAC + LHDC V4]: format incorrect.");
      goto fail;
    }

  } else if (!result_config_cie.hasFeatureLLAC && result_config_cie.hasFeatureLHDCV4) {
    //LHDC V4 only
    log::info(": [LHDCV4 only]");
  if (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_96000) {
    if (result_config_cie.hasFeatureMinBitrate) {
      if (quality_mode < A2DP_LHDC_QUALITY_LOW) {
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW;
        quality_mode = A2DP_LHDC_QUALITY_LOW;
        log::info(": [LHDCV4 only]: reset Qmode (0x{:02x})", quality_mode);
      }
    }
  } else {
    if (result_config_cie.hasFeatureMinBitrate) {
      if (quality_mode < A2DP_LHDC_QUALITY_LOW4) {
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW4;
        quality_mode = A2DP_LHDC_QUALITY_LOW4;
        log::info(": [LHDCV4 only]: reset Qmode (0x{:02x}), ", quality_mode);
      }
    }
  }

  } else if (result_config_cie.hasFeatureLLAC && !result_config_cie.hasFeatureLHDCV4) {
    //LLAC only
    log::info(": [LLAC only]");
    if (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_96000) {
      result_config_cie.sampleRate = A2DP_LHDC_SAMPLING_FREQ_48000;
      codec_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
      codec_user_config_.sample_rate = BTAV_A2DP_CODEC_SAMPLE_RATE_48000; //also set UI settings
      log::info(": [LLAC only]: reset SampleRate (0x{:02x})", result_config_cie.sampleRate);
    }

    if (quality_mode > A2DP_LHDC_QUALITY_LOW && quality_mode != A2DP_LHDC_QUALITY_ABR) {
      codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW;
      quality_mode = A2DP_LHDC_QUALITY_LOW;
      log::info(": [LLAC only]: reset Qmode (0x{:02x})", quality_mode);
    }

    /* LLAC: prevent quality mode using 64kbps */
    if (result_config_cie.hasFeatureMinBitrate) {
    if (quality_mode < A2DP_LHDC_QUALITY_LOW1) {
      codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW1;
      quality_mode = A2DP_LHDC_QUALITY_LOW1;
      log::info(": [LLAC only]: reset Qmode (0x{:02x})", quality_mode);
    }
    }

  } else if (!result_config_cie.hasFeatureLLAC && !result_config_cie.hasFeatureLHDCV4) {
    //LHDC V3 only
    log::info(": [LHDCV3 only]");
  if (result_config_cie.sampleRate == A2DP_LHDC_SAMPLING_FREQ_96000) {
    if (result_config_cie.hasFeatureMinBitrate) {
      if (quality_mode < A2DP_LHDC_QUALITY_LOW) {
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW;
        quality_mode = A2DP_LHDC_QUALITY_LOW;
        log::info(": [LHDCV3 only]: reset Qmode (0x{:02x})", quality_mode);
      }
    }
  } else {
    if (result_config_cie.hasFeatureMinBitrate) {
      if (quality_mode < A2DP_LHDC_QUALITY_LOW4) {
        codec_user_config_.codec_specific_1 = A2DP_LHDC_QUALITY_MAGIC_NUM | A2DP_LHDC_QUALITY_LOW4;
        quality_mode = A2DP_LHDC_QUALITY_LOW4;
        log::info(": [LHDCV3 only]: reset Qmode (0x{:02x}), ", quality_mode);
      }
    }
  }
  }

  log::info(": Final quality_mode = ({}) ",
      quality_mode,
      lhdcV3_QualityModeBitRate_toString(quality_mode).c_str());

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

  /* Setup final nego result codec config to peer */
  if (int ret = A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &result_config_cie,
                         p_result_codec_config) != A2DP_SUCCESS) {
    log::error(": A2DP_BuildInfoLhdcV3Sink fail(0x{:02x})", ret);
    goto fail;
  }


  // Create a local copy of the peer codec capability, and the
  // result codec config.
    log::error(": is_capability = {}", is_capability);
  if (is_capability) {
    status = A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
                                ota_codec_peer_capability_);
  } else {
    status = A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &sink_info_cie,
                                ota_codec_peer_config_);
  }
  CHECK(status == A2DP_SUCCESS);

  status = A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &result_config_cie,
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

bool A2dpCodecConfigLhdcV3Base::setPeerCodecCapabilities(
      const uint8_t* p_peer_codec_capabilities) {
  std::lock_guard<std::recursive_mutex> lock(codec_mutex_);
  is_source_ = false;
  tA2DP_LHDCV3_SINK_CIE peer_info_cie;
  uint8_t sampleRate;
  uint8_t bits_per_sample;

  // Save the internal state
  btav_a2dp_codec_config_t saved_codec_selectable_capability =
  codec_selectable_capability_;
  uint8_t saved_ota_codec_peer_capability[AVDT_CODEC_SIZE];
  memcpy(saved_ota_codec_peer_capability, ota_codec_peer_capability_,
         sizeof(ota_codec_peer_capability_));

  tA2DP_STATUS status =
  A2DP_ParseInfoLhdcV3Sink(&peer_info_cie, p_peer_codec_capabilities, true);
  if (status != A2DP_SUCCESS) {
      log::error( ": can't parse peer's capabilities: error = {}",
                 status);
      goto fail;
  }
/*
  if (peer_info_cie.version > a2dp_lhdc_source_caps.version) {
      log::error( ": can't parse peer's capabilities: Missmatch version({}:{})",
                 a2dp_lhdc_source_caps.version, peer_info_cie.version);
      goto fail;
  }
*/

  // Compute the selectable capability - bits per sample
  //codec_selectable_capability_.bits_per_sample =
  //a2dp_lhdc_source_caps.bits_per_sample;
  bits_per_sample = a2dp_lhdcv3_sink_caps.bits_per_sample & peer_info_cie.bits_per_sample;
  if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16) {
      codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_16;
  }
  if (bits_per_sample & BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24) {
      codec_selectable_capability_.bits_per_sample |= BTAV_A2DP_CODEC_BITS_PER_SAMPLE_24;
  }


  // Compute the selectable capability - sample rate
  sampleRate = a2dp_lhdcv3_sink_caps.sampleRate & peer_info_cie.sampleRate;
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_44100) {
      codec_selectable_capability_.sample_rate |=
      BTAV_A2DP_CODEC_SAMPLE_RATE_44100;
  }
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_48000) {
      codec_selectable_capability_.sample_rate |=
      BTAV_A2DP_CODEC_SAMPLE_RATE_48000;
  }
  if (sampleRate & A2DP_LHDC_SAMPLING_FREQ_96000) {
      codec_selectable_capability_.sample_rate |=
      BTAV_A2DP_CODEC_SAMPLE_RATE_96000;
  }


  // Compute the selectable capability - channel mode
  codec_selectable_capability_.channel_mode = BTAV_A2DP_CODEC_CHANNEL_MODE_STEREO;

  status = A2DP_BuildInfoLhdcV3Sink(AVDT_MEDIA_TYPE_AUDIO, &peer_info_cie,
                              ota_codec_peer_capability_);
  CHECK(status == A2DP_SUCCESS);
  return true;

fail:
  // Restore the internal state
  codec_selectable_capability_ = saved_codec_selectable_capability;
  memcpy(ota_codec_peer_capability_, saved_ota_codec_peer_capability,
         sizeof(ota_codec_peer_capability_));
  return false;
}



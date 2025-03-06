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

//
// A2DP Codec API for LHDC
//

#ifndef A2DP_VENDOR_LHDCV3_H
#define A2DP_VENDOR_LHDCV3_H

#include "a2dp_codec_api.h"
#include "a2dp_vendor_lhdc_constants.h"
#include "avdt_api.h"

#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#define LOG_NDEBUG 1	//set 0 to turn on VERBOSE LOG
#endif

/** Start of LHDC A2DP-Related API definition ***************************************/
#define EXTEND_FUNC_CODE_GET_SPECIFIC                   ((unsigned int) 0x0A010001)
#define EXTEND_FUNC_VER_GET_SPECIFIC_V1                 ((unsigned int) 0x01000000)
#define EXTEND_FUNC_VER_GET_SPECIFIC_V2                 ((unsigned int) 0x02000000)
#define LHDC_EXTEND_FUNC_CODE_A2DP_TYPE_MASK            (0x0A)
#define LHDC_EXTEND_FUNC_CODE_LIB_TYPE_MASK             (0x0C)

/* ************************************************************************
 * Version info: EXTEND_FUNC_CODE_GET_SPECIFIC
 * EXTEND_FUNC_VER_GET_SPECIFIC_V1:  Total Size: 41 bytes
   * API Version:                   (4 bytes)
   * API Code:                      (4 bytes)
   * A2DP Codec Config Code:        (1 bytes)
   * A2dp Specific1:                (8 bytes)
   * A2dp Specific2:                (8 bytes)
   * A2dp Specific3:                (8 bytes)
   * A2dp Specific4:                (8 bytes)
 * EXTEND_FUNC_VER_GET_SPECIFIC_V2:  Total Size: 64 bytes
   * API Version:                   (4 bytes)
   * API Code:                      (4 bytes)
   * A2DP Codec Config Code:        (1 bytes)
   * Reserved:                      (7 bytes)
   * A2dp Specific1:                (8 bytes)
   * A2dp Specific2:                (8 bytes)
   * A2dp Specific3:                (8 bytes)
   * A2dp Specific4:                (8 bytes)
   * Capabilities Metadata sub fields:  (7*2 bytes)
     * sub[0~1]:    JAS
     * sub[2~3]:    AR
     * sub[4~5]:    META
     * sub[6~7]:    LLAC
     * sub[8~9]:    MBR
     * sub[10~11]:  LARC
     * sub[12~13]:  LHDCV4
   * Padded:                        (2 bytes)
 * ************************************************************************/
#define LHDC_EXTEND_FUNC_CONFIG_API_VERSION_SIZE        4       /* API version */
#define LHDC_EXTEND_FUNC_CONFIG_API_CODE_SIZE           4       /* API index code */
#define LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_SIZE       1       /* A2DP codec config code */
#define LHDC_EXTEND_FUNC_CONFIG_RESERVED_V2             7       /* V2 Reserved bytes */
#define LHDC_EXTEND_FUNC_CONFIG_SPECIFIC1_SIZE          8       /* Specific 1 */
#define LHDC_EXTEND_FUNC_CONFIG_SPECIFIC2_SIZE          8       /* Specific 2 */
#define LHDC_EXTEND_FUNC_CONFIG_SPECIFIC3_SIZE          8       /* Specific 3 */
#define LHDC_EXTEND_FUNC_CONFIG_SPECIFIC4_SIZE          8       /* Specific 4 */
/* Capabilities metadata fields(2 bytes for each tuple) */
#define LHDC_EXTEND_FUNC_CONFIG_CAPMETA_SIZE_V2         (7<<1)  /* V2 Capabilities */
#define LHDC_EXTEND_FUNC_CONFIG_PADDED_SIZE_V2          2       /* V2 Padded Fields */

/* Total size of buffer */
#define LHDC_EXTEND_FUNC_CONFIG_TOTAL_FIXED_SIZE_V1    (LHDC_EXTEND_FUNC_CONFIG_API_VERSION_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_API_CODE_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC1_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC2_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC3_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC4_SIZE)
#define LHDC_EXTEND_FUNC_CONFIG_TOTAL_FIXED_SIZE_V2    (LHDC_EXTEND_FUNC_CONFIG_API_VERSION_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_API_CODE_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_RESERVED_V2 + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC1_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC2_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC3_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_SPECIFIC4_SIZE + \
                                                          LHDC_EXTEND_FUNC_CONFIG_CAPMETA_SIZE_V2 + \
                                                          LHDC_EXTEND_FUNC_CONFIG_PADDED_SIZE_V2)
/* Head of each field */
#define LHDC_EXTEND_FUNC_CONFIG_API_VERSION_HEAD        (0)
#define LHDC_EXTEND_FUNC_CONFIG_API_CODE_HEAD           (LHDC_EXTEND_FUNC_CONFIG_API_VERSION_HEAD + 4)  //4
#define LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_HEAD       (LHDC_EXTEND_FUNC_CONFIG_API_CODE_HEAD + 4)     //8
/* Following part in V1 */
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS1_HEAD_V1        (LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_HEAD + 1) //9~16
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS2_HEAD_V1        (LHDC_EXTEND_FUNC_A2DP_SPECIFICS1_HEAD_V1 + 8)  //17~24
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS3_HEAD_V1        (LHDC_EXTEND_FUNC_A2DP_SPECIFICS2_HEAD_V1 + 8)  //25~32
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS4_HEAD_V1        (LHDC_EXTEND_FUNC_A2DP_SPECIFICS3_HEAD_V1 + 8)  //33~40
/* Following part in V2 */
#define LHDC_EXTEND_FUNC_A2DP_RESERVED_HEAD_V2          (LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_HEAD + 1) //9~15
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS1_HEAD_V2        (LHDC_EXTEND_FUNC_CONFIG_A2DPCFG_CODE_HEAD + 8) //16~23
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS2_HEAD_V2        (LHDC_EXTEND_FUNC_A2DP_SPECIFICS1_HEAD_V2 + 8)  //24~31
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS3_HEAD_V2        (LHDC_EXTEND_FUNC_A2DP_SPECIFICS2_HEAD_V2 + 8)  //32~39
#define LHDC_EXTEND_FUNC_A2DP_SPECIFICS4_HEAD_V2        (LHDC_EXTEND_FUNC_A2DP_SPECIFICS3_HEAD_V2 + 8)  //40~47
#define LHDC_EXTEND_FUNC_A2DP_CAPMETA_HEAD_V2           (LHDC_EXTEND_FUNC_A2DP_SPECIFICS4_HEAD_V2 + 8)  //48~61
#define LHDC_EXTEND_FUNC_A2DP_PADDED_HEAD_V2            (LHDC_EXTEND_FUNC_A2DP_CAPMETA_HEAD_V2 + LHDC_EXTEND_FUNC_CONFIG_CAPMETA_SIZE_V2)   //62~63


/* code definition mapping to A2DP codec specific in a2dp_codec_api.h
 * 0x01: codec_config_
 * 0x02: codec_capability_
 * 0x03: codec_local_capability_
 * 0x04: codec_selectable_capability_
 * 0x05: codec_user_config_
 * 0x06: codec_audio_config_
 */
#define LHDC_EXTEND_FUNC_A2DP_TYPE_SPECIFICS_FINAL_CFG          (0x01)
#define LHDC_EXTEND_FUNC_A2DP_TYPE_SPECIFICS_FINAL_CAP          (0x02)
#define LHDC_EXTEND_FUNC_A2DP_TYPE_SPECIFICS_LOCAL_CAP          (0x03)
#define LHDC_EXTEND_FUNC_A2DP_TYPE_SPECIFICS_SELECTABLE_CAP     (0x04)
#define LHDC_EXTEND_FUNC_A2DP_TYPE_SPECIFICS_USER_CFG           (0x05)
#define LHDC_EXTEND_FUNC_A2DP_TYPE_SPECIFICS_AUDIO_CFG          (0x06)


/************************
 * Capability Meta Format: (denotes where source capabilities bits are stored in specifics)
   * Capability Code:                   (1 byte)
   * Saving Position Info:              (1 byte)
 ************************/
/* Capabilities's code: */
#define LHDC_EXTEND_FUNC_A2DP_LHDC_JAS_CODE      (0x01)
#define LHDC_EXTEND_FUNC_A2DP_LHDC_AR_CODE       (0x02)
#define LHDC_EXTEND_FUNC_A2DP_LHDC_META_CODE     (0x03)
#define LHDC_EXTEND_FUNC_A2DP_LHDC_LLAC_CODE     (0x04)
#define LHDC_EXTEND_FUNC_A2DP_LHDC_MBR_CODE      (0x05)
#define LHDC_EXTEND_FUNC_A2DP_LHDC_LARC_CODE     (0x06)
#define LHDC_EXTEND_FUNC_A2DP_LHDC_V4_CODE       (0x07)

/* Capabilities's saving position Info:
 *  1. in which specific                        (represented in leftmost 2-bits)
 *  2. at which bit position of the specific    (represented in rightmost 6-bits)
 * */
#define LHDC_EXTEND_FUNC_A2DP_SPECIFIC1_INDEX    (0x00)     //2-bit:00
#define LHDC_EXTEND_FUNC_A2DP_SPECIFIC2_INDEX    (0x40)     //2-bit:01
#define LHDC_EXTEND_FUNC_A2DP_SPECIFIC3_INDEX    (0x80)     //2-bit:10
#define LHDC_EXTEND_FUNC_A2DP_SPECIFIC4_INDEX    (0xC0)     //2-bit:11
/** End of LHDC A2DP-Related API definition ***************************************/


class A2dpCodecConfigLhdcV3 : public A2dpCodecConfig {
 public:
  bool copySinkCapability(uint8_t * codec_info);
  A2dpCodecConfigLhdcV3(btav_a2dp_codec_priority_t codec_priority);
  virtual ~A2dpCodecConfigLhdcV3();

  bool init() override;
  //uint64_t encoderIntervalMs() const override;
  //int getEffectiveMtu() const override;
  tA2DP_STATUS setCodecConfig(const uint8_t* p_peer_codec_info, bool is_capability,
                      uint8_t* p_result_codec_config) override;
  bool setPeerCodecCapabilities(
      const uint8_t* p_peer_codec_capabilities) override;
 private:
  bool useRtpHeaderMarkerBit() const override;
  /*
  bool updateEncoderUserConfig(
      const tA2DP_ENCODER_INIT_PEER_PARAMS* p_peer_params,
      bool* p_restart_input, bool* p_restart_output,
      bool* p_config_updated) override;
      */
  void debug_codec_dump(int fd) override;
};

bool A2DP_VendorGetLowLatencyEnabledLhdcV3();
// Checks whether the codec capabilities contain a valid A2DP LHDC Source
// codec.
// NOTE: only codecs that are implemented are considered valid.
// Returns true if |p_codec_info| contains information about a valid LHDC
// codec, otherwise false.
bool A2DP_IsVendorSourceCodecValidLhdcV3(const uint8_t* p_codec_info);

// Checks whether the codec capabilities contain a valid peer A2DP LHDC Sink
// codec.
// NOTE: only codecs that are implemented are considered valid.
// Returns true if |p_codec_info| contains information about a valid LHDC
// codec, otherwise false.
bool A2DP_IsVendorPeerSinkCodecValidLhdcV3(const uint8_t* p_codec_info);

// Checks whether the A2DP data packets should contain RTP header.
// |content_protection_enabled| is true if Content Protection is
// enabled. |p_codec_info| contains information about the codec capabilities.
// Returns true if the A2DP data packets should contain RTP header, otherwise
// false.
bool A2DP_VendorUsesRtpHeaderLhdcV3(bool content_protection_enabled,
                                  const uint8_t* p_codec_info);

// Gets the A2DP LHDC codec name for a given |p_codec_info|.
const char* A2DP_VendorCodecNameLhdcV3(const uint8_t* p_codec_info);

// Checks whether two A2DP LHDC codecs |p_codec_info_a| and |p_codec_info_b|
// have the same type.
// Returns true if the two codecs have the same type, otherwise false.
bool A2DP_VendorCodecTypeEqualsLhdcV3(const uint8_t* p_codec_info_a,
                                    const uint8_t* p_codec_info_b);

// Checks whether two A2DP LHDC codecs |p_codec_info_a| and |p_codec_info_b|
// are exactly the same.
// Returns true if the two codecs are exactly the same, otherwise false.
// If the codec type is not LHDC, the return value is false.
bool A2DP_VendorCodecEqualsLhdcV3(const uint8_t* p_codec_info_a,
                                const uint8_t* p_codec_info_b);

// Gets the track sample rate value for the A2DP LHDC codec.
// |p_codec_info| is a pointer to the LHDC codec_info to decode.
// Returns the track sample rate on success, or -1 if |p_codec_info|
// contains invalid codec information.
int A2DP_VendorGetTrackSampleRateLhdcV3(const uint8_t* p_codec_info);

// Gets the bits per audio sample for the A2DP LHDC codec.
// |p_codec_info| is a pointer to the LHDC codec_info to decode.
// Returns the bits per audio sample on success, or -1 if |p_codec_info|
// contains invalid codec information.
int A2DP_VendorGetTrackBitsPerSampleLhdcV3(const uint8_t* p_codec_info);

// Gets the channel count for the A2DP LHDC codec.
// |p_codec_info| is a pointer to the LHDC codec_info to decode.
// Returns the channel count on success, or -1 if |p_codec_info|
// contains invalid codec information.
int A2DP_VendorGetTrackChannelCountLhdcV3(const uint8_t* p_codec_info);

// Gets the channel mode code for the A2DP LHDC codec.
// The actual value is codec-specific - see |A2DP_LHDC_CHANNEL_MODE_*|.
// |p_codec_info| is a pointer to the LHDC codec_info to decode.
// Returns the channel mode code on success, or -1 if |p_codec_info|
// contains invalid codec information.
int A2DP_VendorGetChannelModeCodeLhdcV3(const uint8_t* p_codec_info);

// Gets the A2DP LHDC audio data timestamp from an audio packet.
// |p_codec_info| contains the codec information.
// |p_data| contains the audio data.
// The timestamp is stored in |p_timestamp|.
// Returns true on success, otherwise false.
bool A2DP_VendorGetPacketTimestampLhdcV3(const uint8_t* p_codec_info,
                                       const uint8_t* p_data,
                                       uint32_t* p_timestamp);

// Builds A2DP LHDC codec header for audio data.
// |p_codec_info| contains the codec information.
// |p_buf| contains the audio data.
// |frames_per_packet| is the number of frames in this packet.
// Returns true on success, otherwise false.
bool A2DP_VendorBuildCodecHeaderLhdcV3(const uint8_t* p_codec_info, BT_HDR* p_buf,
                                     uint16_t frames_per_packet);

// Decodes A2DP LHDC codec info into a human readable string.
// |p_codec_info| is a pointer to the LHDC codec_info to decode.
// Returns a string describing the codec information.
std::string A2DP_VendorCodecInfoStringLhdcV3(const uint8_t* p_codec_info);

// New feature to check codec info is supported Channel Separation.
int8_t A2DP_VendorGetChannelSplitModeLhdcV3(const uint8_t* p_codec_info);

bool A2DP_VendorGetLowLatencyStateLhdcV3(const uint8_t* p_codec_info);
int16_t A2DP_VendorGetMaxDatarateLhdcV3(const uint8_t* p_codec_info);
uint8_t A2DP_VendorGetVersionLhdcV3(const uint8_t* p_codec_info);

bool A2DP_VendorHasV4FlagLhdcV3(const uint8_t* p_codec_info);
bool A2DP_VendorHasLLACFlagLhdcV3(const uint8_t* p_codec_info);

bool A2DP_VendorHasJASFlagLhdcV3(const uint8_t* p_codec_info);
bool A2DP_VendorHasARFlagLhdcV3(const uint8_t* p_codec_info);
bool A2DP_VendorHasMETAFlagLhdcV3(const uint8_t* p_codec_info);
bool A2DP_VendorHasMinBRFlagLhdcV3(const uint8_t* p_codec_info);
bool A2DP_VendorHasLARCFlagLhdcV3(const uint8_t* p_codec_info);

// Decodes and displays LHDC codec info (for debugging).
// |p_codec_info| is a pointer to the LHDC codec_info to decode and display.
void A2DP_VendorDumpCodecInfoLhdcV3(const uint8_t* p_codec_info);

// Gets the A2DP LHDC encoder interface that can be used to encode and prepare
// A2DP packets for transmission - see |tA2DP_ENCODER_INTERFACE|.
// |p_codec_info| contains the codec information.
// Returns the A2DP LHDC encoder interface if the |p_codec_info| is valid and
// supported, otherwise NULL.
const tA2DP_ENCODER_INTERFACE* A2DP_VendorGetEncoderInterfaceLhdcV3(
    const uint8_t* p_codec_info);

// Adjusts the A2DP LHDC codec, based on local support and Bluetooth
// specification.
// |p_codec_info| contains the codec information to adjust.
// Returns true if |p_codec_info| is valid and supported, otherwise false.
bool A2DP_VendorAdjustCodecLhdcV3(uint8_t* p_codec_info);

// Gets the A2DP LHDC Source codec index for a given |p_codec_info|.
// Returns the corresponding |btav_a2dp_codec_index_t| on success,
// otherwise |BTAV_A2DP_CODEC_INDEX_MAX|.
btav_a2dp_codec_index_t A2DP_VendorSourceCodecIndexLhdcV3(
    const uint8_t* p_codec_info);

// Gets the A2DP LHDC Source codec name.
const char* A2DP_VendorCodecIndexStrLhdcV3(void);

// Initializes A2DP LHDC Source codec information into |tAVDT_CFG|
// configuration entry pointed by |p_cfg|.
bool A2DP_VendorInitCodecConfigLhdcV3(AvdtpSepConfig* p_cfg);

bool A2DP_VendorGetSrcCapVectorLhdcv3(uint8_t* capVector);

// Gets the track bitrate value for the A2DP LHDCV3 codec.
// |p_codec_info| is a pointer to the LHDC codec_info to decode.
// Returns the track bit rate on success, or -1 if |p_codec_info|
// contains invalid codec information.
int A2DP_VendorGetBitRateLhdcV3(const uint8_t* p_codec_info);

#endif  // A2DP_VENDOR_LHDCV3_H

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

//
// Interface to the A2DP LHDC V5 Decoder
//

#ifndef A2DP_VENDOR_LHDCV5_DECODER_H
#define A2DP_VENDOR_LHDCV5_DECODER_H

#include "a2dp_codec_api.h"
#include "a2dp_vendor_lhdc_constants.h"
#include "a2dp_vendor_lhdcv5_constants.h"


// Save CODEC information
// Return true on success, otherwise false.
bool a2dp_lhdcv5_decoder_save_codec_info (const uint8_t* p_codec_info);

// Loads the A2DP LHDC V5 decoder.
// Return true on success, otherwise false.
bool A2DP_VendorLoadDecoderLhdcV5(void);

// Unloads the A2DP LHDC V5 decoder.
void A2DP_VendorUnloadDecoderLhdcV5(void);

// Initialize the A2DP LHDC V5 decoder.
bool a2dp_vendor_lhdcv5_decoder_init(decoded_data_callback_t decode_callback);

// Cleanup the A2DP LHDC V5 decoder.
void a2dp_vendor_lhdcv5_decoder_cleanup(void);

// Decode LHDC V5 packet to PCM
bool a2dp_vendor_lhdcv5_decoder_decode_packet(BT_HDR* p_buf);

// Start the A2DP LHDC V5 decoder.
void a2dp_vendor_lhdcv5_decoder_start(void);

// Suspend the A2DP LHDC V5 decoder.
void a2dp_vendor_lhdcv5_decoder_suspend(void);

// A2DP LHDC V5 decoder configuration.
void a2dp_vendor_lhdcv5_decoder_configure(const uint8_t* p_codec_info);

#endif  // A2DP_VENDOR_LHDCV5_DECODER_H

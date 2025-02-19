/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/import/chips/common/utils/imageProcs/common_ringId.H $    */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2016,2018                        */
/* [+] International Business Machines Corp.                              */
/*                                                                        */
/*                                                                        */
/* Licensed under the Apache License, Version 2.0 (the "License");        */
/* you may not use this file except in compliance with the License.       */
/* You may obtain a copy of the License at                                */
/*                                                                        */
/*     http://www.apache.org/licenses/LICENSE-2.0                         */
/*                                                                        */
/* Unless required by applicable law or agreed to in writing, software    */
/* distributed under the License is distributed on an "AS IS" BASIS,      */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or        */
/* implied. See the License for the specific language governing           */
/* permissions and limitations under the License.                         */
/*                                                                        */
/* IBM_PROLOG_END_TAG                                                     */

#ifndef _COMMON_RINGID_H_
#define _COMMON_RINGID_H_

#include <stdint.h>
#include <stddef.h>

///////////////////////////////////////////////////////////////////////////////
//                     Declare assumptions - Begin                           //
//                                                                           //
//

// Various data type defs for enums. Serves following purposes:
// - Reduces space since enum defaults to an int type.
// - Enables using these types without the scope operator for
//   those enums that are namespaced, e.g. RingID.
// NB! DO NOT CHANGE THESE DEFS W/O EXPLICIT CONSENT FROM
//     INFRASTRUCT TEAM. (These defs affect packing assumptions
//     of ring structures that go into the image ringSections.)
typedef uint16_t  RingId_t;         // Type for RingID enum
typedef uint8_t   ChipletType_t;    // Type for CHIPLET_TYPE enum
typedef uint8_t   PpeType_t;        // Type for PpeType
typedef uint8_t   ChipType_t;       // Type for ChipType enum
typedef uint8_t   RingType_t;       // Type for RingType enum
typedef uint8_t   RingVariant_t;    // Type for RingVariant enum
typedef uint8_t   RingBlockType_t;  // Type for RingBlockType enum, e.g. GET_SINGLE_RING
typedef uint32_t  TorCpltOffset_t;  // Type for offset value to chiplet's CMN or INST section
typedef uint8_t   myBoolean_t;      // false:0, true:1, undefined:UNDEFINED_BOOLEAN

#define  UNDEFINED_RING_ID      (RingId_t)0xffff
#define  UNDEFINED_CHIPLET_TYPE (ChipletType_t)0xff
#define  UNDEFINED_PPE_TYPE     (PpeType_t)0xff
#define  UNDEFINED_CHIP_TYPE    (ChipType_t)0xff
#define  INVALID_RING_TYPE      (RingType_t)0xff
#define  UNDEFINED_RING_VARIANT (RingVariant_t)0xff
#define  UNDEFINED_RING_BLOCK_TYPE (RingBlockType_t)0xff;

#define  UNDEFINED_DD_LEVEL     (uint8_t)0xff

#define  MAX_TOR_RING_OFFSET    (uint16_t)(256*256-1) // Max val of uint16

#define  MAX_RING_PATH_LENGTH   (uint8_t)500
#define  MAX_RING_NAME_LENGTH   (uint8_t)50

#define  UNDEFINED_BOOLEAN      (myBoolean_t)0xff

//                                                                           //
//                     Declare assumptions - End                             //
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//                     TOR layout definitions - Begin                        //
//                                                                           //

//
// TOR header field (appears in top of every HW, SBE, CEN, OVRD, etc ring section)
//
typedef struct
{
    uint32_t   magic;       // =TOR_MAGIC_xyz
    uint8_t    version;     // =TOR version
    ChipType_t chipType;    // Value from ChipType enum
    uint8_t    ddLevel;     // Actual DD level of ringSection
    uint8_t    undefined;
    uint32_t   size;        // Size of ringSection.
} TorHeader_t;

//
// Subsequent TOR fields (listed in order they appear in TOR ringSections)
//
typedef struct
{
    uint32_t offset;
    uint32_t size;
} TorPpeBlock_t;

typedef struct
{
    TorCpltOffset_t cmnOffset;
    TorCpltOffset_t instOffset;
} TorCpltBlock_t;

typedef uint16_t TorRingOffset_t;  // Offset value to actual ring

//                                                                           //
//                     TOR layout definitions - End                          //
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//                     Key TOR constants - Begin                             //
//                                                                           //

//#define TOR_VERSION  1  // Initial version. Large RS4 header.
//#define TOR_VERSION  2  // Reduced RS4 header.
//#define TOR_VERSION  3  // Added TOR magic header.
//#define TOR_VERSION  4  // TOR API code restructuring.
//#define TOR_VERSION  5  // Removed TOR-level DD handling.
//#define TOR_VERSION  6  // Added additional runtime risk level (RL2)
#define TOR_VERSION  7  // Added three more runtime risk levels (RL3/4/5)

// TOR Magic values for top-level TOR ringSection and sub-ringSections
enum TorMagicNum
{
    TOR_MAGIC       = (uint32_t)0x544F52  ,   // "TOR"
    TOR_MAGIC_HW    = (uint32_t)0x544F5248,   // "TORH"
    TOR_MAGIC_SBE   = (uint32_t)0x544F5242,   // "TORB"
    TOR_MAGIC_SGPE  = (uint32_t)0x544F5247,   // "TORG"
    TOR_MAGIC_CME   = (uint32_t)0x544F524D,   // "TORM"
    TOR_MAGIC_OVRD  = (uint32_t)0x544F5252,   // "TORR"
    TOR_MAGIC_OVLY  = (uint32_t)0x544F524C,   // "TORL"
    TOR_MAGIC_CEN   = (uint32_t)0x544F524E,   // "TORN"
};

//                                                                           //
//                     Key TOR constants - End                               //
///////////////////////////////////////////////////////////////////////////////


//
// Chip types and List to represent p9n, p9c, p9a, cen (centaur)
//
enum ChipType
{
    CT_P9N,
    CT_P9C,
    CT_P9A,
    CT_CEN,
    NUM_CHIP_TYPES
};

typedef struct
{
    const char*  name;
    ChipType_t   type;
} ChipTypeList_t;

static const ChipTypeList_t CHIP_TYPE_LIST[] =
{
    {"p9n", CT_P9N},
    {"p9c", CT_P9C},
    {"p9a", CT_P9A},
    {"cen", CT_CEN},
};


//
// Ring related data structs and types
//
typedef enum RingClass
{
    EKB_RING,
    EKB_FSM_RING,
    EKB_STUMPED_RING,
    EKB_CMSK_RING,
    EKB_NONFLUSH_RING,
    VPD_RING,
    CEN_RING,
    NUM_RING_CLASSES
} RingClass_t;

//
// General Ring ID list structure
//
typedef struct
{
    const char*  ringName;
    RingId_t     ringId;
    uint8_t      instanceIdMin;
    uint8_t      instanceIdMax;
    RingClass_t  ringClass;
    uint32_t     scanScomAddress;
} GenRingIdList;

// PPE types supported.
// - This enum also reflects the order with which they appear in the HW image's .rings section.
// - Do  NOT  make changes to the values or order of this enum.
enum PpeType
{
    PT_SBE        = 0x00,
    PT_CME        = 0x01,
    PT_SGPE       = 0x02,
    NUM_PPE_TYPES = 0x03
};

// Ring variants supported.
// - This enum also reflects the order with which they appear in various images' .rings section.
// - Do  NOT  make changes to the values or order of this enum.
enum RingVariant
{
    RV_BASE     = 0x00,
    RV_CC       = 0x01,
    RV_RL       = 0x02,  // Kernel and user protection
    RV_RL2      = 0x03,  // Kernel only protection
    RV_RL3      = 0x04,  // Rugby v4
    RV_RL4      = 0x05,  // Java performance
    RV_RL5      = 0x06,  // Spare
    NUM_RING_VARIANTS = 0x07,
};

extern const char* ppeTypeName[];
extern const char* ringVariantName[];

// Variant order for Common rings
typedef struct
{
    RingVariant_t  variant[NUM_RING_VARIANTS];
} RingVariantOrder;


// Ring types supported.
// - This enum also reflects the order with which they appear in various images' .rings section.
// - Do  NOT  make changes to the values or order of this enum.
enum RingType
{
    COMMON_RING = 0,
    INSTANCE_RING = 1,
    ALLRING = 2
};

enum RingBlockType
{
    GET_SINGLE_RING     = 0x00,
    GET_PPE_LEVEL_RINGS = 0x01,
    PUT_SINGLE_RING     = 0x02
};

typedef struct
{
    // Chiplet ID of the first instance of the Chiplet
    uint8_t iv_base_chiplet_number;

    // Number of common rings for the Chiplet
    uint8_t iv_num_common_rings;

    // Number of instance rings for the Chiplet (w/different ringId values)
    uint8_t iv_num_instance_rings;

    // Number of instance rings for the Chiplet (w/different ringId values
    // AND different scanAddress values)
    uint8_t iv_num_instance_rings_scan_addrs;

    // Number of variants for common rings (instance rings only have BASE variant)
    uint8_t iv_num_common_ring_variants;
} ChipletData_t;

// This is used to Set (Mark) the left-most bit
#define  INSTANCE_RING_MARK  (uint8_t)0x80
//
// This is used to Set (Mark) the left-most bit
#define  INSTANCE_RING_MASK  (uint8_t)0x7F

// This is used to mark an invalid ring in the ring properties list
#define INVALID_RING_OFFSET  (uint8_t)0xFF

// This structure is needed for mapping a RingID to it's corresponding name.
// The names will be used by the build scripts when generating the TOR.
typedef struct
{
    uint8_t       iv_torOffSet;
#ifndef __PPE__
    char          iv_name[MAX_RING_NAME_LENGTH];
#endif
    ChipletType_t iv_type;
} RingProperties_t;


//
// Universal infrastructure error codes
//
#define INFRASTRUCT_RC_SUCCESS                0
#define INFRASTRUCT_RC_FAILURE                1
#define INFRASTRUCT_RC_CODE_BUG               2
#define INFRASTRUCT_RC_USER_ERROR             3
#define INFRASTRUCT_RC_ENV_ERROR              4
#define INFRASTRUCT_RC_NOOF_CODES             5 // Do not use as RC code
//
// TOR specific error codes
//
#define TOR_SUCCESS                           INFRASTRUCT_RC_SUCCESS
#define TOR_FAILURE                           INFRASTRUCT_RC_FAILURE
#define TOR_CODE_BUG                          INFRASTRUCT_RC_CODE_BUG
#define TOR_USER_ERROR                        INFRASTRUCT_RC_USER_ERROR
#define TOR_INVALID_MAGIC_NUMBER              INFRASTRUCT_RC_NOOF_CODES + 1
#define TOR_INVALID_CHIPTYPE                  INFRASTRUCT_RC_NOOF_CODES + 3
#define TOR_INVALID_CHIPLET                   INFRASTRUCT_RC_NOOF_CODES + 4
#define TOR_INVALID_VARIANT                   INFRASTRUCT_RC_NOOF_CODES + 5
#define TOR_INVALID_RING_ID                   INFRASTRUCT_RC_NOOF_CODES + 6
#define TOR_INVALID_INSTANCE_ID               INFRASTRUCT_RC_NOOF_CODES + 7
#define TOR_INVALID_RING_BLOCK_TYPE           INFRASTRUCT_RC_NOOF_CODES + 8
#define TOR_UNSUPPORTED_RING_SECTION          INFRASTRUCT_RC_NOOF_CODES + 9
#define TOR_RING_NOT_FOUND                    INFRASTRUCT_RC_NOOF_CODES + 10
#define TOR_AMBIGUOUS_API_PARMS               INFRASTRUCT_RC_NOOF_CODES + 11
#define TOR_SECTION_NOT_FOUND                 INFRASTRUCT_RC_NOOF_CODES + 12
#define TOR_DD_LEVEL_NOT_FOUND                INFRASTRUCT_RC_NOOF_CODES + 13
#define TOR_OP_BUFFER_INVALID                 INFRASTRUCT_RC_NOOF_CODES + 14
#define TOR_OP_BUFFER_SIZE_EXCEEDED           INFRASTRUCT_RC_NOOF_CODES + 15
#define TOR_IMAGE_DOES_NOT_SUPPORT_CME        INFRASTRUCT_RC_NOOF_CODES + 16
#define TOR_IMAGE_DOES_NOT_SUPPORT_SGPE       INFRASTRUCT_RC_NOOF_CODES + 17
#define TOR_IMAGE_DOES_NOT_SUPPORT_DD_LEVEL   INFRASTRUCT_RC_NOOF_CODES + 18
#define TOR_IMAGE_DOES_NOT_SUPPORT_PPE_LEVEL  INFRASTRUCT_RC_NOOF_CODES + 19
#define TOR_RING_AVAILABLE_IN_RINGSECTION     INFRASTRUCT_RC_NOOF_CODES + 20
#define TOR_BUFFER_TOO_SMALL                  INFRASTRUCT_RC_NOOF_CODES + 21
#define TOR_TOO_MANY_DD_LEVELS                INFRASTRUCT_RC_NOOF_CODES + 22
#define TOR_OFFSET_TOO_BIG                    INFRASTRUCT_RC_NOOF_CODES + 23
#define TOR_NO_RINGS_FOR_VARIANT              INFRASTRUCT_RC_NOOF_CODES + 24

#ifndef __HOSTBOOT_MODULE // Only needed by ring_apply in EKB
int ringid_get_raw_ring_file_path( uint32_t       i_magic,
                                   RingVariant_t  i_ringVariant,
                                   char*          io_directory );
#endif

int ringid_get_noof_ring_ids( ChipType_t  i_chipType,
                              RingId_t*   o_numRingIds);

int ringid_get_noof_chiplets( ChipType_t  i_chipType,
                              uint32_t    i_torMagic,
                              uint8_t*    o_numChiplets );

int ringid_get_ring_list( ChipType_t       i_chipType,
                          RingId_t         i_ringId,
                          GenRingIdList**  o_ringIdList);

int ringid_get_properties( ChipType_t         i_chipType,
                           uint32_t           i_torMagic,
                           uint8_t            i_torVersion,
                           ChipletType_t      i_chipletType,
                           ChipletData_t**    o_chipletData,
                           GenRingIdList**    o_ringIdListCommon,
                           GenRingIdList**    o_ringIdListInstance,
                           RingVariantOrder** o_ringVariantOrder,
                           RingProperties_t** o_ringProps,
                           uint8_t*           o_numVariants);

#endif  // _COMMON_RINGID_H_


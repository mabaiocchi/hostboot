/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/isteps/istep07/call_mss_attr_update.C $               */
/*                                                                        */
/* OpenPOWER HostBoot Project                                             */
/*                                                                        */
/* Contributors Listed Below - COPYRIGHT 2015,2018                        */
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
/**
 *  @file call_mss_attr_update.C
 *  Contains the wrapper for istep 7.5
 */

/******************************************************************************/
// Includes
/******************************************************************************/
#include    <stdint.h>
#include    <map>
#include    <arch/pirformat.H>

#include    <trace/interface.H>
#include    <initservice/taskargs.H>
#include    <errl/errlentry.H>
#include    <errl/errlmanager.H>

#include    <isteps/hwpisteperror.H>

#include    <errl/errludtarget.H>
#include    <initservice/isteps_trace.H>
#include    <initservice/initserviceif.H>

// SBE
#include    <sbeif.H>

//  targeting support
#include    <targeting/common/commontargeting.H>
#include    <targeting/common/utilFilter.H>

// fapi2 support
#include <fapi2.H>
#include <fapi2/target.H>
#include <fapi2/plat_hwp_invoker.H>

#include <config.h>

// HWP
#include <p9_mss_attr_update.H>

namespace   ISTEP_07
{

using   namespace   ISTEP;
using   namespace   ISTEP_ERROR;
using   namespace   ERRORLOG;
using   namespace   TARGETING;


typedef struct procIds
{
    Target* proc;                                  // Proc
    ATTR_FABRIC_GROUP_ID_type groupIdDflt;         // Default Group ID
    ATTR_PROC_EFF_FABRIC_GROUP_ID_type groupIdEff; // Effective Group ID
    ATTR_FABRIC_GROUP_ID_type groupId;             // Desired Group ID
    ATTR_FABRIC_CHIP_ID_type chipIdDflt;           // Default Chip ID
    ATTR_PROC_EFF_FABRIC_CHIP_ID_type chipIdEff;   // Effective Chip ID
    ATTR_FABRIC_CHIP_ID_type chipId;               // Desired Chip ID
} procIds_t;

const uint8_t INVALID_PROC = 0xFF;


/**
 * @brief   check_proc0_memory_config   Check memory config for proc0
 *
 *  Loops through all processors gathering group ID and chip ID information for
 *  each one and determining which is proc0 based on having the smallest group
 *  ID / chip Id.
 *
 *  Checks that proc0 has associated functional dimms.  If it does not have any
 *  functional dimms behind it, look for a proc that does have functional dimms
 *  and swap IDs for these processors.
 *
 *  Loops through all processors again making sure that the effective group and
 *  chip IDs are the desired IDs.  If the IDs are not as desired, update the
 *  effective ID attributes and flag that an SBE Update is needed.
 *
 *  If SBE Update is needed, sync all attributes to the FSP and perform update.
 *
 * @param[in/out] io_istepErr  Error for this istep
 *
 * @return errlHndl_t       valid errlHndl_t handle if there was an error
 *                          NULL if no errors;
 */
errlHndl_t check_proc0_memory_config(IStepError & io_istepErr)
{
    errlHndl_t l_err = nullptr;
    bool l_updateNeeded = false;

    TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
              "check_proc0_memory_config entry");

    // Get all procs
    TargetHandleList l_procsList;
    getAllChips(l_procsList, TYPE_PROC);

    TARGETING::Target * l_sys = NULL;
    TARGETING::targetService().getTopLevelTarget(l_sys);

    // Loop through all procs getting IDs
    procIds_t l_procIds[l_procsList.size()];
    uint8_t i = 0;
    uint8_t l_proc0 = INVALID_PROC;
    uint8_t l_victim = INVALID_PROC;
    for (const auto & l_procChip : l_procsList)
    {
        l_procIds[i].proc = l_procChip;

        // Get Group IDs
        l_procIds[i].groupIdDflt =
            l_procChip->getAttr<ATTR_FABRIC_GROUP_ID>();
        l_procIds[i].groupIdEff =
            l_procChip->getAttr<ATTR_PROC_EFF_FABRIC_GROUP_ID>();
        l_procIds[i].groupId = l_procIds[i].groupIdDflt;

        // Get Chip IDs
        l_procIds[i].chipIdDflt =
            l_procChip->getAttr<ATTR_FABRIC_CHIP_ID>();
        l_procIds[i].chipIdEff =
            l_procChip->getAttr<ATTR_PROC_EFF_FABRIC_CHIP_ID>();
        l_procIds[i].chipId = l_procIds[i].chipIdDflt;

        // Check if this proc should be tracked as proc0
        if(l_proc0 == INVALID_PROC)
        {
            // No proc0, make initial assignment
            l_proc0 = i;
        }
        else if ( PIR_t::createChipId(l_procIds[i].groupId,
                                      l_procIds[i].chipId) <
                  PIR_t::createChipId(l_procIds[l_proc0].groupId,
                                      l_procIds[l_proc0].chipId) )
        {
            // Smaller group ID / chip ID, replace assignment
            l_proc0 = i;
        }

        TRACDCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                  "check_proc0_memory_config: Initial settings for "
                  "Proc %.8X\n"
                  "  groupIdDflt = %d, groupIdEff = %d, groupId = %d\n"
                  "  chipIdDflt = %d, chipIdEff = %d, chipId = %d",
                  get_huid(l_procIds[i].proc),
                  l_procIds[i].groupIdDflt,
                  l_procIds[i].groupIdEff,
                  l_procIds[i].groupId,
                  l_procIds[i].chipIdDflt,
                  l_procIds[i].chipIdEff,
                  l_procIds[i].chipId);

        // Increment index
        i++;
    }

    // Get the functional DIMMs for proc0
    PredicateHwas l_functional;
    l_functional.functional(true);
    TargetHandleList l_dimms;
    PredicateCTM l_dimm(CLASS_LOGICAL_CARD, TYPE_DIMM);
    PredicatePostfixExpr l_checkExprFunctional;
    l_checkExprFunctional.push(&l_dimm).push(&l_functional).And();
    targetService().getAssociated(l_dimms,
                                  l_procIds[l_proc0].proc,
                                  TargetService::CHILD_BY_AFFINITY,
                                  TargetService::ALL,
                                  &l_checkExprFunctional);

    TARGETING::ATTR_PAYLOAD_KIND_type payload_kind =
            l_sys->getAttr<TARGETING::ATTR_PAYLOAD_KIND>();

    TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
              "check_proc0_memory_config: %d functional dimms behind proc0 "
              "%.8X",
              l_dimms.size(), get_huid(l_procIds[l_proc0].proc) );

    if(l_dimms.empty())
    {
        TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                  "check_proc0_memory_config: proc0 %.8X has no functional "
                  "dimms behind it",
                  get_huid(l_procIds[l_proc0].proc) );

        // Loop through all procs to find ones for memory remap
        for (i = 0; i < l_procsList.size(); i++)
        {
            // If proc0, then continue
            if(i == l_proc0)
            {
                continue;
            }

            // Get the functional DIMMs for the proc
            targetService().getAssociated(l_dimms,
                                          l_procIds[i].proc,
                                          TargetService::CHILD_BY_AFFINITY,
                                          TargetService::ALL,
                                          &l_checkExprFunctional);

            // If proc does not have memory, then continue
            if(l_dimms.empty())
            {
                TRACDCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          "check_proc0_memory_config: Proc %.8X has no  "
                          "functional dimms behind it",
                          get_huid(l_procIds[i].proc) );

                continue;
            }

            // If our master proc doesn't have memory, and we're on a phyp
            // system, we want to use this proc's memory instead.
#if 0
            // TODO RTC: 181139. This support can not be put into place
            // until we're able to use the Get Capabilities function
            TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                      "check_proc0_memory_config: Payload kind is %llx",
                      payload_kind);
            if(payload_kind == TARGETING::PAYLOAD_KIND_PHYP)
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                         "check_proc0_memory_config: We are in a PHYP system, "
                         "setting master to use alt memory from proc %llx.",
                         get_huid(l_procIds[i].proc));

                uint8_t l_chipID = l_procIds[i].chipId;
                uint8_t l_groupID = l_procIds[i].groupId;

                TargetHandle_t l_masterProc = NULL;
                targetService().masterProcChipTargetHandle(l_masterProc);

                uint8_t l_proc_memory = l_masterProc->getAttr<
                        TARGETING::ATTR_PROC_MEM_TO_USE>();

                if( l_proc_memory != ((l_groupID <<3) | l_chipID))
                {
                    l_masterProc->setAttr<TARGETING::ATTR_PROC_MEM_TO_USE>(
                                ((l_groupID << 3) | l_chipID));

                    l_updateNeeded = true;
                    // Leave loop after switching memory
                    break;
                }
            }else
            {
#endif
            // Use this proc for swapping memory with proc0
            l_victim = i;

            // Set the desired proc0 IDs from swapped proc's default IDs
            l_procIds[l_proc0].groupId = l_procIds[l_victim].groupIdDflt;
            l_procIds[l_proc0].chipId = l_procIds[l_victim].chipIdDflt;
            TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                      "check_proc0_memory_config: proc0 %.8X is set to use "
                      "groupId %d and chipId %d",
                      get_huid(l_procIds[l_proc0].proc),
                      l_procIds[l_proc0].groupId,
                      l_procIds[l_proc0].chipId);

            // Set the desired IDs for the swapped proc from proc0 defaults
            l_procIds[l_victim].groupId = l_procIds[l_proc0].groupIdDflt;
            l_procIds[l_victim].chipId = l_procIds[l_proc0].chipIdDflt;
            TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                      "check_proc0_memory_config: Proc %.8X is set to use "
                      "groupId %d and chipId %d",
                      get_huid(l_procIds[l_victim].proc),
                      l_procIds[l_victim].groupId,
                      l_procIds[l_victim].chipId);

            // Leave loop after swapping memory
            break;
#if 0
            }
#endif
        }

        if(payload_kind != TARGETING::PAYLOAD_KIND_PHYP)
        {
            // Check that a victim was found
            assert( l_victim < l_procsList.size(), "No swap match found" );
        }
    }
#if 0
    //  TODO RTC: 181139. This support can not be put into place
    //  until we're able to use the Get Capabilities function
    else if( !(l_dimms.empty()) &&
               (payload_kind == TARGETING::PAYLOAD_KIND_PHYP) )
    {
        // If the memory isn't empty, and we're on a phyp system,
        // we want to verify that we're set up to use the correct memory
        uint8_t l_chipID = l_procIds[i].chipId;
        uint8_t l_groupID = l_procIds[i].groupId;

        TargetHandle_t l_masterProc = NULL;
        targetService().masterProcChipTargetHandle(l_masterProc);

        uint8_t l_proc_memory =
                l_masterProc->getAttr<TARGETING::ATTR_PROC_MEM_TO_USE>();

        if( l_proc_memory != ((l_groupID <<3) | l_chipID))
        {
            l_masterProc->setAttr<TARGETING::ATTR_PROC_MEM_TO_USE>(
                                ((l_groupID << 3) | l_chipID));

            l_updateNeeded = true;
        }
    }
#endif

    if(payload_kind != TARGETING::PAYLOAD_KIND_PHYP)
    {
#if 0
        // TODO RTC: 181139. This support can not be put into place
        // until we're able to use the Get Capabilities function
        TargetHandle_t l_masterProc = NULL;
        targetService().masterProcChipTargetHandle(l_masterProc);

        // Check the attribute, and default it to proc0 if
        // it doesn't match.
        uint8_t l_proc_memory =
                    l_masterProc->getAttr<TARGETING::ATTR_PROC_MEM_TO_USE>();

        if( l_proc_memory != 0)
        {
            l_masterProc->setAttr<TARGETING::ATTR_PROC_MEM_TO_USE>(0);

            l_updateNeeded = true;
        }
#endif
        // Loop through all procs detecting that IDs are set correctly
        for (i = 0; i < l_procsList.size(); i++)
        {
            TRACDCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                  "check_proc0_memory_config: Compare settings for "
                  "Proc %.8X\n"
                  "  groupIdEff = %d, groupId = %d\n"
                  "  chipIdEff = %d, chipId = %d",
                  get_huid(l_procIds[i].proc),
                  l_procIds[i].groupIdEff,
                  l_procIds[i].groupId,
                  l_procIds[i].chipIdEff,
                  l_procIds[i].chipId);

            if((l_procIds[i].groupId != l_procIds[i].groupIdEff) ||
               (l_procIds[i].chipId != l_procIds[i].chipIdEff) )
            {
                // Update attributes
                (l_procIds[i].proc)->
                  setAttr<ATTR_PROC_EFF_FABRIC_GROUP_ID>(l_procIds[i].groupId);
                (l_procIds[i].proc)->
                  setAttr<ATTR_PROC_EFF_FABRIC_CHIP_ID>(l_procIds[i].chipId);

                l_updateNeeded = true;
            }

            TRACDCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                  "check_proc0_memory_config: Current attribute "
                  "settings for Proc %.8X\n"
                  "  ATTR_PROC_EFF_FABRIC_GROUP_ID = %d\n"
                  "  ATTR_FABRIC_GROUP_ID = %d\n"
                  "  ATTR_PROC_EFF_FABRIC_CHIP_ID = %d\n"
                  "  ATTR_FABRIC_CHIP_ID = %d",
                  get_huid(l_procIds[i].proc),
                  (l_procIds[i].proc)->
                      getAttr<ATTR_PROC_EFF_FABRIC_GROUP_ID>(),
                  (l_procIds[i].proc)->getAttr<ATTR_FABRIC_GROUP_ID>(),
                  (l_procIds[i].proc)->
                      getAttr<ATTR_PROC_EFF_FABRIC_CHIP_ID>(),
                  (l_procIds[i].proc)->getAttr<ATTR_FABRIC_CHIP_ID>());
        }

    }

    if(l_updateNeeded)
    {
        do
        {
            // Sync all attributes to the FSP
            l_err = syncAllAttributesToFsp();
            if( l_err )
            {
                // Something failed on the sync.  Commit the error here
                // and continue with the Re-IPL Request
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          ERR_MRK"check_proc0_memory_config() - Error "
                          "syncing attributes to FSP, RC=0x%X, PLID=0x%lX",
                          ERRL_GETRC_SAFE(l_err),
                          ERRL_GETPLID_SAFE(l_err));

                io_istepErr.addErrorDetails(l_err);
                errlCommit(l_err, HWPF_COMP_ID);
            }
            else
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          INFO_MRK"check_proc0_memory_config() - Sync "
                          "Attributes to FSP" );
            }

            // Rebuild SBE image and trigger reconfig loop
            l_err = SBE::updateProcessorSbeSeeproms();

            if( l_err )
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                          ERR_MRK"check_proc0_memory_config() - Error calling "
                          "updateProcessorSbeSeeproms, RC=0x%X, PLID=0x%lX",
                          ERRL_GETRC_SAFE(l_err),
                          ERRL_GETPLID_SAFE(l_err));

                break;
            }
        } while(0);
    }

    return l_err;
} // end check_proc0_memory_config()


//
//  Wrapper function to call mss_attr_update
//
void*    call_mss_attr_update( void *io_pArgs )
{
    IStepError l_StepError;

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace, "call_mss_attr_update entry");
    errlHndl_t l_err = NULL;

    // Check the memory on proc0 chip
    l_err = check_proc0_memory_config(l_StepError);

    if (l_err)
    {
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   "ERROR 0x%.8X:  check_proc0_memory_config",
                   l_err->reasonCode());

        // Ensure istep error created and has same plid as this error
        l_StepError.addErrorDetails( l_err );
        errlCommit( l_err, HWPF_COMP_ID );
    }
    else
    {
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   "SUCCESS:  check_proc0_memory_config");
    }

    if (l_StepError.isNull())
    {
        // Get all functional MCS chiplets
        TARGETING::TargetHandleList l_mcsTargetList;
        getAllChiplets(l_mcsTargetList, TYPE_MCS);

        for (const auto & l_mcsTarget: l_mcsTargetList)
        {
            const fapi2::Target<fapi2::TARGET_TYPE_MCS>
                l_fapi2_mcs_target(l_mcsTarget);

            TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                "Running p9_mss_attr_update HWP on "
                "MCS target HUID %.8X", TARGETING::get_huid(l_mcsTarget));
            FAPI_INVOKE_HWP(l_err, p9_mss_attr_update, l_fapi2_mcs_target);
            if(l_err)
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                    "ERROR 0x%.8X : p9_mss_attr_update HWP returned "
                    "error for HUID %.8x",
                    l_err->reasonCode(), TARGETING::get_huid(l_mcsTarget));
                l_StepError.addErrorDetails(l_err);
                errlCommit( l_err, HWPF_COMP_ID );
            }
        }
    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace, "call_mss_attr_update exit" );

    return l_StepError.getErrorHandle();
}

};   // end namespace

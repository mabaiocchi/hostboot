/* IBM_PROLOG_BEGIN_TAG                                                   */
/* This is an automatically generated prolog.                             */
/*                                                                        */
/* $Source: src/usr/isteps/istep21/call_host_runtime_setup.C $            */
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

#include <errl/errlentry.H>
#include <errl/errlmanager.H>
#include <initservice/isteps_trace.H>
#include <isteps/hwpisteperror.H>
#include <isteps/istep_reasoncodes.H>
#include <isteps/pm/pm_common_ext.H>
#include <initservice/initserviceif.H>
#include <initservice/istepdispatcherif.H>
#include <vfs/vfs.H>
#include <htmgt/htmgt.H>
#include <runtime/runtime.H>
#include <runtime/customize_attrs_for_payload.H>
#include <targeting/common/util.H>
#include <targeting/targplatutil.H>
#include <vpd/vpd_if.H>
#include <util/utiltce.H>
#include <util/utilmclmgr.H>
#include <map>
#include <sys/internode.h>
#include <mbox/ipc_msg_types.H>

#include <secureboot/service.H>
#include <secureboot/containerheader.H>
#include <sys/mm.h>
//SBE interfacing
#include    <sbeio/sbeioif.H>
#include    <sys/misc.h>

#include <hbotcompid.H>

#ifdef CONFIG_IPLTIME_CHECKSTOP_ANALYSIS
  #include <isteps/pm/occAccess.H>
  #include <isteps/pm/occCheckstop.H>
#endif

using   namespace   ERRORLOG;
using   namespace   ISTEP;
using   namespace   ISTEP_ERROR;
using   namespace   TARGETING;

namespace ISTEP_21
{


// Direct non-master nodes to close and disable their TCEs
errlHndl_t closeNonMasterTces(void)
{
    errlHndl_t l_err = nullptr;
    uint64_t nodeid = TARGETING::UTIL::getCurrentNodePhysId();

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
               ENTER_MRK"closeNonMasterTces(): nodeid=%d", nodeid);

    // keep track of the number of messages we send so we
    // know how many responses to expect
    uint64_t msg_count = 0;

    do{

        TARGETING::Target * sys = nullptr;
        TARGETING::targetService().getTopLevelTarget( sys );
        assert(sys != nullptr, "closeNonMasterTces() system target is nullptr");

        TARGETING::ATTR_HB_EXISTING_IMAGE_type hb_images =
            sys->getAttr<TARGETING::ATTR_HB_EXISTING_IMAGE>();
        // This msgQ catches the node responses from the commands
        msg_q_t msgQ = msg_q_create();
        l_err = MBOX::msgq_register(MBOX::HB_CLOSE_TCES_MSGQ,msgQ);

        if(l_err)
        {
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                       "closeNonMasterTces(): MBOX::msgq_register failed!" );
            break;
        }

        // loop thru rest all nodes -- sending msg to each
        TARGETING::ATTR_HB_EXISTING_IMAGE_type mask = 0x1 <<
          ((sizeof(TARGETING::ATTR_HB_EXISTING_IMAGE_type) * 8) -1);

        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   "closeNonMasterTces(): HB_EXISTING_IMAGE (mask) = 0x%X, "
                   "(hb_images=0x%X)",
                   mask, hb_images);

        for (uint64_t l_node=0; (l_node < MAX_NODES_PER_SYS); l_node++ )
        {
            if (l_node == nodeid)
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "closeNonMasterTces(): don't send IPC_CLOSE_TCES "
                           "message to master node %d",
                           nodeid );
                continue;
            }

            if( 0 != ((mask >> l_node) & hb_images ) )
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "closeNonMasterTces(): send IPC_CLOSE_TCES message "
                           "to node %d",
                           l_node );

                msg_t * msg = msg_allocate();
                msg->type = IPC::IPC_CLOSE_TCES;
                msg->data[0] = l_node;      // destination node
                msg->data[1] = nodeid;      // respond to this node

                // send the message to the slave hb instance
                l_err = MBOX::send(MBOX::HB_IPC_MSGQ, msg, l_node);

                if( l_err )
                {
                    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                               "closeNonMasterTces(): MBOX::send to node %d "
                               "failed",
                               l_node);
                    break;
                }

                ++msg_count;

            } // end of node to process
        } // end for loop on nodes

        // wait for a response to each message we sent
        if( l_err == nullptr )
        {
            //$TODO RTC:189356 - need timeout here
            while(msg_count)
            {
                msg_t * response = msg_wait(msgQ);
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "closeNonMasterTces(): IPC_CLOSE_TCES : node %d "
                           "completed",
                           response->data[0]);
                msg_free(response);
                --msg_count;
            }
        }

        MBOX::msgq_unregister(MBOX::HB_CLOSE_TCES_MSGQ);
        msg_q_destroy(msgQ);

    } while(0);

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
               EXIT_MRK"closeNonMasterTces(): l_err rc = 0x%X, msg_count=%d",
               ERRL_GETRC_SAFE(l_err), msg_count );

    return l_err;
}


// Verify PAYLOAD and Move PAYLOAD+HDAT from Temporary TCE-related
// memory region to the proper location
errlHndl_t verifyAndMovePayload(void)
{
    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
               ENTER_MRK"verifyAndMovePayload()" );

    errlHndl_t l_err = nullptr;
    void * payload_tmp_virt_addr = nullptr;
    void * payloadBase_virt_addr = nullptr;
    void * hdat_tmp_virt_addr = nullptr;
    void * hdat_final_virt_addr = nullptr;

    enum Map_FailLocs_t {
        NO_MAP_FAIL             = 0x0,
        PAYLOAD_TMP_MAP_FAIL    = 0x1, // payload_tmp_virt_addr
        PAYLOAD_BASE_MAP_FAIL   = 0x2, // payloadBase_virt_addr
        HDAT_TMP_MAP_FAIL       = 0x3, // hdat_tmp_virt_addr
        HDAT_FINAL_MAP_FAIL     = 0x4, // hdat_final_virt_addr

        PAYLOAD_TMP_UNMAP_FAIL  = 0x5, // payload_tmp_virt_addr
        PAYLOAD_BASE_UNMAP_FAIL = 0x6, // payloadBase_virt_addr
        HDAT_TMP_UNMAP_FAIL     = 0x7, // hdat_tmp_virt_addr
        HDAT_FINAL_UNMAP_FAIL   = 0x8, // hdat_final_virt_addr
    };

    Map_FailLocs_t blockMapFail = NO_MAP_FAIL;

    // Make sure these constants are page-aligned, as they are used below for
    // mm_block_map:
    static_assert((MCL_TMP_ADDR % PAGESIZE) == 0, "verifyAndMovePayload() MCL_TMP_ADDR isn't page-aligned");
    static_assert((MCL_TMP_SIZE % PAGESIZE) == 0, "verifyAndMovePayload() MCL_TMP_SIZE isn't page-aligned");
    static_assert((HDAT_TMP_ADDR % PAGESIZE) == 0, "verifyAndMovePayload() HDAT_TMP_ADDR isn't page-aligned");

    do{

    if (!TCE::utilUseTcesForDmas())
    {
        // If TCEs were not enabled, no need to verify and move
        break;
    }

    TARGETING::ATTR_PAYLOAD_KIND_type payload_kind =
      TARGETING::PAYLOAD_KIND_NONE;
    bool is_phyp = TARGETING::is_phyp_load(&payload_kind);

    // Only Supporting PHYP/POWERVM and SAPPHIRE/OPAL at this time
    // @TODO RTC 183831 in case we ever need to support Payload AVPS
    if( !(TARGETING::PAYLOAD_KIND_PHYP == payload_kind ) &&
        !(TARGETING::PAYLOAD_KIND_SAPPHIRE == payload_kind ) )
    {
        break;
    }

    // Setup componend IDs and strings
    const MCL::ComponentID l_compId = is_phyp ? MCL::g_PowervmCompId
                                              : MCL::g_OpalCompId;
    MCL::CompIdString l_IdStr = {};
    MCL::compIdToString(l_compId, l_IdStr);

    // Get Temporary Virtual Address To Payload
    // - Need to make Memory spaces HRMOR-relative
    uint64_t hrmorVal = cpu_spr_value(CPU_SPR_HRMOR);
    uint64_t payload_tmp_phys_addr = hrmorVal - VMM_HRMOR_OFFSET + MCL_TMP_ADDR;
    uint64_t payload_size          = MCL_TMP_SIZE;

    payload_tmp_virt_addr = mm_block_map(
                             reinterpret_cast<void*>(payload_tmp_phys_addr),
                             payload_size);

    // Check for nullptr being returned
    if (payload_tmp_virt_addr == nullptr)
    {
        blockMapFail = PAYLOAD_TMP_MAP_FAIL;
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   ERR_MRK"verifyAndMovePayload(): Fail to mm_block_map "
                   "payload_tmp_virt_addr (loc=0x%X)",
                   blockMapFail);
        // Error log created outside of do-while loop
        break;
    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,"verifyAndMovePayload() "
               "Processing PAYLOAD_KIND = %d (Id='%s') (is_phyp=%d): "
               "physAddr=0x%.16llX, virtAddr=0x%.16llX",
               payload_kind, l_IdStr, is_phyp, payload_tmp_phys_addr,
               payload_tmp_virt_addr );


    // Parse Container Header
    SECUREBOOT::ContainerHeader l_conHdr;
    l_err = l_conHdr.setHeader(payload_tmp_virt_addr);
    if (l_err)
    {
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   ERR_MRK"verifyAndMovePayload(): Fail to parse container "
                   "header at payload_tmp_virt_addr = 0x%.16llX",
                   payload_tmp_virt_addr);
        break;
    }

    // If in Secure Mode Verify PHYP at Temporary TCE-related Memory Location
    if (SECUREBOOT::enabled() && is_phyp)
    {
        TRACDCOMP( ISTEPS_TRACE::g_trac_isteps_trace,"verifyAndMovePayload() "
                   "Verifying PAYLOAD: physAddr=0x%.16llX, virtAddr=0x%.16llX",
                   payload_tmp_phys_addr, payload_tmp_virt_addr );

        // Verify Container
        l_err = SECUREBOOT::verifyContainer(payload_tmp_virt_addr);
        if (l_err)
        {
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                "verifyAndMovePayload(): failed verifyContainer");
            l_err->collectTrace("ISTEPS_TRACE",256);
            SECUREBOOT::handleSecurebootFailure(l_err);
            assert(false,"Bug! handleSecurebootFailure shouldn't return!");
        }

        // Get PAYLOAD size from verified Header
        payload_size = l_conHdr.payloadTextSize() + PAGESIZE;
        assert(payload_size <= MCL_TMP_SIZE, "verifyAndMovePayload payload_size 0x%X must be <= MCL_TMP_SIZE (0x%X)", payload_size, MCL_TMP_SIZE );

        // Verify ASCII Component Id in the Secure Header matches expected value
        l_err = SECUREBOOT::verifyComponentId(l_conHdr, l_IdStr);
        if (l_err)
        {
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                       ERR_MRK"verifyAndMovePayload(): Fail to verify component"
                       "Id %s in header at payload_tmp_virt_addr = 0x%.16llX",
                       l_IdStr, payload_tmp_virt_addr);
            break;
        }
    }

    // Extend PAYLOAD
    l_err = MCL::MasterContainerLidMgr::tpmExtend(l_compId, l_conHdr);
    if (l_err)
    {
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   ERR_MRK"verifyAndMovePayload(): Fail to tpmExend "
                   "Id %s in header at payload_tmp_virt_addr = 0x%.16llX",
                   l_IdStr, payload_tmp_virt_addr);
        break;
    }

    // Move PAYLOAD to Final Location
    // Get Target Service, and the system target.
    TargetService& tS = targetService();
    TARGETING::Target* sys = nullptr;
    (void) tS.getTopLevelTarget( sys );
    assert(sys != nullptr, "verifyAndMovePayload() sys target is NULL");
    uint64_t payloadBase = sys->getAttr<TARGETING::ATTR_PAYLOAD_BASE>();

    payloadBase = payloadBase * MEGABYTE;

    if (is_phyp)
    {
        // Put header before PAYLOAD_BASE for PHYP/POWERVM
        payloadBase -= PAGESIZE;
    }
    else
    {
        // Move virtual address past OPAL header for memcpy below
        payload_tmp_virt_addr = reinterpret_cast<void*>(
                                  reinterpret_cast<uint64_t>(
                                    payload_tmp_virt_addr) +
                                    PAGESIZE);
        payload_size -= PAGESIZE;
    }

    payloadBase_virt_addr = mm_block_map(
                               reinterpret_cast<void*>(payloadBase),
                               payload_size);

    // Check for nullptr being returned
    if (payloadBase_virt_addr == nullptr)
    {
        blockMapFail = PAYLOAD_BASE_MAP_FAIL;
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   ERR_MRK"verifyAndMovePayload(): Fail to mm_block_map "
                   "payloadBase_virt_addr (loc=0x%X)",
                   blockMapFail);
        // Error log created outside of do-while loop
        break;
    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                "verifyAndMovePayload(): Copy PAYLOAD from 0x%.16llX (va="
                "0x%llX) to PAYLOAD_BASE = 0x%.16llX (va=0x%llX), size=0x%llX",
                payload_tmp_phys_addr, payload_tmp_virt_addr, payloadBase,
                payloadBase_virt_addr, payload_size);

    memcpy (payloadBase_virt_addr,
            payload_tmp_virt_addr,
            payload_size);


    // Move HDAT into its proper place after it was temporarily put into
    // HDAT_TMP_ADDR-relative-to-HRMOR (HDAT_TMP_SIZE) by the FSP via TCEs
    uint64_t hdat_tmp_phys_addr = hrmorVal - VMM_HRMOR_OFFSET + HDAT_TMP_ADDR;

    hdat_tmp_virt_addr = mm_block_map(
                            reinterpret_cast<void*>(hdat_tmp_phys_addr),
                            HDAT_TMP_SIZE);

    // Check for nullptr being returned
    if (hdat_tmp_virt_addr == nullptr)
    {
        blockMapFail = HDAT_TMP_MAP_FAIL;
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   ERR_MRK"verifyAndMovePayload(): Fail to mm_block_map "
                   "hdat_tmp_virt_addr (loc=0x%X)",
                   blockMapFail);
        // Error log created outside of do-while loop
        break;
    }

    // Determine location of HDAT from NACA section of PAYLOAD
    uint64_t hdat_cpy_offset = 0;

    // Convert the move payloadBase_va to after secure header for PHYP
    uint64_t payloadBase_va = reinterpret_cast<uint64_t>(payloadBase_virt_addr);
    payloadBase_va += (is_phyp ? PAGESIZE : 0 );

    RUNTIME::findHdatLocation(payloadBase_va, hdat_cpy_offset);

    // PHYP images require adding 1 PAGESIZE since our virtual address starts
    // at the secure header of PAYLOAD before PAYLOAD_BASE
    if (is_phyp)
    {
        hdat_cpy_offset += PAGESIZE;
    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
               "verifyAndMovePayload(): hdat_copy_offset = 0x%X",
               hdat_cpy_offset);

    hdat_final_virt_addr = mm_block_map(
                              reinterpret_cast<void*>(payloadBase +
                                                      hdat_cpy_offset),
                              HDAT_TMP_SIZE);

    // Check for nullptr being returned
    if (hdat_final_virt_addr == nullptr)
    {
        blockMapFail = HDAT_FINAL_MAP_FAIL;
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                   ERR_MRK"verifyAndMovePayload(): Fail to mm_block_map "
                   "hdat_final_virt_addr (loc=0x%X)",
                   blockMapFail);
        // Error log created outside of do-while loop
        break;
    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                "verifyAndMovePayload(): Copy HDAT from 0x%.16llX (va="
                "0x%llX) to HDAT_FINAL = 0x%.16llX (va=0x%llX), size=0x%llX",
                hdat_tmp_phys_addr, hdat_tmp_virt_addr,
                payloadBase+hdat_cpy_offset, hdat_final_virt_addr, HDAT_TMP_SIZE);

    memcpy(hdat_final_virt_addr,
           hdat_tmp_virt_addr,
           HDAT_TMP_SIZE);

    } while(0);

    // Handle Possible mm_block_map fails here
    if (blockMapFail != NO_MAP_FAIL)
    {
        // Trace done above. Just create error log here.

        /*@
         * @errortype
         * @reasoncode       RC_MM_MAP_ERR
         * @severity         ERRL_SEV_UNRECOVERABLE
         * @moduleid         MOD_VERIFY_AND_MOVE_PAYLOAD
         * @userdata1        Map Fail Location
         * @userdata2        <UNUSED>
         * @devdesc          mm_block_map failed and returned nullptr
         * @custdesc         A problem occurred during the IPL
         *                   of the system.
         */
        errlHndl_t map_err = new ErrlEntry(ERRL_SEV_UNRECOVERABLE,
                                           MOD_VERIFY_AND_MOVE_PAYLOAD,
                                           RC_MM_MAP_ERR,
                                           blockMapFail,
                                           0x0,
                                           true /*Add HB SW Callout*/);
        map_err->collectTrace("ISTEPS_TRACE",256);

        // if l_err already exists just commit this log; otherwise set to l_err
        if (l_err == nullptr)
        {
            l_err = map_err;
            map_err = nullptr;
        }
        else
        {
            errlCommit(map_err, ISTEP_COMP_ID);
        }
    }

    // Cleanup/Unmap Memory Blocks
    std::map<void*,Map_FailLocs_t> ptrs_to_unmap =
    {
        { payload_tmp_virt_addr, PAYLOAD_TMP_UNMAP_FAIL },
        { payloadBase_virt_addr, PAYLOAD_BASE_UNMAP_FAIL },
        { hdat_tmp_virt_addr,    HDAT_TMP_UNMAP_FAIL },
        { hdat_final_virt_addr,  HDAT_FINAL_UNMAP_FAIL },
    };

    for ( auto ptr : ptrs_to_unmap )
    {
        if (ptr.first == nullptr)
        {
            continue;
        }

        int rc = mm_block_unmap(ptr.first);

        if (rc != 0)
        {
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                       ERR_MRK"verifyAndMovePayload(): Failed to unmap "
                       "0x%.16llX (loc=0x%X)",
                       ptr.first, ptr.second);

            /*@
             * @errortype
             * @reasoncode       RC_MM_UNMAP_ERR
             * @severity         ERRL_SEV_UNRECOVERABLE
             * @moduleid         MOD_VERIFY_AND_MOVE_PAYLOAD
             * @userdata1        Map Fail Location
             * @userdata2        Return code from mm_block_unmap
             * @devdesc          mm_block_unmap failed and returned nullptr
             * @custdesc         A problem occurred during the IPL
             *                   of the system.
             */
            errlHndl_t unmap_err = new ErrlEntry(ERRL_SEV_UNRECOVERABLE,
                                                 MOD_VERIFY_AND_MOVE_PAYLOAD,
                                                 RC_MM_UNMAP_ERR,
                                                 ptr.second,
                                                 rc,
                                                 true /*Add HB SW Callout*/);
            unmap_err->collectTrace("ISTEPS_TRACE",256);

            // if l_err already exists just commit this log;
            // otherwise set to l_err
            if (l_err == nullptr)
            {
                l_err = unmap_err;
                unmap_err = nullptr;
            }
            else
            {
                errlCommit(unmap_err, ISTEP_COMP_ID);
            }
        }
    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
               EXIT_MRK"verifyAndMovePayload(): l_err rc = 0x%X",
               ERRL_GETRC_SAFE(l_err) );

    return l_err;
}


void* call_host_runtime_setup (void *io_pArgs)
{
    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
            "call_host_runtime_setup entry" );

    errlHndl_t l_err = NULL;

    IStepError l_StepError;

    // Need to wait here until Fsp tells us go
    INITSERVICE::waitForSyncPoint();

    do
    {
        // Close PAYLOAD TCEs
        if (TCE::utilUseTcesForDmas())
        {
            l_err = TCE::utilClosePayloadTces();
            if ( l_err )
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "Failed TCE::utilClosePayloadTces" );
                // break from do loop if error occurred
                break;
            }

            // Close TCEs on non-master nodes
            l_err = closeNonMasterTces();
            if ( l_err )
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "Failed closeNonMasterTces" );
                // break from do loop if error occurred
                break;
            }
        }

        // Need to load up the runtime module if it isn't already loaded
        if (  !VFS::module_is_loaded( "libruntime.so" ) )
        {
            l_err = VFS::module_load( "libruntime.so" );

            if ( l_err )
            {
                //  load module returned with errl set
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                        "Could not load runtime module" );
                // break from do loop if error occured
                break;
            }
        }

        //Need to send System Configuration down to SBE for all HB
        //instances
        l_err = RUNTIME::sendSBESystemConfig();
        if(l_err)
        {
            break;
        }

        // Verify PAYLOAD and Move PAYLOAD+HDAT from Temporary TCE-related
        // memory region to the proper location
        l_err = verifyAndMovePayload();
        if(l_err)
        {
            break;
        }

        // Map the Host Data into the VMM if applicable
        l_err = RUNTIME::load_host_data();
        if( l_err )
        {
            break;
        }

        // Fill in Hostboot runtime data if there is a PAYLOAD
        if( !(TARGETING::is_no_load()) )
        {
            // API call to fix up the secureboot fields
            l_err = RUNTIME::populate_hbSecurebootData();
            if ( l_err )
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                            "Failed hbSecurebootData setup" );
                break;
            }

            // API call to populate the TPM Info fields
            l_err = RUNTIME::populate_hbTpmInfo();
            if ( l_err )
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                            "Failed hbTpmInfo setup" );
                break;
            }
        }

        l_err = RUNTIME::persistent_rwAttrRuntimeCheck();
        if ( l_err )
        {
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                        "Failed persistent_rwAttrRuntimeCheck()" );
            break;
        }

#ifdef CONFIG_START_OCC_DURING_BOOT
        bool l_activatePM = TARGETING::is_sapphire_load();
#else
        bool l_activatePM = false;
#endif

        if(l_activatePM)
        {
            TARGETING::Target* l_failTarget = NULL;
            bool pmStartSuccess = true;

            l_err = loadAndStartPMAll(HBPM::PM_LOAD, l_failTarget);
            if (l_err)
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "loadAndStartPMAll failed");

                // Commit the error and continue with the istep
                errlCommit(l_err, ISTEP_COMP_ID);
                pmStartSuccess = false;
            }

#ifdef CONFIG_HTMGT
            // Report PM status to HTMGT
            HTMGT::processOccStartStatus(pmStartSuccess,l_failTarget);
#else
            // Verify all OCCs have reached the checkpoint
            if (pmStartSuccess)
            {
                l_err = HBPM::verifyOccChkptAll();
                if (l_err)
                {
                    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                               "verifyOccCheckpointAll failed");

                    // Commit the error and continue with the istep
                    errlCommit(l_err, ISTEP_COMP_ID);
                }
            }
#endif
        }

#ifdef CONFIG_IPLTIME_CHECKSTOP_ANALYSIS
        if(TARGETING::is_phyp_load() )
        {
            //Explicity clearing the SRAM flag before starting Payload.
            //This tells the OCC bootloader where to pull the OCC image from
            //0: mainstore, 1: SRAM. We want to use mainstore after this point

            //Get master proc
            TARGETING::TargetService & tS = TARGETING::targetService();
            TARGETING::Target* masterproc = NULL;
            tS.masterProcChipTargetHandle( masterproc );

            //Clear (up to and including the IPL flag)
            size_t sz_data = HBOCC::OCC_OFFSET_IPL_FLAG + 6;
            size_t sz_dw   = sizeof(uint64_t);
            uint64_t l_occAppData[(sz_data+(sz_dw-1))/sz_dw];
            memset( l_occAppData, 0x00, sizeof(l_occAppData) );

            const uint32_t l_SramAddrApp = HBOCC::OCC_405_SRAM_ADDRESS;
            l_err = HBOCC::writeSRAM( masterproc, l_SramAddrApp, l_occAppData,
                                      sz_data );
            if(l_err)
            {
                TRACFCOMP(ISTEPS_TRACE::g_trac_isteps_trace,
                                "Error in writeSRAM of 0");
                break;
            }
        }
#endif


#if 0 //@TODO-RTC:164022-Support max pstate without OCC
#ifdef CONFIG_SET_NOMINAL_PSTATE
        // Speed up processors.
        l_err = setMaxPstate();
        if (l_err)
        {
            l_err->setSev(ERRORLOG::ERRL_SEV_PREDICTIVE);
            ERRORLOG::errlCommit(l_err, ISTEP_COMP_ID);
        }
#endif
#endif

        if( TARGETING::is_sapphire_load()
            && (!INITSERVICE::spBaseServicesEnabled()) )
        {
            //@fixme-RTC:172836-broken for HDAT mode?
            // Update the VPD switches for golden side boot
            // Must do this before building the devtree
            l_err = VPD::goldenSwitchUpdate();
            if ( l_err )
            {
                break;
            }
        }
        else if( TARGETING::is_phyp_load() )
        {
            //Update the MDRT value (for MS Dump)
            l_err = RUNTIME::writeActualCount(RUNTIME::MS_DUMP_RESULTS_TBL);
            if(l_err != NULL)
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "write_MDRT_Count failed" );
                break;
            }
        }

#if 0 //@TODO-RTC:147565-Core checkstop escalation
        // Revert back to standard runtime mode where core checkstops
        //  do not escalate to system checkstops
        // Workaround for HW286670
        l_err = enableCoreCheckstops();
        if ( l_err )
        {
            break;
        }
#endif

        // Fill in Hostboot runtime data for all nodes
        // (adjunct partition)
        // Write the HB runtime data into mainstore
        l_err = RUNTIME::populate_hbRuntimeData();
        if ( l_err )
        {
            TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                       "Failed hbRuntimeData setup" );
            // break from do loop if error occurred
            break;
        }

        if( !INITSERVICE::spBaseServicesEnabled() )
        {
            // Invalidate the VPD cache for golden side boot
            // Also invalidate in manufacturing mode
            // Must do this after saving away the VPD cache into mainstore,
            //  i.e. after RUNTIME::populate_hbRuntimeData()
            l_err = VPD::goldenCacheInvalidate();
            if ( l_err )
            {
                break;
            }
        }

        if (TCE::utilUseTcesForDmas())
        {
            // Disable all TCEs
            l_err = TCE::utilDisableTces();
            if ( l_err )
            {
                TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                           "Failed TCE::utilDisableTces" );
                // break from do loop if error occurred
                break;
            }
        }

    } while(0);

    if( l_err )
    {
        TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
                "istep host_runtime_setup failed see plid 0x%x", l_err->plid());

        // Create IStep error log and cross reference error that occurred
        l_StepError.addErrorDetails( l_err );

        // Commit Error
        errlCommit(l_err, ISTEP_COMP_ID);

    }

    TRACFCOMP( ISTEPS_TRACE::g_trac_isteps_trace,
            "call_host_runtime_setup exit" );

    return l_StepError.getErrorHandle();
}


};

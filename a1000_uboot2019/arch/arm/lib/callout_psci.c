#include <common.h>
#include "sec_def.h"
#include "command_def.h"

CALLOUT_DATA u32 bst_smc_priv_fid[] = {
    0x32000003U, /* optee_handle_rpc, optee smc call return from rpc */
    0x32000004U, /* optee_do_call_with_arg, optee smc call with arg */
    0x80000000U, /* psci_init_smccc, get arm smccc version */
    0x80000001U, /* smccc_soc_init, arm SMCCC arch feature id */
    0xb2000001U, /* optee_msg_get_os_revision, optee smc get os revision */
    0xb2000007U, /* optee_config_shm_memremap, optee smc get shm config */
    0xb2000009U, /* optee_msg_exchange_capabilities */
    0xb200000aU, /* __optee_disable_shm_cache, optee smc disable shm cache */
    0xb200000bU, /* optee_enable_shm_cache, optee smc enable shm cache */
    0xbf00ff01U, /* optee_msg_api_uid_is_optee_api, optee smc call uid */
    0xbf00ff03U, /* optee_msg_api_revision_is_compatible, optee smc call revision */
    0xc20000feU, /* smc_send_message, scmi's smc function id */
    0xc4000001U, /* unknown */
    0xc4000012U  /* unknown */
};

CALLOUT int8_t find_psci_fid(u32 psci_fid)
{
    for (u32 i = 0; i < (sizeof(bst_smc_priv_fid) / sizeof(u32)); i++) {
        if (psci_fid == bst_smc_priv_fid[i]) {
            return 1;
        }
    }
    return 0;
}
CALLOUT_CLASS(find_psci_fid, CMD_PSCI_FIND_FID);
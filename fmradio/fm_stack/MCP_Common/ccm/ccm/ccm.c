/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include "mcp_hal_fs.h"
#include "mcp_hal_pm.h"
#include "mcp_hal_os.h"
#include "mcp_hal_hci.h"
#include "mcp_defs.h"
#include "ccm_hal_pwr_up_dwn.h"
#include "ccm.h"
#include "ccm_imi.h"
#include "ccm_vaci.h"
#include "ccm_vaci_chip_abstration.h"
#include "mcp_config_parser.h"
#include "mcp_unicode.h"
#include "mcp_hal_log.h"
#include "mcpf_defs.h"

MCP_HAL_LOG_SET_MODULE(MCP_HAL_LOG_MODULE_TYPE_CCM);

struct tagCcmObj {
	void *			hMcpf;
    McpUint         refCount;
    McpHalChipId    chipId;
    
    handle_t        ccmaObj;
    CcmImObj        *imObj;
    TCCM_VAC_Object *vacObj;
    Cal_Config_ID   *calObj;

McpConfigParser 	tConfigParser;			/* configuration file storage and parser */
};

typedef struct tagCcmStaticData {
    CcmObj  _ccm_Objs[MCP_HAL_MAX_NUM_OF_CHIPS];
} CcmStaticData;


/*
    A single instance that holds the static ("class") CCM data 
*/
MCP_STATIC  CcmStaticData _CCM_StaticData;

MCP_STATIC  CcmStatus _CCM_StaticInit(void);

#ifndef MCP_STK_ENABLE
void _CCM_NotifyChipOn(void *handle,
                       McpU16 projectType,
                       McpU16 versionMajor,
                       McpU16 versionMinor);
#endif

CcmStatus CCM_StaticInit(void)
{
    CcmStatus       status;
#ifndef MCP_STK_ENABLE
    CcmImStatus     imStatus;
#endif
    ECCM_VAC_Status vacStatus;
    
    /* Used to init once only */
    static McpBool firstInit = MCP_TRUE;

    MCP_FUNC_START("CCM_StaticInit");

    if (firstInit == MCP_FALSE)
    {
        MCP_LOG_INFO(("_CCM_StaticInit Already Initialized, Exiting Successfully"));
        MCP_RET(CCM_STATUS_SUCCESS);
    }

    firstInit = MCP_FALSE;

    status = _CCM_StaticInit();
    MCP_VERIFY_FATAL((status == CCM_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR, ("_CCM_StaticInit"));

    /* Initialize contained entities (IM, PM, VAC) */
    #ifndef MCP_STK_ENABLE
    imStatus = CCM_IM_StaticInit();
    MCP_VERIFY_FATAL((imStatus == CCM_IM_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR, ("CCM_IM_StaticInit"));
    #endif

    vacStatus = CCM_VAC_StaticInit();
    MCP_VERIFY_FATAL ((CCM_VAC_STATUS_SUCCESS == vacStatus), CCM_STATUS_INTERNAL_ERROR,
                      ("CCM_StaticInit: VAC initialization failed with status %d", vacStatus));

    CCM_CAL_StaticInit();

    status = CCM_STATUS_SUCCESS;
    
    MCP_FUNC_END();
    
    return status;
}

/*
    The function needs to do the following:
    - Perform CCM "class" static initialization (if necessary - first time)
    - "Create" the instance (again, if it's the first creation of this instance)
*/
CcmStatus CCM_Create(void *hMcpf, McpHalChipId chipId, handle_t *thisObj)
{
    CcmStatus status;
#ifndef MCP_STK_ENABLE
    CcmImStatus imStatus;
#endif
    ECCM_VAC_Status vacStatus;
    McpConfigParserStatus eConfigParserStatus;
	McpUtf8 scriptPath[MCP_HAL_CONFIG_FS_MAX_PATH_LEN_CHARS *
                       MCP_HAL_CONFIG_MAX_BYTES_IN_UTF8_CHAR] = "";
#ifdef MCP_STK_ENABLE
    handle_t ccmaObj;
#endif
	
    MCP_FUNC_START("CCM_Create");

    MCP_VERIFY_FATAL((chipId < MCP_HAL_MAX_NUM_OF_CHIPS),
                     CCM_STATUS_INTERNAL_ERROR,
                     (("Invalid Chip Id"), chipId));
    
    if (_CCM_StaticData._ccm_Objs[chipId].refCount == 0)
    {
        _CCM_StaticData._ccm_Objs[chipId].chipId = chipId;
		_CCM_StaticData._ccm_Objs[chipId].hMcpf  = hMcpf;

		/* read ini file */
		
        MCP_StrCpyUtf8(scriptPath, (const McpUtf8 *)CCM_VAC_CONFIG_PATH_NAME);
        MCP_StrCatUtf8(scriptPath, (const McpUtf8 *)CCM_VAC_CONFIG_FILE_NAME);

		eConfigParserStatus = MCP_CONFIG_PARSER_Open(scriptPath,
                                                     (McpU8*)CCM_VAC_MEM_CONFIG,
                                                     &(_CCM_StaticData._ccm_Objs[chipId].tConfigParser));
		MCP_VERIFY_ERR((MCP_CONFIG_PARSER_STATUS_SUCCESS == eConfigParserStatus),
					   CCM_VAC_STATUS_FAILURE_UNSPECIFIED,
					   ("_CCM_VAC_ConfigurationEngine_Create: reading config file failed with status %d",
					    eConfigParserStatus));
		
#ifdef MCP_STK_ENABLE
        /* Create CCM Adapter */
        ccmaObj = CCMA_Create(hMcpf, chipId, NULL);
        MCP_VERIFY_FATAL((ccmaObj != NULL),
                          CCM_STATUS_INTERNAL_ERROR,
                          ("Fail result of CCMA_Create"));

        _CCM_StaticData._ccm_Objs[chipId].ccmaObj = ccmaObj;

#else
        imStatus = CCM_IM_Create(hMcpf,
                                 chipId,
                                 &_CCM_StaticData._ccm_Objs[chipId].imObj,
                                 _CCM_NotifyChipOn,
                                 &_CCM_StaticData._ccm_Objs[chipId]);
        MCP_VERIFY_FATAL((imStatus == CCM_IM_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR, ("CCM_IM_Create Failed"));

        ccmaObj = *CCM_IMI_GetCcmaObj (_CCM_StaticData._ccm_Objs[chipId].imObj);
#endif

        CAL_Create (chipId,
                    ccmaObj,
                    &(_CCM_StaticData._ccm_Objs[chipId].calObj),
                    &(_CCM_StaticData._ccm_Objs[chipId].tConfigParser));
        /* CAL creation cannot fail */

        vacStatus = CCM_VAC_Create (chipId,
                                    _CCM_StaticData._ccm_Objs[chipId].calObj, 
                                    &(_CCM_StaticData._ccm_Objs[chipId].vacObj),
                                    &(_CCM_StaticData._ccm_Objs[chipId].tConfigParser));
        MCP_VERIFY_FATAL ((CCM_VAC_STATUS_SUCCESS == vacStatus), CCM_STATUS_INTERNAL_ERROR,
                          ("CCM_Create: VAC creation failed with status %d", vacStatus));
    }

    ++_CCM_StaticData._ccm_Objs[chipId].refCount;

    /* Set the instance pointer (out parameter) to be used in external references to this instance */
    *thisObj = (handle_t)&_CCM_StaticData._ccm_Objs[chipId];

    status = CCM_STATUS_SUCCESS;
    
    MCP_FUNC_END();

    return status;
}

/*
    
*/
CcmStatus CCM_Destroy(handle_t *thisObj)
{
    CcmStatus status;
#ifndef MCP_STK_ENABLE
    CcmImStatus imStatus;
#endif

    McpHalChipId chipId = ((CcmObj *)*thisObj)->chipId;
        
    MCP_FUNC_START("CCM_Destroy");

    MCP_VERIFY_FATAL((_CCM_StaticData._ccm_Objs[chipId].refCount > 0),
                     CCM_STATUS_INTERNAL_ERROR, 
                        ("CCM_IM_Destroy: CCM(#%d) Doesn't Exist", chipId));

    /* Another client of this instance wishes to destroy it */      
    --_CCM_StaticData._ccm_Objs[chipId].refCount;

    if (_CCM_StaticData._ccm_Objs[chipId].refCount == 0)
    {
        CCM_VAC_Destroy (&((CcmObj *)*thisObj)->vacObj);
        /* VAC destruction cannot fail */

        CAL_Destroy (&(((CcmObj *)*thisObj)->calObj));
        /* CAL destruction cannot fail */

#ifdef MCP_STK_ENABLE
        /* Destroy CCM Adapter */
        CCMA_Destroy(_CCM_StaticData._ccm_Objs[chipId].ccmaObj);
#else
        /* Last instance client, now we can actually perform destruction*/
        imStatus = CCM_IM_Destroy(&((CcmObj *)*thisObj)->imObj);
        MCP_VERIFY_FATAL((imStatus == CCM_IM_STATUS_SUCCESS),
                          CCM_STATUS_INTERNAL_ERROR,
                          ("CCM_IM_Destroy Failed"));
#endif
    }

    *thisObj = NULL;

    status = CCM_STATUS_SUCCESS;
    
    MCP_FUNC_END();

    return status;
}

#ifdef MCP_STK_ENABLE
CcmStatus CCM_Configure(CcmObj *ccmObj)
{
    CcmStatus status = CCM_STATUS_SUCCESS;
    CcmaStatus ccmaStatus;
    ECCM_VAC_Status vacStatus;
    THalHci_ChipVersion chipVersion;

    MCP_FUNC_START("CCM_Configure");

    /* Get chip's version from the CCMA */
    ccmaStatus = CCMA_GetChipVersion(ccmObj->ccmaObj, &chipVersion);
    MCP_VERIFY_FATAL((CCMA_STATUS_SUCCESS == ccmaStatus),
                     CCM_STATUS_INTERNAL_ERROR,
                     ("CCMA_GetChipVersion failed"));
    
    /* Configure the CAL */
    CCM_CAL_Configure(ccmObj->calObj,
                      chipVersion.projectType,
                      chipVersion.major,
                      chipVersion.minor);
    
    /* Configure the VAC */
    vacStatus = CCM_VAC_Configure(ccmObj->vacObj);
    MCP_VERIFY_FATAL((CCM_VAC_STATUS_SUCCESS == vacStatus),
                     CCM_STATUS_INTERNAL_ERROR,
                     ("CCM_VAC_Configure failed"));

    MCP_FUNC_END();
    
    return status;
}

#endif

CcmImObj *CCM_GetIm(CcmObj *thisObj)
{
    return thisObj->imObj;
}

TCCM_VAC_Object *CCM_GetVac(CcmObj *thisObj)
{
    return thisObj->vacObj;
}

Cal_Config_ID *CCM_GetCAL(CcmObj *thisObj)
{
    return thisObj->calObj;
}

CcmStatus _CCM_StaticInit(void)
{
    CcmStatus               status;
    McpHalOsStatus          mcpHalOsStatus;
#ifndef MCP_STK_ENABLE
    CcmHalPwrUpDwnStatus    ccmHalPwrUpDwnStatus;
#endif    
    McpHalFsStatus          halFsStatus;
    McpUint                 chipIdx;
    McpHalPmStatus          halPmStatus;

    MCP_FUNC_START("_CCM_StaticInit");

    mcpHalOsStatus = MCP_HAL_OS_Init();
    MCP_VERIFY_FATAL((mcpHalOsStatus == MCP_HAL_OS_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR,
                        ("MCP_HAL_OS_Init Failed (%d)", mcpHalOsStatus));
            
    halFsStatus = MCP_HAL_FS_Init();
    MCP_VERIFY_FATAL((halFsStatus == MCP_HAL_FS_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR,
                        ("MCP_HAL_FS_Init Failed (%d)", halFsStatus));
    halPmStatus = MCP_HAL_PM_Init();
    MCP_VERIFY_FATAL((halFsStatus == MCP_HAL_FS_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR,
                        ("MCP_HAL_FS_Init Failed (%d)", halFsStatus));

#ifndef MCP_STK_ENABLE
    ccmHalPwrUpDwnStatus = CCM_HAL_PWR_UP_DWN_Init();
    MCP_VERIFY_FATAL((ccmHalPwrUpDwnStatus == CCM_HAL_PWR_UP_DWN_STATUS_SUCCESS), CCM_STATUS_INTERNAL_ERROR,
                        ("CCM_HAL_PWR_UP_DWN_Init Failed (%d)", ccmHalPwrUpDwnStatus));
#endif    

    for (chipIdx = 0; chipIdx < MCP_HAL_MAX_NUM_OF_CHIPS; ++chipIdx)
    {
        _CCM_StaticData._ccm_Objs[chipIdx].refCount = 0;
    }

    status = CCM_STATUS_SUCCESS;
    
    MCP_FUNC_END();

    return status;
}

#ifndef MCP_STK_ENABLE
void _CCM_NotifyChipOn(void *handle,
                       McpU16 projectType,
                       McpU16 versionMajor,
                       McpU16 versionMinor)
{
    CcmObj *thisObj = (CcmObj *)handle;

    /* configure the CAL */
    CCM_CAL_Configure(thisObj->calObj, projectType, versionMajor, versionMinor);

    /* configure the VAC */
    CCM_VAC_Configure(thisObj->vacObj);

}
#endif

handle_t *CCM_GetCcmaObj(CcmObj *thisObj)
{
#ifdef MCP_STK_ENABLE
    return &thisObj->ccmaObj;
#else
    return CCM_IMI_GetCcmaObj (thisObj->imObj);
#endif
}


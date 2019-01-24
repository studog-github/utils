/***************************************************************************
 *
 * File:
 *     $Id: goep.c 9200 2012-05-28 16:29:37Z khendrix $
 *     $Product: iAnywhere MT OBEX SDK v3.x $
 *     $Revision: 9200 $
 *
 * Description:   This file contains the functions that comprise the Generic 
 *                Object Exchange profile implementation.
 * 
 * Created:       September 4, 2000
 *
 * Copyright 2000-2006 Extended Systems, Inc.
 * Portions copyright 2006-2012 iAnywhere Solutions, Inc.
 * All rights reserved. All unpublished rights reserved.
 *
 * Unpublished Confidential Information of iAnywhere Solutions, Inc.  
 * Do Not Disclose.
 *
 * No part of this work may be used or reproduced in any form or by any 
 * means, or stored in a database or retrieval system, without prior written 
 * permission of iAnywhere Solutions, Inc.
 * 
 * Use of this work is governed by a license granted by iAnywhere Solutions, 
 * Inc.  This work contains confidential and proprietary information of 
 * iAnywhere Solutions, Inc. which is protected by copyright, trade secret, 
 * trademark and other intellectual property rights.
 *
 ****************************************************************************/
#include "sys/goepext.h"

#ifndef QNX_BLUETOOTH
#include "../../add-ins/obex/obdebug.c"
#include "goep_types.c"
#endif /* QNX_BLUETOOTH */

#ifdef QNX_BLUETOOTH_GOEP_UNICODE_HANDLING
#include <stdio.h>
#include <arpa/inet.h>
#endif /* QNX_BLUETOOTH_GOEP_UNICODE_HANDLING */

/*
 * The GOEP layer relies on events deliverd as part of the OBEX packet
 * flow control option. Therefore it must be enabled when using the GOEP.
 * It also uses server header build and client header parse functions.
 */
#if OBEX_PACKET_FLOW_CONTROL == XA_DISABLED
#error "OBEX_PACKET_FLOW_CONTROL Must be enabled."
#endif /* OBEX_PACKET_FLOW_CONTROL == XA_DISABLED */

#if OBEX_HEADER_BUILD == XA_DISABLED
#error "OBEX_HEADER_BUILD Must be enabled."
#endif /* OBEX_HEADER_BUILD == XA_DISABLED */

/****************************************************************************
 *
 * RAM Data
 *
 ****************************************************************************/
#if OBEX_ROLE_CLIENT == XA_ENABLED
#if XA_CONTEXT_PTR == XA_ENABLED
static GoepClientData   tempClient;
GoepClientData         *GoeClient = &tempClient;
#else /* XA_CONTEXT_PTR == XA_ENABLED */
GoepClientData          GoeClient;
#endif /* XA_CONTEXT_PTR == XA_ENABLED */
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_ROLE_SERVER == XA_ENABLED
#if XA_CONTEXT_PTR == XA_ENABLED
static GoepServerData   tempServer;
GoepServerData         *GoeServer = &tempServer;
#else /* XA_CONTEXT_PTR == XA_ENABLED */
GoepServerData          GoeServer;
#endif /* XA_CONTEXT_PTR == XA_ENABLED */
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

/****************************************************************************
 *
 * Internal Function Prototypes
 *
 ****************************************************************************/
#if OBEX_ROLE_CLIENT == XA_ENABLED
static void GoepClntCallback(ObexClientCallbackParms *parms);
static void GoepClientProcessHeaders(ObexClientCallbackParms *parms);
static void NotifyCurrClient(ObexClientApp *clientApp, GoepEventType Event);
static void NotifyAllClients(ObexClientApp *clientApp, GoepEventType Event);
#if OBEX_SESSION_SUPPORT == XA_ENABLED
static void ResumeClientOperation(ObexClientApp *clientApp, ObexOpcode Op);
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
#if GOEP_ADDITIONAL_HEADERS > 0
static BOOL ClientBuildHeaders(GoepClientObexCons *client, U16 *more);
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
#if BT_SECURITY == XA_ENABLED
static BtStatus GoepClientSecAccessRequest(GoepClientApp *Client);
#endif /* BT_SECURITY == XA_ENABLED */
#if OBEX_SRM_MODE == XA_ENABLED
static ObStatus ClientBuildSrmHeader(GoepClientObexCons *client);
#endif /* OBEX_SRM_MODE == XA_ENABLED */
static ObStatus BuildConnectReq(GoepClientObexCons *client, GoepConnectReq *connect);
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_ROLE_SERVER == XA_ENABLED
static void GoepSrvrCallback(ObexServerCallbackParms *parms);
static void GoepServerProcessHeaders(ObexServerCallbackParms *parms);
static void NotifyCurrServer(ObexServerApp *serverApp, GoepEventType Event);
static void NotifyAllServers(ObexServerApp *serverApp, GoepEventType Event);
static void StartServerOperation(ObexServerApp *serverApp, GoepOperation Op);
static void AssociateServer(ObexServerApp *serverApp);
#if OBEX_SESSION_SUPPORT == XA_ENABLED
static void ResumeServerOperation(ObexServerApp *serverApp, ObexOpcode Op);
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
#if GOEP_ADDITIONAL_HEADERS > 0
static BOOL ServerBuildHeaders(GoepServerObexCons *server);
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
#if BT_SECURITY == XA_ENABLED
static BtStatus GoepServerSecAccessRequest(GoepServerObexCons *Server);
#endif /* BT_SECURITY == XA_ENABLED */
#if OBEX_SRM_MODE == XA_ENABLED
static ObStatus ServerBuildSrmHeader(GoepServerObexCons *server);
#endif /* OBEX_SRM_MODE == XA_ENABLED */
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

static void DoHeaderCopy(U8 *Dst, U16 *Len, U16 MaxLen, U8 *Src, U16 SrcLen);
static void DoUniHeaderCopy(GoepUniType *Dst, U16 *Len, U16 MaxLen, U8 *Src, U16 SrcLen);
static BOOL GetFreeConnId(U8 *id, U8 type);

/****************************************************************************
 *
 * Public API Functions
 *
 ****************************************************************************/
/*---------------------------------------------------------------------------
 *            GOEP_Init()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initialize the GOEP component.  This must be the first GOEP 
 *            function called by the application layer, or if multiple 
 *            GOEP applications exist, this function should be called
 *            at system startup (see XA_LOAD_LIST in config.h).  OBEX must 
 *            also be initialized separately.
 *
 * Return:    TRUE or FALSE
 *
 */
BOOL GOEP_Init(void)
{
    U8 i;

    OS_LockObex();

    GOEP_InitTypes();

#if OBEX_ROLE_CLIENT == XA_ENABLED
#if XA_CONTEXT_PTR == XA_ENABLED
    OS_MemSet((U8 *)GoeClient, 0, sizeof(GoepClientData));
#else /* XA_CONTEXT_PTR == XA_ENABLED */
    OS_MemSet((U8 *)&GoeClient, 0, sizeof(GoepClientData));
#endif /* XA_CONTEXT_PTR == XA_ENABLED */
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_ROLE_SERVER == XA_ENABLED
#if XA_CONTEXT_PTR == XA_ENABLED
    OS_MemSet((U8 *)GoeServer, 0, sizeof(GoepServerData));
#else /* XA_CONTEXT_PTR == XA_ENABLED */
    OS_MemSet((U8 *)&GoeServer, 0, sizeof(GoepServerData));
#endif /* XA_CONTEXT_PTR == XA_ENABLED */
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

    /* Setup a connection Id and connection Count for each client/server */
    for (i = 0; i < GOEP_NUM_OBEX_CONS; i++) {
#if OBEX_ROLE_CLIENT == XA_ENABLED
        /* Setup the connId */
        GOEC(clients)[i].connId = i;
        /* Setup the connCount */
        GOEC(clients)[i].connCount = 0;
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */
#if OBEX_ROLE_SERVER == XA_ENABLED
        /* Setup the connId */
        GOES(servers)[i].connId = i;
        /* Setup the connCount */
        GOES(servers)[i].connCount = 0;
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */
    }

    /* GOEP Initialized */
#if OBEX_ROLE_CLIENT == XA_ENABLED
    GOEC(initialized) = TRUE;
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */
#if OBEX_ROLE_SERVER == XA_ENABLED
    GOES(initialized) = TRUE;
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

    OS_UnlockObex();
    return TRUE;

}
 
#if OBEX_ROLE_SERVER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_RegisterServerEx()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initialize the OBEX Server.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_RegisterServerEx(GoepServerApp *Server,
                               const GoepRegisterServerParms *Parms)
{
    /* Note: for backwards-compatibility, this registration routine routes 
     * into the old GOEP_RegisterSrvr() interface, which remains basically
     * unmodified.
     */
    Server->callback   = Parms->callback;
    Server->type       = Parms->type;
    Server->appParent  = Parms->appParent;

#if OBEX_SERVER_CONS_SIZE > 0
    /* Copy targets from parameters if necessary. Make sure there is room */
    CheckUnlockedParm(OB_STATUS_INVALID_PARM, Parms->numTargets <= OBEX_SERVER_CONS_SIZE);
    Server->numTargets = Parms->numTargets;
    if (Server->numTargets) {
        OS_MemCopy((U8 *)&Server->target, (U8 *)&Parms->target, Server->numTargets * sizeof(ObexConnection *));
    }
#endif

#if BT_SERVICE_ID == XA_ENABLED
    if (Parms->serviceId) {
        Server->serviceId  = Parms->serviceId;
    } else {
        Server->serviceId = Server;
    }
#endif /* BT_SERVICE_ID == XA_ENABLED */

#if BT_SECURITY == XA_ENABLED
    return GOEP_RegisterSrvr(Server, Parms->obStoreFuncs, &Parms->secParms);
#else
    return GOEP_RegisterSrvr(Server, Parms->obStoreFuncs, 0);
#endif
}

/*---------------------------------------------------------------------------
 *            GOEP_RegisterSrvr()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initialize the OBEX Server
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_RegisterSrvr(GoepServerApp *Service, 
                           const ObStoreFuncTable *ObStoreFuncs,
                           const BtSecurityParms *SecParms)
{
    ObStatus  status = OB_STATUS_SUCCESS;
    U8        id;
#if OBEX_SERVER_CONS_SIZE > 0
    U8        i;
#endif /* OBEX_SERVER_CONS_SIZE > 0 */

    OS_LockObex();

#if BT_SECURITY == XA_DISABLED
    UNUSED_PARAMETER(SecParms);
#endif /* BT_SECURITY == XA_DISABLED */

#if XA_ERROR_CHECK == XA_ENABLED
    if (GOES(initialized) != TRUE) {
        /* GOEP is not initialized */
        status = OB_STATUS_FAILED;
        goto Error;
    }

    if ((!Service) || (!ObStoreFuncs) || (Service->type >= GOEP_MAX_PROFILES) || 
#if OBEX_SERVER_CONS_SIZE > 0
        (Service->numTargets > OBEX_SERVER_CONS_SIZE) || 
        ((Service->numTargets > 0) && (Service->type == GOEP_PROFILE_OPUSH)) ||
#endif /* OBEX_SERVER_CONS_SIZE > 0 */
        (Service->callback == 0)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */
    
  
    Assert(Service && ObStoreFuncs && (Service->type < GOEP_MAX_PROFILES) &&
          (Service->callback != 0));

    /* Create an OBEX connection if available */
    if (GOES(connCount) < GOEP_NUM_OBEX_CONS) {
        /* Get a free server conn Id */
        if (!GetFreeConnId(&id, GOEP_SERVER)) {
            status = OB_STATUS_NO_RESOURCES;
            goto Error;
        }
        OBEX_InitAppHandle(&GOES(servers)[id].obs.handle, GOES(servers)[id].
                           headerBlock, (U16)GOEP_SERVER_HB_SIZE, ObStoreFuncs);

#if (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (BT_STACK_VERSION >= 402)
        GOES(servers)[id].obs.trans.ObexServerL2BtTrans.server.serviceId = Service->serviceId;
#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
        GOES(servers)[id].obs.trans.ObexClientL2BtTrans.client.serviceId = Service->serviceId;
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */
#endif /* (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (BT_STACK_VERSION >= 402) */

        status = OBEX_ServerInit(&GOES(servers)[id].obs, GoepSrvrCallback, 
                                 OBEX_EnumTransports());

        if (status == OB_STATUS_SUCCESS) {
            Service->connId = id;
            /* Increment the current number of Obex connections */
            GOES(connCount)++;
        }
    } else {
        /* Not enough Obex connections allocated in GOEP to allocate a new 
         * Connection Id 
         */
        Report(("GOEP: Current Obex connections = %i, greater than or equal to \
                 GOEP_NUM_OBEX_CONS = %i\n", GOES(connCount), 
                GOEP_NUM_OBEX_CONS));
        status = OB_STATUS_NO_RESOURCES;
        goto Error;
    }

    if (status == OB_STATUS_SUCCESS) {
#if OBEX_SERVER_CONS_SIZE > 0
        for (i = 0; i < Service->numTargets; i++) {
            Assert(Service->type != GOEP_PROFILE_OPUSH);
            status = OBEX_ServerRegisterTarget(&GOES(servers)[Service->connId].obs, 
                                               Service->target[i]);
            if (status != OB_STATUS_SUCCESS) {
#if OBEX_DEINIT_FUNCS == XA_ENABLED
                if (GOES(servers)[Service->connId].connCount == 0)
                    OBEX_ServerDeinit(&GOES(servers)[Service->connId].obs);
#endif /* OBEX_DEINIT_FUNCS == XA_ENABLED */
                goto Error;
            }
        }
#endif /* OBEX_SERVER_CONS_SIZE > 0 */
        /* Set this server to "in use" */
        GOES(servers)[Service->connId].connCount++;
        GOES(servers)[Service->connId].profiles[Service->type] = Service;

        if (Service->appParent == 0) {
            Service->appParent = Service->callback;
        }

#if BT_SECURITY == XA_ENABLED
        /* Only register Security if we are not using the older "deprecated" API's
         * to setup security (GOEP_RegisterServerSecurityRecord)
         */
        if (SecParms) {
            /* Register security for the GOEP service */
            Service->secRecord.id =  SEC_GOEP_ID;
            Service->secRecord.channel = (U32)Service;
            Service->secRecord.level = SecParms->level;
#if BT_STACK_VERSION > 311
            Service->secRecord.pinLen = SecParms->pinLen;
#endif /* BT_STACK_VERSION > 311 */
#if BT_SERVICE_ID == XA_ENABLED
            Service->secRecord.serviceId = Service->serviceId;
#endif /* BT_SERVICE_ID == XA_ENABLED */
            status = SEC_Register(&Service->secRecord);
            if (status == BT_STATUS_SUCCESS) {
                Service->registered = TRUE;
            }
        }
#endif /* BT_SECURITY == XA_ENABLED */
    }

Error:
    OS_UnlockObex();
    return status;
}

#if BT_SECURITY == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_RegisterServerSecurityRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Registers a security record for the GOEP service.
 *
 * Return:    (See goep.h)
 */
BtStatus GOEP_RegisterServerSecurityRecord(GoepServerApp *Service, BtSecurityLevel Level)
{
    BtStatus status = BT_STATUS_FAILED;

    if (Service->registered) {
        /* Already registered, so just return success */
        return BT_STATUS_SUCCESS;
    }

    OS_LockObex();

    /* Register security for the GOEP service */
    Service->secRecord.id =  SEC_GOEP_ID;
    Service->secRecord.channel = (U32)Service;
    Service->secRecord.level = Level;
#if BT_STACK_VERSION > 311
    Service->secRecord.pinLen = 0;
#endif /* BT_STACK_VERSION > 311 */

    status = SEC_Register(&Service->secRecord);
    if (status == BT_STATUS_SUCCESS) {
        Service->registered = TRUE;
    }

    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_UnregisterServerSecurityRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Unregisters a security handler for the GOEP service.
 *
 * Return:    (See goep.h)
 */
BtStatus GOEP_UnregisterServerSecurityRecord(GoepServerApp *Service)
{
    BtStatus status = BT_STATUS_SUCCESS;

    OS_LockObex();

    if (Service->registered) {
        /* Unregister security for the GOEP Service */
        status = SEC_Unregister(&Service->secRecord);
        if (status == BT_STATUS_SUCCESS) {
            Service->registered = FALSE;
        }
    }

    OS_UnlockObex();

    return status;
}
#endif /* BT_SECURITY == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_GetObexServer()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieves the OBEX Server pertaining to a specific GOEP Server.  
 *            This function is valid after GOEP_Init and GOEP_RegisterServer 
 *            have been called.
 *
 * Return:    ObexServerApp
 *
 */
ObexServerApp* GOEP_GetObexServer(GoepServerApp *Service)
{
    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Service) || (Service->connId >= GOEP_NUM_OBEX_CONS)) {
        OS_UnlockObex();
        return 0;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service && (Service->connId < GOEP_NUM_OBEX_CONS));

    OS_UnlockObex();
    return &(GOES(servers)[Service->connId].obs);
}


#if OBEX_DEINIT_FUNCS == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_DeregisterServer()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deinitialize the OBEX Server.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_DeregisterServer(GoepServerApp *Service)
{
    ObStatus    status;
    GoepServerObexCons *server;
#if OBEX_SERVER_CONS_SIZE > 0
    U8          i;
#endif /* OBEX_SERVER_CONS_SIZE > 0 */

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if (GOES(initialized) != TRUE) {
        /* GOEP is not initialized, so there is nothing to deinit */
        status = OB_STATUS_SUCCESS;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) || 
#if OBEX_SERVER_CONS_SIZE > 0
        (Service->numTargets > OBEX_SERVER_CONS_SIZE) ||
#endif /* OBEX_SERVER_CONS_SIZE > 0 */
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) &&
           (server->profiles[Service->type] == Service));

    /* We had better have a connection open */
    if (GOES(connCount) == 0) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

    /* See if they are processing an operation */
    if ((server->currOp.oper != GOEP_OPER_NONE) &&
        (server->currOp.handler == Service)) {
        status = OB_STATUS_BUSY;
        goto Error;
    }
    server->profiles[Service->type] = 0;

#if OBEX_SERVER_CONS_SIZE > 0
    for (i = 0; i < Service->numTargets; i++) {
        OBEX_ServerDeregisterTarget(&server->obs, Service->target[i]);
    }
#endif /* OBEX_SERVER_CONS_SIZE > 0 */

    if (--(server->connCount) > 0) {
        status = OB_STATUS_SUCCESS;
        goto Error;
    }

    --GOES(connCount);
    status = OBEX_ServerDeinit(&server->obs);

#if BT_SECURITY == XA_ENABLED
    if (status == OB_STATUS_SUCCESS) {
        if (Service->registered) {
            /* Unregister security for the GOEP Service */
            status = SEC_Unregister(&Service->secRecord);
            if (status == BT_STATUS_SUCCESS) {
                Service->registered = FALSE;
            }
        }
    }
#endif /* BT_SECURITY == XA_ENABLED */

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_DEINIT_FUNCS == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_ServerAbort
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Abort the current server operation.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerAbort(GoepServerApp *Service, ObexRespCode Resp)
{
    ObStatus    status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) || 
        (Service->connId >= GOEP_NUM_OBEX_CONS) ||
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) &&
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

    if (((Service != 0) && (Service != server->currOp.handler)) ||
        (server->currOp.oper == GOEP_OPER_NONE)) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

    status = OBEX_ServerAbort(&server->obs, Resp, FALSE);

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ServerAccept
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function is called to accept a Push or Pull request.
 *            It MUST be called during the GOEP_EVENT_PROVIDE_OBJECT indication
 *            for a Push operation, but can be delayed until the 
 *            GOEP_EVENT_CONTINUE event for Pull operations. However, failure 
 *            to follow these rules will result in an abort of the operation.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerAccept(GoepServerApp *Service, void *Obsh)
{
    ObStatus            status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Obsh) || (Service->type >= GOEP_MAX_PROFILES) || 
        (Service->connId >= GOEP_NUM_OBEX_CONS) ||
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Obsh && (Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

    if ((server->currOp.handler != Service) ||
        (((server->currOp.oper == GOEP_OPER_PUSH) && (server->currOp.event != GOEP_EVENT_PROVIDE_OBJECT)) ||
         ((server->currOp.oper == GOEP_OPER_PULL) && (server->currOp.event != GOEP_EVENT_CONTINUE) &&
          (server->currOp.event != GOEP_EVENT_PROVIDE_OBJECT)))) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

    if (server->currOp.oper == GOEP_OPER_PULL && 
        server->currOp.info.pushpull.objectLen
#if OBEX_DYNAMIC_OBJECT_SUPPORT == XA_ENABLED
        && server->currOp.info.pushpull.objectLen != UNKNOWN_OBJECT_LENGTH
#endif /* OBEX_DYNAMIC_OBJECT_SUPPORT == XA_ENABLED */
        ) {
        OBEXH_Build4Byte(&server->obs.handle, OBEXH_LENGTH, 
                         server->currOp.info.pushpull.objectLen);
    }

    server->object = Obsh;

    OBEX_ServerAccept(&server->obs, server->object);

    status = OB_STATUS_SUCCESS;

Error:
    OS_UnlockObex();
    return status;
}


#if OBEX_BODYLESS_GET == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ServerAcceptNoObject
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Accepts a GET without specifying a target object.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerAcceptNoObject(GoepServerApp *Service)
{
    ObStatus            status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) || 
        (Service->connId >= GOEP_NUM_OBEX_CONS) ||
        (server->profiles[Service->type] != Service) ||
        (server->currOp.oper != GOEP_OPER_PULL)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service) &&
           (server->currOp.oper == GOEP_OPER_PULL));

    if ((server->currOp.handler != Service) ||
        (server->currOp.event != GOEP_EVENT_PROVIDE_OBJECT)) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

    server->object = 0;

    OBEX_ServerAcceptNoObject(&server->obs);

    status = OB_STATUS_SUCCESS;

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_BODYLESS_GET == XA_ENABLED */


/*---------------------------------------------------------------------------
 *            GOEP_ServerContinue
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Called to keep data flowing between the client and the server.
 *            This function must be called once for every GOEP_EVENT_CONTINUE
 *            event received. It does not have to be called in the context
 *            of the callback. It can be deferred for flow control reasons.
 *
 * Return:    ObStatus
 */
ObStatus GOEP_ServerContinue(GoepServerApp *Service)
{
    ObStatus            status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) || 
        (Service->connId >= GOEP_NUM_OBEX_CONS) ||
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

    if ((server->currOp.handler != Service) || (!server->oustandingResp) ||
        ((server->currOp.event != GOEP_EVENT_CONTINUE) &&
         ((server->currOp.event != GOEP_EVENT_START) || (server->currOp.oper != GOEP_OPER_ABORT)))) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

#if OBEX_SRM_MODE == XA_ENABLED
    /* Build the SRM parameter header, if one exists to build */
    if ((server->currOp.oper == GOEP_OPER_PUSH) || (server->currOp.oper == GOEP_OPER_PULL)) {
        status = ServerBuildSrmHeader(server);
        if (status == OB_STATUS_NOT_FOUND) {
            server->srmParamAllowed = FALSE;
        } else if (status != OB_STATUS_SUCCESS) {
            status = OB_STATUS_FAILED;
            goto Error;
        }
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ServerBuildHeaders(server) == FALSE) {
        OBEXH_FlushHeaders(&server->obs.handle);
        Report(("GOEP: ServerBuildHeaders failed. Retry!"));
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

    server->oustandingResp = FALSE;
    OBEX_ServerSendResponse(&server->obs);

    status = OB_STATUS_SUCCESS;

Error:
    OS_UnlockObex();
    return status;
}

#if OBEX_AUTHENTICATION == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ServerAuthenticate
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Respond to an authentication challenge received from a client, 
 *            or send a challenge to the client.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerAuthenticate(GoepServerApp     *Service, 
                                 ObexAuthResponse  *Response,
                                 ObexAuthChallenge *Challenge )
{
    ObStatus    status;
    GoepServerObexCons *server;
    BOOL success = FALSE;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];

#if XA_ERROR_CHECK == XA_ENABLED
    if ((Challenge && Response) || (!Challenge && !Response) ||
        (Service->type >= GOEP_MAX_PROFILES) || 
        (Service->connId >= GOEP_NUM_OBEX_CONS) ||
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Challenge || Response) && !(Challenge && Response) && 
           (Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

    if (server->currOp.handler != Service) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

    /* Build Challenge if one was provided. Otherwise build a
     * response, as long as we've received a challenge.
     */
    if (Challenge) {
        success = OBEXH_BuildAuthChallenge(&server->obs.handle, Challenge, 
                                           server->nonce);
        /*
         * Since we are challenging the client, the proper thing
         * to do is to respond with unauthorized.
         */
        status = OBEX_ServerAbort(&server->obs, OBRC_UNAUTHORIZED, FALSE);
        if (status != OB_STATUS_SUCCESS) goto Error;
    } 
    else if (server->flags & GOEF_CHALLENGE) {
        success = OBEXH_BuildAuthResponse(&server->obs.handle, Response, 
                                          server->currOp.challenge.nonce);
        server->flags &= ~GOEF_CHALLENGE;
    }

    if (!success) {
        status = OB_STATUS_FAILED;
    } else {
        status = OB_STATUS_SUCCESS;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ServerVerifyAuthResponse
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Verifies the received authentication response.
 *
 * Return:    TRUE if client's response is authenticated.
 *
 */
BOOL GOEP_ServerVerifyAuthResponse(GoepServerApp *Service, U8 *Password, 
                                   U8 PasswordLen)
{
    BOOL                    status;
    ObexAuthResponseVerify  verify;
    GoepServerObexCons     *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Service) || (!Password)) {
        status = FALSE;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service && Password);

    server = &GOES(servers)[Service->connId];
  
    if (server->flags & GOEF_RESPONSE) {
        /* Verify the authentication response */
        verify.nonce = server->nonce;
        verify.digest = server->currOp.response.digest;
        verify.password = Password;
        verify.passwordLen = PasswordLen;

        status = OBEXH_VerifyAuthResponse(&verify);
    } else status = FALSE;

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_ServerGetTpConnInfo
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieves OBEX transport layer connection information.  This 
 *            function can be called when a transport connection is active to 
 *            retrieve connection specific information.   
 *
 * Return:    
 *     TRUE - The tpInfo structure was successfully completed.
 *     FALSE - The transport is not connected (XA_ERROR_CHECK only).
 *
 */
BOOL GOEP_ServerGetTpConnInfo(GoepServerApp *Service, 
                              ObexTpConnInfo *tpInfo)
                                   
{
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Service) || (!tpInfo)) {
        OS_UnlockObex();
        return FALSE;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service && tpInfo);

    server = &GOES(servers)[Service->connId];
    
    OS_UnlockObex();
    return OBEX_GetTpConnInfo(&server->obs.handle, tpInfo);
}

#if GOEP_ADDITIONAL_HEADERS > 0
/*---------------------------------------------------------------------------
 *            GOEP_ServerQueueHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function queues a Byte Sequence, UNICODE, 1-byte, or 4-byte 
 *            OBEX header for transmission by the GOEP Server.
 *
 * Return:    True if the OBEX Header was queued successfully.
 */
BOOL GOEP_ServerQueueHeader(GoepServerApp *Service, ObexHeaderType Type,
                            const U8 *Value, U16 Len) {
    U8                  i;
    BOOL                status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) { 
        status = FALSE;
        goto Done;
    }

    /* Only allow certain headers to be queued for transmission */
    if ((Type == OBEXH_TYPE) || (Type == OBEXH_TARGET) ||
        (Type == OBEXH_WHO) || (Type == OBEXH_CONNID) ||
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        (Type == OBEXH_SESSION_PARAMS) || (Type == OBEXH_SESSION_SEQ_NUM) ||
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        (Type == OBEXH_AUTH_CHAL) || (Type == OBEXH_AUTH_RESP) || (Type == OBEXH_WAN_UUID)) {
        status = FALSE;
        goto Done;
    }

#if GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED
    /* Unicode headers must have an even length if we aren't
     * automatically performing the unicode conversions. 
     */
    if (OBEXH_IsUnicode(Type) && ((Len % 2) != 0)) {
        status = FALSE;
        goto Done;
    }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED */
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
 
    if ((server->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = FALSE;
        goto Done;
    }

#if XA_ERROR_CHECK == XA_ENABLED
    /* Non-Body headers cannot exceed the available OBEX packet size */
    if ((Type != OBEXH_BODY) && (Len > OBEXH_GetAvailableTxHeaderLen(&server->obs.handle))) {
        status = FALSE;
        goto Done;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) {
        if (server->queuedHeaders[i].type == 0) {
            /* Found an empty header to queue */
            server->queuedHeaders[i].type = Type;
            server->queuedHeaders[i].buffer = Value;
            server->queuedHeaders[i].len = Len;
            status = TRUE;
            goto Done;
        }
    }

    /* No empty headers available */
    status = FALSE;

Done:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ServerFlushQueuedHeaders()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function flushes any headers queued with 
 *            GOEP_ServerQueueHeader.  Since a header is not flushed by 
 *            GOEP until it has been sent, it is possible that an upper
 *            layer profile API may need to flush these queued headers in
 *            the case of an internal failure.
 *
 * Return:    None.
 */
void GOEP_ServerFlushQueuedHeaders(GoepServerApp *Service) {
    U8                  i;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) { 
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */
    
    Assert(Service);

    server = &GOES(servers)[Service->connId];

    /* Clear all of the built headers */
    for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) 
        OS_MemSet((U8 *)&server->queuedHeaders[i], 0, sizeof(server->queuedHeaders[i]));

#if XA_ERROR_CHECK == XA_ENABLED
Error:
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    OS_UnlockObex();
}
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */


#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ServerTpConnect
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initiate an OBEX Transport Connection from the GOEP Server.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerTpConnect(GoepServerApp *Service, ObexTpAddr *Target)
{
    ObStatus status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) ||
        (Service->connId >= GOEP_NUM_OBEX_CONS) || !Target ||
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

#if BT_STACK == XA_ENABLED
    if (!(Target->type & OBEX_TP_BLUETOOTH) && 
        !(Target->type & OBEX_TP_BLUETOOTH_L2CAP)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* BT_STACK == XA_ENABLED */
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) && Target &&
           (server->profiles[Service->type] == Service));

#if BT_STACK == XA_ENABLED
    Assert((Target->type & OBEX_TP_BLUETOOTH) || (Target->type & OBEX_TP_BLUETOOTH_L2CAP));
#endif /* BT_STACK == XA_ENABLED */

    if (server->flags & GOEF_ACTIVE) {
        /* Already connected */
        status = OB_STATUS_SUCCESS;
        goto Error;
    }

#if BT_STACK == XA_ENABLED
#if OBEX_RFCOMM_TRANSPORT == XA_ENABLED
    if (Target->type & OBEX_TP_BLUETOOTH) {
        /* Default is to use the RFCOMM transport */
        server->flags |= GOEF_SERVER_BT_TP_INITIATED;
    }
#endif /* OBEX_RFCOMM_TRANSPORT == XA_ENABLED */

#if OBEX_L2CAP_TRANSPORT == XA_ENABLED
    if (Target->type & OBEX_TP_BLUETOOTH_L2CAP) {
        /* Override and use the L2CAP transport */
        Target->type = OBEX_TP_BLUETOOTH_L2CAP;
        server->flags |= GOEF_SERVER_L2BT_TP_INITIATED;
    } else
#endif /* OBEX_L2CAP_TRANSPORT == XA_ENABLED */
#if OBEX_RFCOMM_TRANSPORT == XA_ENABLED
    {
        Target->type = OBEX_TP_BLUETOOTH;
    }
#endif /* OBEX_RFCOMM_TRANSPORT == XA_ENABLED */
#endif /* BT_STACK == XA_ENABLED */

    status = OBEX_ServerTpConnect(&server->obs, Target);
    
    /* Keep track of the target information */
    Service->targetConn = Target;

    if (status == OB_STATUS_SUCCESS) {
        /* The transport connection is up */
        server->currOp.oper = GOEP_OPER_NONE;
        server->flags |= GOEF_ACTIVE;
    } else if (status == OB_STATUS_PENDING) {
        /* The transport connection is coming up */
        server->currOp.handler = Service;
    } else {
        server->flags &= ~(GOEF_SERVER_L2BT_TP_INITIATED|GOEF_SERVER_BT_TP_INITIATED);
    }

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */

#if OBEX_ALLOW_SERVER_TP_DISCONNECT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ServerTpDisconnect
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initiate destruction of an OBEX Transport Connection from the
 *            GOEP Server.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerTpDisconnect(GoepServerApp *Service)
{
    ObStatus    status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) ||
        (Service->connId >= GOEP_NUM_OBEX_CONS) || 
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));


    if ((server->flags & GOEF_ACTIVE) == 0) {
        /* Already disconnected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    /* It's time to disconnect the transport connection */
    status = OBEX_ServerTpDisconnect(&server->obs, TRUE);

    if (status == OB_STATUS_SUCCESS) {
        /* The transport connection is down */
        server->currOp.oper = GOEP_OPER_NONE;
        server->flags &= ~GOEF_ACTIVE;
        server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
    } 
    else if (status == OB_STATUS_PENDING) {
        /* The transport connection is going down */
        server->currOp.handler = Service;
    }

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_ALLOW_SERVER_TP_DISCONNECT == XA_ENABLED */

#if OBEX_SRM_MODE == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ServerIssueSrmWait
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Queues up the Single Response Mode parameter to be issued during
 *            the next response packet. The SRM mode is set separately during
 *            the OBEX PUT/GET procedure only.  The only supported parameter
 *            in GOEP is the OSP_REQUEST_WAIT value.  Once this parameter is
 *            not issued in a response packet, it can no longer be issued
 *            for the duration of the operation.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ServerIssueSrmWait(GoepServerApp *Service)
{
    ObStatus            status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) ||
        (Service->connId >= GOEP_NUM_OBEX_CONS) || 
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

#if OBEX_SRM_MODE == XA_ENABLED
    if (server->tpType == OBEX_TP_BLUETOOTH) {
        /* SRMP is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    server->srmSettings.flags = OSF_PARAMETERS;
    server->srmSettings.mode = 0;
    server->srmSettings.parms = OSP_REQUEST_WAIT;

    status = OB_STATUS_SUCCESS;

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if OBEX_SESSION_SUPPORT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_AcceptSession()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Accepts the current session operation (Create or Resume) 
 *            indicated to the server by providing a Server Session that
 *            is necessary to proceed with the operation.
 *
 * Return:    OB_STATUS_SUCCESS - The session was accepted successfully.
 *            OB_STATUS_FAILED - The server was not ready to accept a session.
 *            OB_STATUS_INVALID_HANDLE - An object store handle is required but
 *                not specified in the resume parameters.
 *            OB_STATUS_INVALID_PARM - A provided session parameter was invalid
 *                or the session does not match the one the client requested.
 */
ObStatus GOEP_AcceptSession(GoepServerApp *Service, ObexServerSession *Session, 
                                  ObexSessionResumeParms *ResumeParms)
{
    ObStatus            status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service || !Session ) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service && Session);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) ||
        (Service->connId >= GOEP_NUM_OBEX_CONS) || 
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

    status = OBEX_ServerAcceptSession(&server->obs, Session, ResumeParms);

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ServerSetSessionTimeout()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Sets the session timeout for the active session on the OBEX
 *            server.  This timeout value will be sent to the client to 
 *            indicate the required OBEX Session Suspend timeout on the server.  
 *            If this value is lower than the value requested by the client, 
 *            this value will be set as the OBEX Session Suspend timeout.  At 
 *            every negotiation, this timeout will be requested, regardless of
 *            the last negotiated OBEX Session Suspend timeout.
 *
 * Return:    OB_STATUS_SUCCESS - Timeout successfully set.
 *            OB_STATUS_INVALID_PARM - A parameter is invalid.
 */
ObStatus GOEP_ServerSetSessionTimeout(GoepServerApp *Service, U32 Timeout)
{
    ObStatus            status;
    GoepServerObexCons *server;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Service) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Service);

    server = &GOES(servers)[Service->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Service->type >= GOEP_MAX_PROFILES) ||
        (Service->connId >= GOEP_NUM_OBEX_CONS) || 
        (server->profiles[Service->type] != Service)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Service->type < GOEP_MAX_PROFILES) && 
           (Service->connId < GOEP_NUM_OBEX_CONS) &&
           (server->profiles[Service->type] == Service));

    status = OBEX_ServerSetSessionTimeout(&server->obs, Timeout);

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED)
/*---------------------------------------------------------------------------
 * GOEP_ServerGetSdpProtocolDescList()
 *
 *     Retrieves the RFCOMM server transport's SDP Protocol descriptor list 
 *     attribute associated with a registered client application. This 
 *     information can then be combined with other attributes to register a 
 *     complete SDP record for a profile.
 *     
 * Requires:
 *     BT_STACK set to XA_ENABLED.
 *
 * Parameters:
 *     Service - A registered GOEP server application.
 *
 *     Attribute - The len, value and id fields in this structure are set
 *         by OBEX on return. The data returned in the value field must not
 *         be modified by the caller.
 *
 *     AttribSize - The number of SdpAttribute entries pointed to by the
 *         Attribute pointer. If zero, no entries are set.
 *
 * Returns:
 *     The number of Protocol Descriptor List attributes required to register 
 *     the server with SDP.
 */
U8 GOEP_ServerGetSdpProtocolDescList(GoepServerApp *Service, 
                                     SdpAttribute *Attribute, 
                                     U8 AttribSize)
{
    /* Pass this request straight down to OBEX */
    return OBEX_ServerGetSdpProtocolDescList(&GOES(servers)[Service->connId].obs.trans.ObexServerBtTrans,
        Attribute, AttribSize);
}
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) */

#if (BT_STACK == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED)
/*---------------------------------------------------------------------------
 * GOEP_ServerGetSdpGoepPsm()
 *
 *     Retrieves the L2CAP server transport's SDP GOEP PSM attribute 
 *     associated with a registered server application. This information can
 *     then be combined with other attributes to register a complete SDP
 *     record for a profile.
 *     
 * Requires:
 *     BT_STACK set to XA_ENABLED.
 *
 * Parameters:
 *     Service - A registered GOEP server application.
 *
 *     Attribute - The len, value and id fields in this structure are set
 *         by OBEX on return. The data returned in the value field must not
 *         be modified by the caller.
 *
 *     AttribSize - The number of SdpAttribute entries pointed to by the
 *         Attribute pointer. If zero, no entries are set.
 *
 * Returns:
 *     The number of Protocol Descriptor List attributes required to register 
 *     the server with SDP.
 */
U8 GOEP_ServerGetSdpGoepPsm(GoepServerApp *Service, 
                            SdpAttribute *Attribute, 
                            U8 AttribSize)
{
    /* Pass this request straight down to OBEX */
    return OBEX_ServerGetSdpGoepPsm(&GOES(servers)[Service->connId].obs.trans.ObexServerL2BtTrans,
        Attribute, AttribSize);
}
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) */
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

#if OBEX_ROLE_CLIENT == XA_ENABLED

/*---------------------------------------------------------------------------
 *            GOEP_RegisterClientEx()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initialize the OBEX Client.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_RegisterClientEx(GoepClientApp *Client,
                               const GoepRegisterClientParms *Parms)
{
    /* Note: for backwards-compatibility, this registration routine routes 
     * into the old GOEP_RegisterSrvr() interface, which remains basically
     * unmodified.
     */
    Client->callback   = Parms->callback;
    Client->type       = Parms->type;
    Client->appParent  = Parms->appParent;

#if BT_SERVICE_ID == XA_ENABLED
    if (Parms->serviceId) {
        Client->serviceId  = Parms->serviceId;
    } else {
        Client->serviceId = Client;
    }
#endif /* BT_SERVICE_ID == XA_ENABLED */

#if BT_SECURITY == XA_ENABLED
    return GOEP_RegisterClnt(Client, Parms->obStoreFuncs, &Parms->secParms);
#else
    return GOEP_RegisterClnt(Client, Parms->obStoreFuncs, 0);
#endif
}

/*---------------------------------------------------------------------------
 *            GOEP_RegisterClnt()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initialize the OBEX Client.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_RegisterClnt(GoepClientApp *Client, 
                           const ObStoreFuncTable *ObStoreFuncs,
                           const BtSecurityParms *SecParms)
{
    U8          id;
    ObStatus    status = OB_STATUS_SUCCESS;

    OS_LockObex();

#if BT_SECURITY == XA_DISABLED
    UNUSED_PARAMETER(SecParms);
#endif /* BT_SECURITY == XA_DISABLED */

#if XA_ERROR_CHECK == XA_ENABLED
    if (GOEC(initialized) != TRUE) {
        /* GOEP is not initialized */
        status = OB_STATUS_FAILED;
        goto Error;
    }

    if ((!Client) || (!ObStoreFuncs) || (Client->type >= GOEP_MAX_PROFILES) || 
       (Client->callback == 0)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client && ObStoreFuncs && (Client->type < GOEP_MAX_PROFILES) &&
          (Client->callback != 0));

    /* Create an OBEX connection if available, and if the Client requests 
     * a new connection 
     */
    if ((GOEC(connCount) < GOEP_NUM_OBEX_CONS)) {
        /* Get a free client conn Id */
        if (!GetFreeConnId(&id, GOEP_CLIENT)) {
            status = OB_STATUS_NO_RESOURCES;
            goto Error;
        }

        OBEX_InitAppHandle(&GOEC(clients)[id].obc.handle, 
                           GOEC(clients)[id].headerBlock, (U16)GOEP_CLIENT_HB_SIZE, ObStoreFuncs);

#if (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (BT_STACK_VERSION > 402)
        GOEC(clients)[id].obc.trans.ObexClientBtTrans.client.serviceId = Client->serviceId;
        GOEC(clients)[id].obc.trans.ObexClientL2BtTrans.client.serviceId = Client->serviceId;
#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
        GOEC(clients)[id].obc.trans.ObexServerBtTrans.server.serviceId = Client->serviceId;
        GOEC(clients)[id].obc.trans.ObexServerL2BtTrans.server.serviceId = Client->serviceId;
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */
#endif /* (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (BT_STACK_VERSION > 402) */

        status = OBEX_ClientInit(&GOEC(clients)[id].obc, GoepClntCallback, 
                                 OBEX_EnumTransports());

        if (status == OB_STATUS_SUCCESS) {
            Client->connId = id;
            /* Increment the current number of Obex connections */
            GOEC(connCount)++;
        }
    } else {
        /* Not enough Obex connections allocated in GOEP to allocate 
         * a new Connection Id 
         */
        Report(("GOEP: Current Obex connections = %i, greater than or equal \
                to GOEP_NUM_OBEX_CONS = %i\n", GOEC(connCount), 
                GOEP_NUM_OBEX_CONS));
        status = OB_STATUS_NO_RESOURCES;
        goto Error;
    }

    if (status == OB_STATUS_SUCCESS) {
        /* Set this client to "in use" */
        GOEC(clients)[Client->connId].connCount++;
        GOEC(clients)[Client->connId].profiles[Client->type] = Client;
        Client->connState = CS_DISCONNECTED;
        Client->obexConnId = OBEX_INVALID_CONNID;
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        Client->savedObexConnId = OBEX_INVALID_CONNID;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

        if (Client->appParent == 0) {
            Client->appParent = Client->callback;
        }

#if BT_SECURITY == XA_ENABLED
        /* Only register Security if we are not using the older "deprecated" API's
         * to setup security (GOEP_RegisterClientSecurityRecord)
         */
        if (SecParms) {
            /* Register security for the GOEP service */
            Client->secRecord.id =  SEC_GOEP_ID;
            Client->secRecord.channel = (U32)Client;
            Client->secRecord.level = SecParms->level;
#if BT_STACK_VERSION > 311
            Client->secRecord.pinLen = SecParms->pinLen;
#endif /* BT_STACK_VERSION > 311 */
#if BT_STACK_VERSION >= 402
            Client->secRecord.serviceId = Client->serviceId;
#endif /* BT_STACK_VERSION >= 402 */

            status = SEC_Register(&Client->secRecord);
            if (status == BT_STATUS_SUCCESS) {
                Client->registered = TRUE;
            }
        }
#endif /* BT_SECURITY == XA_ENABLED */
    }

Error:
    OS_UnlockObex();
    return status;
}

#if BT_SECURITY == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_RegisterClientSecurityRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Registers a security record for the GOEP client.
 *
 * Return:    (See goep.h)
 */
BtStatus GOEP_RegisterClientSecurityRecord(GoepClientApp *Client, BtSecurityLevel Level)
{
    BtStatus status = BT_STATUS_FAILED;

    if (Client->registered) {
        /* Already registered, so just return success */
        return BT_STATUS_SUCCESS;
    }

    OS_LockObex();

    /* Register security for the GOEP service */
    Client->secRecord.id =  SEC_GOEP_ID;
    Client->secRecord.channel = (U32)Client;
    Client->secRecord.level = Level;
#if BT_STACK_VERSION > 311
    Client->secRecord.pinLen = 0;
#endif /* BT_STACK_VERSION > 311 */
    status = SEC_Register(&Client->secRecord);
    if (status == BT_STATUS_SUCCESS) {
        Client->registered = TRUE;
    }

    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_UnregisterClientSecurityRecord()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Unregisters a security handler for the GOEP client.
 *
 * Return:    (See goep.h)
 */
BtStatus GOEP_UnregisterClientSecurityRecord(GoepClientApp *Client)
{
    BtStatus status = BT_STATUS_SUCCESS;

    OS_LockObex();

    if (Client->registered) {
        /* Unregister security for the GOEP Client */
        status = SEC_Unregister(&Client->secRecord);
        if (status == BT_STATUS_SUCCESS) {
            Client->registered = FALSE;
        }
    }

    OS_UnlockObex();

    return status;
}
#endif /* BT_SECURITY == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_GetObexClient()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieves the OBEX Client pertaining to a specific GOEP Client.  
 *            This function is valid after GOEP_Init and GOEP_RegisterClient 
 *            have been called.
 *
 * Return:    ObexClientApp
 *
 */
ObexClientApp* GOEP_GetObexClient(GoepClientApp *Client)
{
    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Client) || (Client->connId >= GOEP_NUM_OBEX_CONS)) {
        OS_UnlockObex();
        return 0;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client && (Client->connId < GOEP_NUM_OBEX_CONS));
    
    OS_UnlockObex();
    return &(GOEC(clients)[Client->connId].obc);
}

#if OBEX_DEINIT_FUNCS == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_DeregisterClient()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Deinitialize the OBEX Client.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_DeregisterClient(GoepClientApp *Client)
{   
    ObStatus    status;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if (GOEC(initialized) != TRUE) {
        /* GOEP is not initialized, so there is nothing to deinit */
        status = OB_STATUS_SUCCESS;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) || 
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */
    
    Assert((Client->type < GOEP_MAX_PROFILES) &&
           (client->profiles[Client->type] == Client));

    /* See if it is connected */
    if (Client->connState != CS_DISCONNECTED) {
        status = OB_STATUS_BUSY;
        goto Error;
    }
    client->profiles[Client->type] = 0;

    Assert(GOEC(connCount) > 0);

    if (--(client->connCount) > 0) {
        status = OB_STATUS_SUCCESS;
        goto Error;
    }

    --GOEC(connCount);
    status = OBEX_ClientDeinit(&client->obc);

    if (status == OB_STATUS_SUCCESS) {
#if BT_SECURITY == XA_ENABLED 
        if (Client->registered) {
            /* Unregister security for the GOEP Client */
            status = SEC_Unregister(&Client->secRecord);
            if (status == BT_STATUS_SUCCESS) {
                Client->registered = FALSE;
            }
        }
#endif /* BT_SECURITY == XA_ENABLED */
    }

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_DEINIT_FUNCS == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_Connect
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX Connect Request. If the Connect parameter is not
 *            used, set it to zero.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_Connect(GoepClientApp *Client, GoepConnectReq *Connect)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    ObStatus            status;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) || 
        (Client->connId >= GOEP_NUM_OBEX_CONS) || 
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    if (Connect && Connect->session) {
        if ((client->flags & GOEF_SESSION_ACTIVE) || 
            (client->flags & GOEF_CREATE_ISSUED)) {
            /* Already have an active session (or one is pending) */
            status = OB_STATUS_BUSY;
            goto Error;
        }
        
        if (client->tpType == OBEX_TP_BLUETOOTH) {
            /* Reliable Session is not allowed on the RFCOMM transport */
            status = OB_STATUS_INVALID_PARM;
            goto Error;
        }

        /* Create a Reliable Session */
        client->currOp.oper = GOEP_OPER_SESSION;
    } else 
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
    {
        /* Build Connect headers */
        status = BuildConnectReq(client, Connect);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }

        client->currOp.oper = GOEP_OPER_CONNECT;
    }

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS) 
#endif /* BT_SECURITY == XA_ENABLED */
    {
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        if (client->currOp.oper == GOEP_OPER_SESSION) {
            client->currOp.session.sessionOpcode = GOEP_OPER_CREATE_SESSION;
            status = OBEX_ClientCreateSession(&client->obc, 
                                              Connect->session, Connect->timeout); 
        } else 
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        {
            status = OBEX_ClientConnect(&client->obc);
        }
    }

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        if (client->currOp.oper == GOEP_OPER_SESSION) {
            client->flags |= GOEF_CREATE_ISSUED;
            /* Save the connect values */
            Client->connect = Connect;
        } else 
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        {
            client->flags |= GOEF_CONNECT_ISSUED;
        }
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_Disconnect
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX Disconnect Request.
 *
 * Return:    ObStatus
 *
 */
#if OBEX_SESSION_SUPPORT == XA_DISABLED
ObStatus GOEP_Disconnect(GoepClientApp *Client)
#else /* OBEX_SESSION_SUPPORT == XA_DISABLED */
ObStatus GOEP_Disconnect(GoepClientApp *Client, ObexClientSession *Session)
#endif /* OBEX_SESSION_SUPPORT == XA_DISABLED */
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    ObStatus            status;
    ObexProtocolHdr     header;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) || 
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) && 
           (client->profiles[Client->type] == Client));

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

    if (Client->obexConnId != OBEX_INVALID_CONNID) {
        header.type = OBEXH_CONNID;
        header.connId = Client->obexConnId;
        status = OBEX_ClientBuildProtocolHeader(&client->obc, &header);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }
    }

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    if (Session) {
        if ((client->flags & GOEF_SESSION_ACTIVE) == 0) {
            /* Already have a closed session */
            status = OB_STATUS_NO_CONNECT;
            goto Error;
        }
    }

    /* Save the session (if one exists ) to be closed after the Disconnect */
    Client->session = Session;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

    client->currOp.oper = GOEP_OPER_DISCONNECT;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

    status = OBEX_ClientDisconnect(&client->obc);

#if BT_SECURITY == XA_ENABLED
    /* We are disconnecting this service, so we should clear its authorized flag */
    if ((status == OB_STATUS_PENDING) || (status == OB_STATUS_SUCCESS)) {
        Client->authorized = FALSE;
    }
#endif /* BT_SECURITY == XA_ENABLED */

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
        Client->obexConnId = OBEX_INVALID_CONNID;
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_Push
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX Put. To delete an object, set the Object Store
 *            Handle in the GoepObjectReq to zero.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_Push(GoepClientApp *Client, GoepObjectReq *Object)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
#if OBEX_SRM_MODE == XA_ENABLED
    ObexSrmSettings     settings;
#endif /* OBEX_SRM_MODE == XA_ENABLED */
    U16                 len;
    ObStatus            status;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U16                 uniName[GOEP_MAX_UNICODE_LEN];
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    ObexProtocolHdr     header;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS)|| (!Object) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) && Object &&
           (client->profiles[Client->type] == Client));

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

#if OBEX_PUT_DELETE == XA_DISABLED
    if (Object->object == 0) {
        status = OB_STATUS_INVALID_HANDLE;
        goto Error;
    }
#endif /* OBEX_PUT_DELETE == XA_DISABLED */

#if OBEX_SRM_MODE == XA_ENABLED
    if ((Object->enableSrm) && (client->tpType == OBEX_TP_BLUETOOTH)) {
        /* SRM is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    if (Client->obexConnId != OBEX_INVALID_CONNID) {
        header.type = OBEXH_CONNID;
        header.connId = Client->obexConnId;
        status = OBEX_ClientBuildProtocolHeader(&client->obc, &header);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }
    }

#if OBEX_SRM_MODE == XA_ENABLED
    if ((Object->enableSrm) && 
        ((client->flags & GOEF_SRM_ACTIVE) == 0)) {
        /* Enable SRM was requested, but only do so if SRM is not already active */
        settings.flags = OSF_MODE;
        settings.mode = OSM_ENABLE;
        settings.parms = 0;

        status = OBEX_ClientSetSrmSettings(&client->obc, &settings);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }

        client->flags |= GOEF_SRM_REQUESTED;
        client->srmParamAllowed = TRUE;
    }

    /* GOEP does not allow SRMP headers for PUT Clients */
    client->srmParamAllowed = FALSE;
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    if (Object->name) {
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
        /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the name header
         * in Unicode form, to prevent memory corruption problems
         */
        Assert(GOEP_MAX_UNICODE_LEN >= (GoepStrLen(Object->name) * 2 + 2)); 
        len = (U16)(AsciiToUnicode(uniName, (const char *)Object->name) + 2);
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, (U8 *)uniName, 
                               len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
        len = GoepStrLen(Object->name) + 2;
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 
                               (U8 *)Object->name, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    }

    if (Object->type) {
        len = (U16)(OS_StrLen((const char *)Object->type) + 1);
        if (len > 1) {
            OBEXH_BuildByteSeq(&client->obc.handle, OBEXH_TYPE, Object->type, 
                               len);
        }
    }

    if (Object->object && Object->objectLen
#if OBEX_DYNAMIC_OBJECT_SUPPORT == XA_ENABLED
        && Object->objectLen != UNKNOWN_OBJECT_LENGTH
#endif /* OBEX_DYNAMIC_OBJECT_SUPPORT == XA_ENABLED */
        ) {
        OBEXH_Build4Byte(&client->obc.handle, OBEXH_LENGTH, Object->objectLen);
    }

    client->object = Object->object;
    client->currOp.oper = GOEP_OPER_PUSH;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS) 
#endif /* BT_SECURITY == XA_ENABLED */
    {
        /* Put or Put Delete based on the value of Object->object */
        status = OBEX_ClientPut(&client->obc, client->object); 
    }

    if (status == OB_STATUS_PENDING) {
#if OBEX_PUT_DELETE == XA_ENABLED
        if (Object->object == 0)
            client->currOp.oper = GOEP_OPER_DELETE;
#endif /* OBEX_PUT_DELETE == XA_ENABLED */
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_Pull
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX Get.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_Pull(GoepClientApp *Client, GoepObjectReq *Object, BOOL More)
{
    ObStatus            status;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U16                 uniName[GOEP_MAX_UNICODE_LEN];
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
#if OBEX_SRM_MODE == XA_ENABLED
    ObexSrmSettings     settings;
#endif /* OBEX_SRM_MODE == XA_ENABLED */
    GoepClientObexCons *client;
    ObexProtocolHdr     header;
    U16                 len, more = 0;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS)|| (!Object) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) && Object &&
           (client->profiles[Client->type] == Client));

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    if (Object->object == 0) {
        status = OB_STATUS_INVALID_HANDLE;
        goto Error;
    }

#if OBEX_SRM_MODE == XA_ENABLED
    if ((Object->enableSrm) && (client->tpType == OBEX_TP_BLUETOOTH)) {
        /* SRM is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    if (Client->obexConnId != OBEX_INVALID_CONNID) {
        header.type = OBEXH_CONNID;
        header.connId = Client->obexConnId;
        status = OBEX_ClientBuildProtocolHeader(&client->obc, &header);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }
    }

#if OBEX_SRM_MODE == XA_ENABLED
    if ((Object->enableSrm) && 
        ((client->flags & GOEF_SRM_ACTIVE) == 0)) {
        /* Enable SRM was requested, but only do so if SRM is not already active */
        settings.flags = OSF_MODE;
        settings.mode = OSM_ENABLE;
        settings.parms = 0;

        status = OBEX_ClientSetSrmSettings(&client->obc, &settings);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }

        client->flags |= GOEF_SRM_REQUESTED;
        client->srmParamAllowed = TRUE;
    }

    /* Build the SRM parameter header, if one exists to build */
    status = ClientBuildSrmHeader(client);
    if (status == OB_STATUS_NOT_FOUND) {
        client->srmParamAllowed = FALSE;
    } else if (status != OB_STATUS_SUCCESS) {
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    if (Object->name) {
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
        /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the name header
         * in Unicode form, to prevent memory corruption problems
         */
        Assert(GOEP_MAX_UNICODE_LEN >= (GoepStrLen(Object->name) * 2 + 2)); 
        len = (U16)(AsciiToUnicode(uniName, (const char *)Object->name) + 2);
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, (U8 *)uniName, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
        len = GoepStrLen(Object->name) + 2;
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, 
                               OBEXH_NAME, (U8 *)Object->name, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    } 

    if (Object->type) {
        len = (U16)(OS_StrLen((const char *)Object->type) + 1);
        if (len > 1) {
            OBEXH_BuildByteSeq(&client->obc.handle, OBEXH_TYPE, 
                               Object->type, len);
        }
    }
    
    client->currOp.oper = GOEP_OPER_PULL;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

    client->object = Object->object;

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS) 
#endif /* BT_SECURITY == XA_ENABLED */
    {
        status = OBEX_ClientGet(&client->obc, client->object, (((more > 0) || More)? TRUE : FALSE));
    }

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_SetFolder
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX SetPath command.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_SetFolder(GoepClientApp *Client, GoepSetFolderReq *Folder)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    U16                 len;
    ObStatus            status;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U16                 uniName[GOEP_MAX_UNICODE_LEN];
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    ObexProtocolHdr     header;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) || (!Folder) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) &&
           (Client->connId < GOEP_NUM_OBEX_CONS) && Folder &&
           (client->profiles[Client->type] == Client));

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    if (Client->obexConnId != OBEX_INVALID_CONNID) {
        header.type = OBEXH_CONNID;
        header.connId = Client->obexConnId;
        status = OBEX_ClientBuildProtocolHeader(&client->obc, &header);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }
    }

    if (Folder->name) {
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
        /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the name header
         * in Unicode form, to prevent memory corruption problems
         */
        Assert(GOEP_MAX_UNICODE_LEN >= (GoepStrLen(Folder->name) * 2 + 2)); 
        len = (U16)(AsciiToUnicode(uniName, (const char *)Folder->name) + 2);
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, (U8 *)uniName, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
        len = GoepStrLen(Folder->name) + 2;
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 
                               (U8 *)Folder->name, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 
                                0, 0);
        }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    } else if (Folder->reset) {
        /* Path reset requires an empty name header to be formed */
        OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
    }

    client->currOp.oper = GOEP_OPER_SETFOLDER;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS) 
#endif /* BT_SECURITY == XA_ENABLED */
    {
        status = OBEX_ClientSetPath(&client->obc, Folder->flags);
    }

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    } else {
        client->currOp.oper = GOEP_OPER_NONE;    
    }

Error:
    OS_UnlockObex();
    return status;
}

#if OBEX_ACTION_SUPPORT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_Action
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX Action. This routine can be used to copy, move/rename,
 *            or set permissions for an object.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_Action(GoepClientApp *Client, GoepActionId Id, 
                     GoepActionReq *Action)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    U16                 len;
    ObStatus            status;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U16                 uniName[GOEP_MAX_UNICODE_LEN];
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    ObexProtocolHdr     header;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS)|| (!Action) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if (Id > GOEP_ACTION_SET_PERMISSIONS) {
        /* Invalid Action */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if ((!Action->name || !Action->destName) && 
        ((Id == GOEP_ACTION_COPY) || (Id == GOEP_ACTION_MOVE))) {
        /* Name and DestName are required for Copy, Move/Rename */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if ((Id == GOEP_ACTION_SET_PERMISSIONS) && (Action->perms == OAP_NONE)) {
        /* Must specify permissions */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) && Action &&
           (client->profiles[Client->type] == Client));

    Assert(Id <= GOEP_ACTION_SET_PERMISSIONS);

    Assert((Action->name && Action->destName) || 
           ((Id != GOEP_ACTION_COPY) && (Id != GOEP_ACTION_MOVE)));

    Assert((Id != GOEP_ACTION_SET_PERMISSIONS) || (Action->perms != OAP_NONE));

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    if (Client->obexConnId != OBEX_INVALID_CONNID) {
        header.type = OBEXH_CONNID;
        header.connId = Client->obexConnId;
        status = OBEX_ClientBuildProtocolHeader(&client->obc, &header);
        if (status != OB_STATUS_SUCCESS) {
            goto Error;
        }
    }

    /* Build Action Identifier header */
    OBEXH_Build1Byte(&client->obc.handle, OBEXH_ACTION_ID, Id);

    if (Action->name) {
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
        /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the name header
         * in Unicode form, to prevent memory corruption problems
         */
        Assert(GOEP_MAX_UNICODE_LEN >= (GoepStrLen(Action->name) * 2 + 2)); 
        len = (U16)(AsciiToUnicode(uniName, (const char *)Action->name) + 2);
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, (U8 *)uniName, 
                               len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
        len = GoepStrLen(Action->name) + 2;
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 
                               (U8 *)Action->name, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_NAME, 0, 0);
        }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    }

    if (Action->destName) {
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
        /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the name header
         * in Unicode form, to prevent memory corruption problems
         */
        Assert(GOEP_MAX_UNICODE_LEN >= (GoepStrLen(Action->destName) * 2 + 2)); 
        len = (U16)(AsciiToUnicode(uniName, (const char *)Action->destName) + 2);
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_DEST_NAME, (U8 *)uniName, 
                               len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_DEST_NAME, 0, 0);
        }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
        len = GoepStrLen(Action->destName) + 2;
        if (len >= 2) {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_DEST_NAME, 
                               (U8 *)Action->destName, len);
        } else {
            OBEXH_BuildUnicode(&client->obc.handle, OBEXH_DEST_NAME, 0, 0);
        }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    }

    if (Action->perms) {
        /* Permissions header is requested, which can occur for Copy, 
         * Move/Rename, or Set Permissions 
         */
        OBEXH_Build4Byte(&client->obc.handle, OBEXH_PERMISSIONS, Action->perms);
    }

    client->currOp.oper = GOEP_OPER_ACTION;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS)
#endif /* BT_SECURITY == XA_ENABLED */
    {
        /* Action based on the action identifier */
        if (Id == GOEP_ACTION_COPY) {
            status = OBEX_ClientCopy(&client->obc); 
        } else if (Id == GOEP_ACTION_MOVE) {
            status = OBEX_ClientMove(&client->obc); 
        } else {
            Assert(Id == GOEP_ACTION_SET_PERMISSIONS);
            status = OBEX_ClientSetPermissions(&client->obc); 
        }
    }

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_ACTION_SUPPORT == XA_ENABLED */

#if OBEX_SESSION_SUPPORT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_SuspendSession
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX "Suspend Session" operation. 
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_SuspendSession(GoepClientApp *Client, 
                             ObexClientSession *Session, 
                             U32 Timeout)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    GoepClientObexCons *client;
    ObStatus            status = OB_STATUS_FAILED;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client || !Session) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) &&
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if (client->tpType == OBEX_TP_BLUETOOTH) {
        /* Reliable Session is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    client->currOp.oper = GOEP_OPER_SESSION;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    client->currOp.session.sessionOpcode = GOEP_OPER_SUSPEND_SESSION;
    status = OBEX_ClientSuspendSession(&client->obc, Session, Timeout); 

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ResumeSession
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX "Resume Session" operation. 
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ResumeSession(GoepClientApp *Client, 
                            ObexClientSession *Session, 
                            ObexSessionResumeParms *ResumeParms,
                            U32 Timeout)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    GoepClientObexCons *client;
    ObStatus            status = OB_STATUS_FAILED;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client || !Session) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client && Session);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) &&
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if (client->tpType == OBEX_TP_BLUETOOTH) {
        /* Reliable Session is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    client->currOp.oper = GOEP_OPER_SESSION;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS)
#endif /* BT_SECURITY == XA_ENABLED */
    {
        client->currOp.session.sessionOpcode = GOEP_OPER_RESUME_SESSION;
        status = OBEX_ClientResumeSession(&client->obc, Session, ResumeParms, Timeout); 
    } 
#if BT_SECURITY == XA_ENABLED
    else if (status == BT_STATUS_PENDING) {
        /* Security is pending, so we must save the session and timeout */
        Client->session = Session;
        Client->resumeParms = ResumeParms;
        Client->timeout = Timeout;
    }
#endif /* BT_SECURITY == XA_ENABLED */

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        if (ResumeParms) {
            /* Store the handler when an operation is being resumed */
            client->resumeHandler = Client;
        }
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
        client->flags |= GOEF_RESUME_ISSUED;
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ClientSetSessionTimeout
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Issue an OBEX "Set Session Timeout" operation. 
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ClientSetSessionTimeout(GoepClientApp *Client, 
                                      ObexClientSession *Session, 
                                      U32 Timeout)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    GoepClientObexCons *client;
    ObStatus            status = OB_STATUS_FAILED;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client || !Session) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client && Session);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) &&
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if (client->tpType == OBEX_TP_BLUETOOTH) {
        /* Reliable Session is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    client->currOp.oper = GOEP_OPER_SESSION;

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        Assert(more == 0);
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS)
#endif /* BT_SECURITY == XA_ENABLED */
    {
        status = OBEX_ClientSetSessionTimeout(&client->obc, Session, Timeout); 
    }

    if (status == OB_STATUS_PENDING) {
        client->currOp.handler = Client;
        /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
        client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    } else {
        client->currOp.oper = GOEP_OPER_NONE;
    }

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_ClientAbort
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Abort the current operation.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ClientAbort(GoepClientApp *Client)
{
    GoepClientObexCons *client;
    ObStatus status = OB_STATUS_FAILED;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) &&
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

#if BT_SECURITY == XA_ENABLED
    /* Assume success */
    status = BT_STATUS_SUCCESS;

    if (!Client->authorized) {
        /* Issued a security access request for this GOEP Client, if we have not 
         * done so already.
         */
        status = GoepClientSecAccessRequest(Client);
        if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
            Client->authorized = TRUE;
        }
    }

    if (status == BT_STATUS_SUCCESS) 
#endif /* BT_SECURITY == XA_ENABLED */
    {
        /* Don't bother checking the client->currOp.handler, since we might 
         * be aborting during the context of the operation call (e.g. GOEP_Push)
         * before the handler is set.  OBEX will only allow an Abort at the
         * proper time, so we don't need to check it again.
         */
        status = OBEX_ClientAbort(&client->obc);
    }

Error:
    OS_UnlockObex();
    return status;
}

#if OBEX_SRM_MODE == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ClientIssueSrmWait
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Queues up the Single Response Mode parameter to be issued during
 *            the next response packet. The SRM mode is set separately during
 *            the OBEX PUT/GET procedure only.  The only supported parameter
 *            in GOEP is the OSP_REQUEST_WAIT value.  Once this parameter is
 *            not issued in a request packet, it can no longer be issued
 *            for the duration of the operation.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_ClientIssueSrmWait(GoepClientApp *Client)
{
    ObStatus            status;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) && 
           (client->profiles[Client->type] == Client));

#if OBEX_SRM_MODE == XA_ENABLED
    if (client->tpType == OBEX_TP_BLUETOOTH) {
        /* SRMP is not allowed on the RFCOMM transport */
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    /* SRM Parameter is queued until the next request is issued */
    client->srmSettings.flags = OSF_PARAMETERS;
    client->srmSettings.mode = 0;
    client->srmSettings.parms = OSP_REQUEST_WAIT;

    status = OB_STATUS_SUCCESS;

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if OBEX_AUTHENTICATION == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_ClientVerifyAuthResponse
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Verifies the received authentication response.
 *
 * Return:    TRUE if server's response is authenticated.
 *
 */
BOOL GOEP_ClientVerifyAuthResponse(GoepClientApp *Client, 
                                   U8 *Password, 
                                   U8 PasswordLen)
{
    BOOL                    status;
    ObexAuthResponseVerify  verify;
    GoepClientObexCons     *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Client) || (!Password)) {
        status = FALSE;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client && Password);

    client = &GOEC(clients)[Client->connId];
    
    if (client->flags & GOEF_RESPONSE) {
        /* Verify the authenticaiton response */
        verify.nonce = client->nonce;
        verify.digest = client->currOp.response.digest;
        verify.password = Password;
        verify.passwordLen = PasswordLen;

        status = OBEXH_VerifyAuthResponse(&verify);
        goto Error;
    }

    status = FALSE;

Error:
    OS_UnlockObex();
    return status;
}
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GOEP_ClientGetTpConnInfo
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieves OBEX transport layer connection information.  This 
 *            function can be called when a transport connection is active to 
 *            retrieve connection specific information.   
 *
 * Return:    
 *     TRUE - The tpInfo structure was successfully completed.
 *     FALSE - The transport is not connected (XA_ERROR_CHECK only).
 *
 */
BOOL GOEP_ClientGetTpConnInfo(GoepClientApp *Client, 
                              ObexTpConnInfo *tpInfo)
                                   
{
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if ((!Client) || (!tpInfo)) {
        OS_UnlockObex();
        return FALSE;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client && tpInfo);

    client = &GOEC(clients)[Client->connId];
        
    OS_UnlockObex();
    return OBEX_GetTpConnInfo(&client->obc.handle, tpInfo);
}

/*---------------------------------------------------------------------------
 *            GOEP_TpConnect
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initiate an OBEX Transport Connection.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_TpConnect(GoepClientApp *Client, ObexTpAddr *Target)
{
    ObStatus           status;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) || !Target ||
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }

#if BT_STACK == XA_ENABLED
    if (!(Target->type & OBEX_TP_BLUETOOTH) && 
        !(Target->type & OBEX_TP_BLUETOOTH_L2CAP)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* BT_STACK == XA_ENABLED */
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) && Target &&
           (client->profiles[Client->type] == Client));

#if BT_STACK == XA_ENABLED
    Assert((Target->type & OBEX_TP_BLUETOOTH) || (Target->type & OBEX_TP_BLUETOOTH_L2CAP));
#endif /* BT_STACK == XA_ENABLED */

    if (client->currOp.handler) {
        status = OB_STATUS_BUSY;
        goto Error;
    }

    if (client->flags & GOEF_ACTIVE) {
        Client->connState = CS_CONNECTED;
        status = OB_STATUS_SUCCESS;
        goto Error;
    }

#if BT_STACK == XA_ENABLED
#if OBEX_RFCOMM_TRANSPORT == XA_ENABLED
    if (Target->type & OBEX_TP_BLUETOOTH) {
        /* Default is to use the RFCOMM transport */
        client->flags |= GOEF_CLIENT_BT_TP_INITIATED;
    }
#endif /* OBEX_RFCOMM_TRANSPORT == XA_ENABLED */

#if OBEX_L2CAP_TRANSPORT == XA_ENABLED
    if (Target->type & OBEX_TP_BLUETOOTH_L2CAP) {
        /* Override and use the L2CAP transport */
        Target->type = OBEX_TP_BLUETOOTH_L2CAP;
        client->flags |= GOEF_CLIENT_L2BT_TP_INITIATED;
    } else
#endif /* OBEX_L2CAP_TRANSPORT == XA_ENABLED */
#if OBEX_RFCOMM_TRANSPORT == XA_ENABLED
    {
        Target->type = OBEX_TP_BLUETOOTH;
    }
#endif /* OBEX_RFCOMM_TRANSPORT == XA_ENABLED */
#endif /* BT_STACK == XA_ENABLED */

    status = OBEX_ClientTpConnect(&client->obc, Target);
    
    /* Keep track of the target information */
    Client->targetConn = Target;

    if (status == OB_STATUS_SUCCESS) {
        /* The transport connection is up */
        client->currOp.oper = GOEP_OPER_NONE;
        Client->connState = CS_CONNECTED;
        client->flags |= GOEF_ACTIVE;
    } else if (status == OB_STATUS_PENDING) {
        /* The transport connection is coming up */
        Client->connState = CS_CONNECTING;
        client->currOp.handler = Client;
    } else {
        client->flags &= ~(GOEF_CLIENT_L2BT_TP_INITIATED|GOEF_CLIENT_BT_TP_INITIATED);
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_TpDisconnect
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Initiate destruction of an OBEX Transport Connection.
 *
 * Return:    ObStatus
 *
 */
ObStatus GOEP_TpDisconnect(GoepClientApp *Client)
{
    ObStatus    status;
    GoepClientObexCons *client;
    I8          i, inUse = 0;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
    
#if XA_ERROR_CHECK == XA_ENABLED
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) || 
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if ((Client->connState != CS_CONNECTED) && 
        (Client->connState != CS_CONNECTING)) {
        status = OB_STATUS_NO_CONNECT;
        goto Error;
    }

    for (i = 0; i < GOEP_MAX_PROFILES; i++) {
        if ((client->profiles[i]) &&
            (client->profiles[i]->connState == CS_CONNECTED)) {
            inUse++;
        }
    }

    if (inUse > 1) {
        /* Tell the client its disconnected */
        Client->connState = CS_DISCONNECTED;
        status = OB_STATUS_SUCCESS;
        goto Error;
    }

    /* It's time to disconnect the transport connection */
    status = OBEX_ClientTpDisconnect(&client->obc, TRUE);

#ifdef QNX_BLUETOOTH_OPP_REMOVE_HANDLER
    if ((status == OB_STATUS_SUCCESS)|| (status == OB_STATUS_NO_CONNECT)) {
        /* The transport connection is down */
        /* We are disconnected, remove the handler. */
        client->currOp.handler = 0;
#else
    if (status == OB_STATUS_SUCCESS) {
        /* The transport connection is down */
#endif /* QNX_BLUETOOTH_OPP_REMOVE_HANDLER */
        client->currOp.oper = GOEP_OPER_NONE;
        Client->connState = CS_DISCONNECTED;
        client->flags &= ~GOEF_ACTIVE;
        client->flags &= ~(GOEF_CLIENT_BT_TP_INITIATED|GOEF_CLIENT_L2BT_TP_INITIATED);
    } 
    else if (status == OB_STATUS_PENDING) {
        /* The transport connection is going down */
        client->currOp.handler = Client;
    }

Error:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ClientContinue
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Called to keep data flowing between the client and the server.
 *            This function must be called once for every GOEP_EVENT_CONTINUE
 *            event received. It does not have to be called in the context
 *            of the callback. It can be deferred for flow control reasons.
 *
 * Return:    ObStatus
 */
ObStatus GOEP_ClientContinue(GoepClientApp *Client)
{
#if GOEP_ADDITIONAL_HEADERS > 0
    U16                 more;
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */
    ObStatus            status;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];
        
#if XA_ERROR_CHECK == XA_ENABLED
    /* Verify Client Conn Id */
    if ((Client->type >= GOEP_MAX_PROFILES) ||
        (Client->connId >= GOEP_NUM_OBEX_CONS) || 
        (client->profiles[Client->type] != Client)) {
        status = OB_STATUS_INVALID_PARM;
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert((Client->type < GOEP_MAX_PROFILES) && 
           (Client->connId < GOEP_NUM_OBEX_CONS) &&
           (client->profiles[Client->type] == Client));

    if ((client->currOp.handler != Client) ||
        (client->currOp.event != GOEP_EVENT_CONTINUE)) {
        status = OB_STATUS_FAILED;
        goto Error;
    }

#if OBEX_SRM_MODE == XA_ENABLED
    /* Build the SRM parameter header, if one exists to build */
    if (client->currOp.oper == GOEP_OPER_PULL) {
        status = ClientBuildSrmHeader(client);
        if (status == OB_STATUS_NOT_FOUND) {
            client->srmParamAllowed = FALSE;
        } else if (status != OB_STATUS_SUCCESS) {
            status = OB_STATUS_FAILED;
            goto Error;
        }
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if GOEP_ADDITIONAL_HEADERS > 0
    if (ClientBuildHeaders(client, &more) == FALSE) {
        OBEXH_FlushHeaders(&client->obc.handle);
        status = OB_STATUS_FAILED;
        goto Error;
    }
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

    OBEX_ClientSendCommand(&client->obc);

    status = OB_STATUS_SUCCESS;

Error:
    OS_UnlockObex();
    return status;
}

#ifdef QNX_BLUETOOTH_OBEX_PKT_SIZE

U32 GOEP_ClientMaxPutPktSize(GoepClientApp *Client) {
    GoepClientObexCons *client;
    U32 size = 0;
    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        goto Done;
    }
#endif

    Assert(Client);

    client = &GOEC(clients)[Client->connId];

    size = ObParserMaxHbSize(&client->obc.handle.parser, OB_OPCODE_PUT);

Done:
    OS_UnlockObex();
    return size;
}

/* Max size of an OBEX TX frame was negotiated at connect time  */
U16 GOEP_ServerMaxGetPktSize(GoepServerApp *Server, BOOL enforceTransportPacketSize) {
    GoepServerObexCons *server;
    U16 size = 0;
    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Server) {
        goto Done;
    }
#endif

    Assert(Server);

    server = &GOES(servers)[Server->connId];

    /* If enforceTransportPacketSize == TRUE then only the max acl size
     * is returned.  If enforceTransportPacketSize == FALSE, then the maximum
     * packet size may exceed the transport size (for example, if using RFCOMM then
     * the returned size would be the negotiated maximum RFCOMM packet size).
     */
    BOOL savedTpEnforcePacketSize = server->obs.handle.tpEnforcePacketSize;
    server->obs.handle.tpEnforcePacketSize = enforceTransportPacketSize;
    size = ObParserMaxHbSize(&server->obs.handle.parser, OB_OPCODE_GET);
    server->obs.handle.tpEnforcePacketSize = savedTpEnforcePacketSize;

    /* The ObParserMaxHbSize takes away 3 header bytes that should be counted
     * for our purposes so add them back
     */
    size += 3;

Done:
    OS_UnlockObex();
    return size;
}

#endif

#if GOEP_ADDITIONAL_HEADERS > 0
/*---------------------------------------------------------------------------
 *            GOEP_ClientQueueHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function queues a Byte Sequence, UNICODE, 1-byte, or 4-byte 
 *            OBEX header for transmission by the GOEP Client.
 *
 * Return:    True if the OBEX Header was queued successfully.
 */
BOOL GOEP_ClientQueueHeader(GoepClientApp *Client, ObexHeaderType Type,
                            const U8 *Value, U16 Len) {
    U8                  i;
    BOOL                status;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        status = FALSE;
        goto Done;
    }
   
    /* NAME and TYPE headers are not recommended, since they are typically built by GOEP
     * when initiating Push, Pull, or SetPath operations. If header ordering is critical, 
     * these headers can be queued in a different order.  If this is done, be sure not to 
     * add these headers a second time when the Push, Pull, or SetPath operation is initiated.
     */

    /* Only allow certain headers to be queued for transmission */
    if ((Type == OBEXH_TARGET) || (Type == OBEXH_WHO) || (Type == OBEXH_CONNID) ||
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        (Type == OBEXH_SESSION_PARAMS) || (Type == OBEXH_SESSION_SEQ_NUM) ||
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        (Type == OBEXH_AUTH_CHAL) || (Type == OBEXH_AUTH_RESP) || (Type == OBEXH_WAN_UUID)) {
        status = FALSE;
        goto Done;
    }

#if GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED
    /* Unicode headers must have an even length if we aren't
     * automatically performing the unicode conversions. 
     */
    if (OBEXH_IsUnicode(Type) && ((Len % 2) != 0)) {
        status = FALSE;
        goto Done;
    }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED */
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];

    if ((client->flags & GOEF_ACTIVE) == 0) {
        /* Not connected */
        status = FALSE;
        goto Done;
    }

#if XA_ERROR_CHECK == XA_ENABLED
    /* Non-Body headers cannot exceed the available OBEX packet size */
    if ((Type != OBEXH_BODY) && (Len > OBEXH_GetAvailableTxHeaderLen(&client->obc.handle))) {
        status = FALSE;
        goto Done;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) {
        if (client->queuedHeaders[i].type == 0) {
            /* Found an empty header to queue */
            client->queuedHeaders[i].type = Type;
            client->queuedHeaders[i].buffer = Value;
            client->queuedHeaders[i].len = Len;
            status = TRUE;
            goto Done;
        }
    }

    /* No empty headers available */
    status = FALSE;

Done:
    OS_UnlockObex();
    return status;
}

/*---------------------------------------------------------------------------
 *            GOEP_ClientFlushQueuedHeaders()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function flushes any headers queued with 
 *            GOEP_ClientQueueHeader.  Since a header is not flushed by 
 *            GOEP until it has been sent, it is possible that an upper
 *            layer profile API may need to flush these queued headers in
 *            the case of an internal failure.
 *
 * Return:    None.
 */
void GOEP_ClientFlushQueuedHeaders(GoepClientApp *Client) {
    U8                  i;
    GoepClientObexCons *client;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Client) {
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Client);

    client = &GOEC(clients)[Client->connId];

    /* Clear all of the built headers */
    for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) 
        OS_MemSet((U8 *)&client->queuedHeaders[i], 0, sizeof(client->queuedHeaders[i]));

#if XA_ERROR_CHECK == XA_ENABLED
Error:
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    OS_UnlockObex();
}
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED)
#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
/*---------------------------------------------------------------------------
 * GOEP_ClientGetSdpProtocolDescList()
 *
 *     Retrieves the RFCOMM server transport's SDP Protocol descriptor list 
 *     attribute associated with a registered client application. This 
 *     information can then be combined with other attributes to register a 
 *     complete SDP record for a profile.
 *     
 * Requires:
 *     BT_STACK, OBEX_ROLE_CLIENT, and OBEX_ALLOW_SERVER_TP_CONNECT
 *     set to XA_ENABLED.
 *
 * Parameters:
 *     Client - A registered GOEP client application.
 *
 *     Attribute - The len, value and id fields in this structure are set
 *         by OBEX on return. The data returned in the value field must not
 *         be modified by the caller.
 *
 *     AttribSize - The number of SdpAttribute entries pointed to by the
 *         Attribute pointer. If zero, no entries are set.
 *
 * Returns:
 *     The number of Protocol Descriptor List attributes required to register 
 *     the client with SDP.
 */
U8 GOEP_ClientGetSdpProtocolDescList(GoepClientApp *Client, 
                                     SdpAttribute *Attribute, 
                                     U8 AttribSize)
{
    /* Pass this request straight down to OBEX */
    return OBEX_ServerGetSdpProtocolDescList(&GOEC(clients)[Client->connId].obc.trans.ObexServerBtTrans,
        Attribute, AttribSize);
}
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) */

#if (BT_STACK == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED)
#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
/*---------------------------------------------------------------------------
 * GOEP_ClientGetSdpGoepPsm()
 *
 *     Retrieves the L2CAP server transport's SDP GOEP PSM attribute 
 *     associated with a registered client application. This information can
 *     then be combined with other attributes to register a complete SDP
 *     record for a profile.
 *     
 * Requires:
 *     BT_STACK, OBEX_ROLE_CLIENT, and OBEX_ALLOW_SERVER_TP_CONNECT 
 *     set to XA_ENABLED.
 *
 * Parameters:
 *     Client - A registered GOEP client application.
 *
 *     Attribute - The len, value and id fields in this structure are set
 *         by OBEX on return. The data returned in the value field must not
 *         be modified by the caller.
 *
 *     AttribSize - The number of SdpAttribute entries pointed to by the
 *         Attribute pointer. If zero, no entries are set.
 *
 * Returns:
 *     The number of Protocol Descriptor List attributes required to register 
 *     the server with SDP.
 */
U8 GOEP_ClientGetSdpGoepPsm(GoepClientApp *Client, 
                            SdpAttribute *Attribute, 
                            U8 AttribSize)
{
    /* Pass this request straight down to OBEX */
    return OBEX_ServerGetSdpGoepPsm(&GOEC(clients)[Client->connId].obc.trans.ObexServerL2BtTrans,
        Attribute, AttribSize);
}
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) */
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_SESSION_SUPPORT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GOEP_GetSessionTimeout()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Retrieve the session suspend timeout value from an OBEX Session
 *            structure.
 *
 * Return:    Timeout value in milliseconds. The value of 0xFFFFFFFF indicates
 *            that the session should never timeout.
 */
U32 GOEP_GetSessionTimeout(void *Session)
{
    U32     timeout = 0;

    OS_LockObex();

#if XA_ERROR_CHECK == XA_ENABLED
    if (!Session) {
        goto Error;
    }
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    Assert(Session);

    timeout = OBEX_GetSessionTimeout(Session);

#if XA_ERROR_CHECK == XA_ENABLED
Error:
#endif /* XA_ERROR_CHECK == XA_ENABLED */

    OS_UnlockObex();
    return timeout;
}
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

/****************************************************************************
 *
 * Internal Functions
 *
 ****************************************************************************/

#if OBEX_ROLE_SERVER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GoepSrvrCallback
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function processes OBEX Server protocol events.
 *
 * Return:    void
 *
 */
static void GoepSrvrCallback(ObexServerCallbackParms *parms)
{
#if BT_SECURITY == XA_ENABLED
    BtStatus            status;
#endif /* BT_SECURITY == XA_ENABLED */
#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED)
    GoepServerApp      *sApp = 0;
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED) */
    ObexTpConnInfo      tpInfo;
    GoepServerObexCons *server = (GoepServerObexCons*)parms->server;

#if XA_DEBUG == XA_ENABLED
#ifdef QNX_BLUETOOTH_FIX_IANYWHERE_COMPILER_WARNINGS
    extern char *pServerEvent(ObServerEvent event);
#endif
    Report(("GOEP: Received Event %s\n", pServerEvent(parms->event)));
    /* server->lastEvent = ServerEventVerifier(server->lastEvent, parms->event); */
#endif /* XA_DEBUG == XA_ENABLED */
    Assert(IsObexLocked());

    switch ( parms->event ) {
    case OBSE_SEND_RESPONSE:
        server->oustandingResp = TRUE;

#if BT_SECURITY == XA_ENABLED
        /* Make sure the Server has been assigned */
        if ((server->currOp.event == GOEP_EVENT_NONE) &&
            (server->currOp.handler == 0)) {
            /* The Server hasn't been assigned yet. Indicate the START
             * event early to make sure the server is assigned.
             */
            NotifyCurrServer(parms->server, GOEP_EVENT_START);
        }

        if (server->currOp.handler) {
            /* Assume success */
            status = BT_STATUS_SUCCESS;

            if (!server->currOp.handler->authorized) {
                status = GoepServerSecAccessRequest(server);
                if ((status == BT_STATUS_SUCCESS) || (status == BT_STATUS_PENDING)) {
                    server->currOp.handler->authorized = TRUE;
                }
            }

            if (status == BT_STATUS_SUCCESS) {
                NotifyCurrServer(parms->server, GOEP_EVENT_CONTINUE);
            } else if (status != BT_STATUS_PENDING) {
                /* Abort the connection operation */
                status = OBEX_ServerAbort(&server->obs, OBRC_TRANSPORT_SECURITY_FAILURE, FALSE);
                if (status != OB_STATUS_SUCCESS) {
                    Report(("GOEP: Failed call to OBEX_ServerAbort: %d", status));
                }
                NotifyCurrServer(parms->server, GOEP_EVENT_CONTINUE);
            }
        } else {
            /* No server existed to perform security with */
            NotifyCurrServer(parms->server, GOEP_EVENT_CONTINUE);
        }
#else /* BT_SECURITY == XA_ENABLED */
        NotifyCurrServer(parms->server, GOEP_EVENT_CONTINUE);
#endif /* BT_SECURITY == XA_ENABLED */
        break;

#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
    case OBSE_DISCOVERY_FAILED:
        Assert((server->flags & GOEF_ACTIVE) == 0);
        server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
        NotifyCurrServer(parms->server, GOEP_EVENT_DISCOVERY_FAILED);
        break;

#ifdef QNX_BLUETOOTH_ODR_PROTOCOL_COLLISION
    case OBSE_PROTOCOL_COLLISION:
        Assert((server->flags & GOEF_ACTIVE) == 0);
#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED)
        if ((server->flags & GOEF_CLIENT_L2BT_TP_INITIATED) && (server->flags & GOEF_CLIENT_BT_TP_INITIATED)) {
            /* OBEX/L2CAP connection attempt failed; try OBEX/RFCOMM automatically */
            sApp = server->currOp.handler;
            sApp->targetConn->type = OBEX_TP_BLUETOOTH;
            sApp->targetConn->proto.bt.addr = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.rm->bdAddr;
            sApp->targetConn->proto.bt.sdpQuery = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.parms;
            sApp->targetConn->proto.bt.sdpQueryLen = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.plen;
            sApp->targetConn->proto.bt.sdpQueryType = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.type;
            server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            server->currOp.handler = 0; /* Clear the current handler for the connection attempt */
            (void)GOEP_ServerTpConnect(sApp, sApp->targetConn);
        } else 
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED) */
        {
            server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            NotifyCurrServer(parms->server, GOEP_EVENT_PROTOCOL_COLLISION);
        }
        break;
#endif

    case OBSE_NO_SERVICE_FOUND:
        Assert((server->flags & GOEF_ACTIVE) == 0);
#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED)
        if ((server->flags & GOEF_CLIENT_L2BT_TP_INITIATED) && (server->flags & GOEF_CLIENT_BT_TP_INITIATED)) {
            /* OBEX/L2CAP connection attempt failed; try OBEX/RFCOMM automatically */
            sApp = server->currOp.handler;
            sApp->targetConn->type = OBEX_TP_BLUETOOTH;
            sApp->targetConn->proto.bt.addr = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.rm->bdAddr;
            sApp->targetConn->proto.bt.sdpQuery = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.parms;
            sApp->targetConn->proto.bt.sdpQueryLen = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.plen;
            sApp->targetConn->proto.bt.sdpQueryType = parms->server->trans.ObexClientL2BtTrans.sdpQueryToken.type;
            server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            server->currOp.handler = 0; /* Clear the current handler for the connection attempt */
            (void)GOEP_ServerTpConnect(sApp, sApp->targetConn);
        } else 
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) && (OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED) */
        {
            server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            NotifyCurrServer(parms->server, GOEP_EVENT_NO_SERVICE_FOUND);
        }
        break;
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */

    case OBSE_PRECOMPLETE:
        NotifyCurrServer(parms->server, GOEP_EVENT_PRECOMPLETE);
        break;

    case OBSE_CONNECTED:
        server->flags |= GOEF_ACTIVE;

        server->currOp.oper = GOEP_OPER_NONE;

        /* Grab the active transport type to determine whether Session + SRM are allowed */
        AssertEval(OBEX_GetTpConnInfo(&server->obs.handle, &tpInfo));
        server->tpType = tpInfo.tpType;

        NotifyAllServers(parms->server, GOEP_EVENT_TP_CONNECTED);
        break;

    case OBSE_DISCONNECT:
#if BT_SECURITY == XA_ENABLED
        if (server->flags & GOEF_SEC_ACCESS_REQUEST) {
            AssertEval(SEC_CancelAccessRequest(&(server->currOp.handler->secToken)) == BT_STATUS_SUCCESS);
            server->flags &= ~GOEF_SEC_ACCESS_REQUEST;
        }
#endif /* BT_SECURITY == XA_ENABLED */

        server->tpType = OBEX_TP_NONE;

        if ((server->flags & GOEF_ACTIVE) ||
            (server->flags & GOEF_SERVER_BT_TP_INITIATED) ||
            (server->flags & GOEF_SERVER_L2BT_TP_INITIATED)) {
            server->flags &= ~GOEF_ACTIVE;
            server->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            server->currOp.oper = GOEP_OPER_NONE;
            server->currOp.discReason = parms->u.discReason;
            NotifyAllServers(parms->server, GOEP_EVENT_TP_DISCONNECTED);
        }
        break;

    case OBSE_HEADER_RX:
        GoepServerProcessHeaders(parms);
        break;

    case OBSE_DELETE_OBJECT:
        /* Change operation to delete, drop into START state notification */
        server->currOp.oper = GOEP_OPER_DELETE;
        NotifyCurrServer(parms->server, GOEP_EVENT_DELETE_OBJECT);
        break;

    case OBSE_PROVIDE_OBJECT:
        NotifyCurrServer(parms->server, GOEP_EVENT_PROVIDE_OBJECT);
        break;
        
#if OBEX_ACTION_SUPPORT == XA_ENABLED
    case OBSE_COPY_OBJECT:
        /* Copy Object */
        NotifyCurrServer(parms->server, GOEP_EVENT_COPY_OBJECT);
        break;

    case OBSE_MOVE_OBJECT:
        /* Move/Rename Object */
        NotifyCurrServer(parms->server, GOEP_EVENT_MOVE_OBJECT);
        break;

    case OBSE_SET_PERMS_OBJECT:
        /* Set Permissions of Object */
        NotifyCurrServer(parms->server, GOEP_EVENT_SET_PERMS_OBJECT);
        break;
#endif /* OBEX_ACTION_SUPPORT == XA_ENABLED */

    case OBSE_ABORTED:
#if BT_SECURITY == XA_ENABLED
        if (server->flags & GOEF_SEC_ACCESS_REQUEST) {
            AssertEval(SEC_CancelAccessRequest(&(server->currOp.handler->secToken)) == BT_STATUS_SUCCESS);
            server->flags &= ~GOEF_SEC_ACCESS_REQUEST;

            /* Clear the authorized flag, since security did not complete */
            server->currOp.handler->authorized = FALSE;
}
#endif /* BT_SECURITY == XA_ENABLED */
 
        if (server->currOp.event != GOEP_EVENT_NONE) {
            /* Makes sure that the currOp.oper is correct during Aborts. */
            if (server->abortedOper != GOEP_OPER_NONE) {
                server->currOp.oper = server->abortedOper;
                server->abortedOper = GOEP_OPER_NONE;
            }
            NotifyCurrServer(parms->server, GOEP_EVENT_ABORTED);
        }
        server->currOp.oper = GOEP_OPER_NONE;
        break;

    case OBSE_COMPLETE:
#if OBEX_SRM_MODE == XA_ENABLED
        /* If SRM was enabled only for this operation, then it will be disabled
         * by OBEX automatically.
         */
        if (server->flags & GOEF_SRM_ACTIVE) {
            server->flags &= ~GOEF_SRM_ACTIVE;
        }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if BT_SECURITY == XA_ENABLED
        if ((server->currOp.oper == GOEP_OPER_DISCONNECT) && (server->currOp.handler)) {
            /* Disconnect completed for this service, so let's clear the authorized flag */
            server->currOp.handler->authorized = FALSE;
        }
#endif /* BT_SECURITY == XA_ENABLED */

        NotifyCurrServer(parms->server, GOEP_EVENT_COMPLETE);
        server->currOp.oper = GOEP_OPER_NONE;
        break;

    case OBSE_PUT_START:
#if OBEX_SRM_MODE == XA_ENABLED
        server->srmParamAllowed = TRUE;
#endif /* OBEX_SRM_MODE == XA_ENABLED */
        StartServerOperation(parms->server, GOEP_OPER_PUSH);
        break;

    case OBSE_GET_START:
#if OBEX_SRM_MODE == XA_ENABLED
        server->srmParamAllowed = TRUE;
#endif /* OBEX_SRM_MODE == XA_ENABLED */
        StartServerOperation(parms->server, GOEP_OPER_PULL);
        break;
    
    case OBSE_SET_PATH_START:
        StartServerOperation(parms->server, GOEP_OPER_SETFOLDER);
        server->currOp.info.setfolder.flags = parms->u.setPathFlags;
        break;
        
#if OBEX_ACTION_SUPPORT == XA_ENABLED
    case OBSE_ACTION_START:
        StartServerOperation(parms->server, GOEP_OPER_ACTION);
        break;
#endif /* OBEX_ACTION_SUPPORT == XA_ENABLED */

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    case OBSE_SESSION_START:
        if (server->tpType == OBEX_TP_BLUETOOTH) {
            /* Reliable Session is not allowed on the RFCOMM transport */
            (void)OBEX_ServerAbort(parms->server, OBRC_FORBIDDEN, TRUE);
            break;
        }
        server->currOp.session.sessionOpcode = parms->opcode;
        StartServerOperation(parms->server, GOEP_OPER_SESSION);
        break;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
    case OBSE_CONNECT_START:
        StartServerOperation(parms->server, GOEP_OPER_CONNECT);
        break;
        
    case OBSE_DISCONNECT_START:
        StartServerOperation(parms->server, GOEP_OPER_DISCONNECT);
        break;
            
    case OBSE_ABORT_START:
        server->abortedOper = server->currOp.oper;
        StartServerOperation(parms->server, GOEP_OPER_ABORT);
        /* Inform the application that an Abort operation is
         * starting.  If an outstanding response exists, it must 
         * be sent via GOEP_ServerContinue to allow the Abort 
         * operation to complete.
         */
        Assert(server->currOp.handler == 0);
        AssociateServer(parms->server);
        NotifyCurrServer(parms->server, GOEP_EVENT_START);
        break;

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    case OBSE_SUSPENDED:
    case OBSE_SESSION_SUSPENDED:
        server->currOp.session.suspended.headerBuff = parms->u.suspended.headerBuff;
        server->currOp.session.suspended.headerLen = parms->u.suspended.headerLen;
        server->currOp.session.suspended.obsh = parms->u.suspended.obsh;
        server->currOp.session.suspended.session = parms->u.suspended.session;

        if (parms->event == OBSE_SESSION_SUSPENDED) {
            /* Reliable Session was suspended */
            NotifyCurrServer(parms->server, GOEP_EVENT_SESSION_SUSPENDED);
        } else {
            /* Reliable Session and an active operation was suspended */
            NotifyCurrServer(parms->server, GOEP_EVENT_SUSPENDED);
        }
        break;

    case OBSE_SESSION_PARMS_RX:
        server->currOp.session.sessionParms = parms->u.sessionParms;
        NotifyCurrServer(parms->server, GOEP_EVENT_SESSION_PARMS_RX);
        break;

    case OBSE_SESSION_ERROR:
        /* If we have already cleared the operation handler by now, then skip this */
        if (!server->currOp.handler) break;

        server->currOp.session.session = parms->u.session;
        NotifyCurrServer(parms->server, GOEP_EVENT_SESSION_ERROR);
        break;

    case OBSE_PROVIDE_SESSION:
        NotifyCurrServer(parms->server, GOEP_EVENT_PROVIDE_SESSION);
        break;

    case OBSE_RESUME_OPER:
        /* Restore the operation being resumed */
        ResumeServerOperation(parms->server, parms->opcode);
        NotifyCurrServer(parms->server, GOEP_EVENT_RESUME_OPER);
        break;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
    }
}

#ifdef QNX_BLUETOOTH_GOEP_UNICODE_HANDLING
/*
 * Calculate how many UTF-8 bytes will be taken up by the supplied GopeUniType (UCS-2) character.
 * This code is copied from and mimics GoepStrnCpy() in goep_types.c
 */
int utf8Bytes( GoepUniType character ) {
    U32 nextChar = ntohs(character);

    if( nextChar <= 0x007f ) {
        /* Characters up to 127 are plain ASCII - copy as-is  */
        return 1;
    } else {
        if( nextChar <= 0x07ff ) {
            /* Characters from 0x80 to 0x7ff are 2-byte sequence */
            return 2;
        } else {
            if( nextChar <= 0xffff ) {
                /* Characters from 0x800 to 0xffff are 3-byte sequence */
                return 3;
            } else {
                if( nextChar <= 0x1fffff ) {
                    /* Characters from 0x10000 to 0x1fffff are 4-byte sequence */
                    return 4;
                } else {
                    /* Error - invalid character encountered.  Fudge it with a masked ASCII byte  */
                    return 1;
                }
            }
        }
    }
}

/*
 * Calculate how many UTF-8 bytes will be taken up by the supplied GoepUniType (UCS-2) string.
 */
int utf8Size( GoepUniType *buffer, int bufferlen ) {

    int totalLen = 0;

    while( *buffer && bufferlen-- > 0 ) {
        totalLen += utf8Bytes( *buffer );
        ++buffer;
    }

    return totalLen;
}

/*
 * Calculate how much of the supplied GoepUniType (UCS-2) string can fit in maxUtf8Len UTF-8 bytes.
 * NOTE:  The return value is the number of GoepUniType characters (not bytes).
 */
int utf8Max( GoepUniType *buffer, int maxUtf8Len ) {

    int totalLen = 0, consumed = 0;

    while( *buffer && totalLen < maxUtf8Len ) {
        if( ( totalLen += utf8Bytes( *buffer ) ) <= maxUtf8Len ) {
            ++consumed;
        }
        ++buffer;
    }

    return consumed;
}

/*
 * Find the location of the last "character" within the supplied GoepUniType (UCS-2) string.
 * NOTE:  The return value is the number of GoepUniType characters (not bytes).
 */
GoepUniType *ucs2rchr( GoepUniType *buffer, GoepUniType character ) {

    int i;

    for(i = 0; buffer[i]; i++ ){
    }

    for(; i > 0; --i ) {
        if( htons( buffer[i] ) == character ) {
            return &(buffer[i]);
        }
    }

    return NULL;
}
#endif /* QNX_BLUETOOTH_GOEP_UNICODE_HANDLING */

/*---------------------------------------------------------------------------
 *            GoepServerProcessHeaders
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the incoming headers based on the current operation.
 *            Note that ConnId and Target are processed by the OBEX layer.
 *
 * Return:    void
 *
 */
static void GoepServerProcessHeaders(ObexServerCallbackParms *parms)
{
    GoepServerObexCons *server = (GoepServerObexCons*)parms->server;

    switch (server->currOp.oper) {
    case GOEP_OPER_CONNECT:
#if GOEP_MAX_WHO_LEN > 0
        if (parms->u.headerRx.type == OBEXH_WHO) {
            DoHeaderCopy(server->currOp.info.connect.who, 
                         &server->currOp.info.connect.whoLen, GOEP_MAX_WHO_LEN, 
                         parms->u.headerRx.buff, parms->u.headerRx.currLen);
            return;
        }
#endif /* GOEP_MAX_WHO_LEN > 0 */
        break;

    case GOEP_OPER_PUSH:
    case GOEP_OPER_PULL:
        switch (parms->u.headerRx.type) {
        case OBEXH_NAME:
            DoUniHeaderCopy(server->currOp.info.pushpull.name, 
                            &server->currOp.info.pushpull.nameLen, GOEP_MAX_UNICODE_LEN, 
                            parms->u.headerRx.buff, parms->u.headerRx.currLen);                

#ifdef QNX_BLUETOOTH_GOEP_UNICODE_HANDLING
            /*
             * IF we have more .buff than would fit into our UTF-8 buffer in iobt_opp_server_object_request(),
             * then the file extension may have been chopped off.
             *
             * If there IS a file extension in .buff,
             *      - determine the length of the extension once converted to UTF-8
             *      - subtract it from the UTF-8 destination buffer length
             *      - copy the UCS-2 extension data OVER the correct spot in the UCS-2 .name buffer
             *
             * NOTE:  For clarity, some of the values used herein are:
             *      - server->currOp.info.pushpull.nameLen - number of UCS-2 bytes in server->currOp.info.pushpull.name (including terminating NULL)
             *      - parms->u.headerRx.currLen            - number of UCS-2 bytes in parms->u.headerRx.buff (including terminating NULL)
             *      - DEST_FILENAME_MAX                    - number of UTF-8 bytes allowed in filename
             */

/*  NOTE:  We must reserve room for "(1)" in case we need to rename a subsequent file transfer,
        so make the maximum length 8 less than the file system maximum just for safety.         */
#define DEST_FILENAME_MAX   (FILENAME_MAX - 8)
            /*  Is there more in headerRx.buff than would fit in the UTF-8 buffer this is destined for? */
            if ( utf8Size( (GoepUniType *)parms->u.headerRx.buff, parms->u.headerRx.currLen ) > DEST_FILENAME_MAX ) {
                GoepUniType *extPtr = NULL;
                /* Is there a period (ie. file extension) in the headerRx.buff?     */
                if ( ( extPtr = ucs2rchr( (GoepUniType *)parms->u.headerRx.buff, '.' ) ) != NULL ) {
                    int extUtf8Len, extUcs2Len, extUcs2Off;
                    extUtf8Len = utf8Size( extPtr, GOEP_MAX_UNICODE_LEN );
                    extUcs2Len = GoepStrLen( extPtr );
                    /* Determine the offset where we should copy the extension by determining where it would fit in the UTF-8 output buffer */
                    extUcs2Off = utf8Max( (GoepUniType *)parms->u.headerRx.buff, ( DEST_FILENAME_MAX - extUtf8Len ) );
                    /* Is the extension more than just a period AND is the extension shorter than the UTF-8 filename buffer?    */
                    if( extUtf8Len > 1 && ( extUtf8Len + 1 ) < DEST_FILENAME_MAX ) {
                        /* Initialize the "current length" to the location where the extension should go, then concatenate the extension.   */
                        server->currOp.info.pushpull.nameLen = (extUcs2Off * sizeof(GoepUniType));
                        DoUniHeaderCopy( server->currOp.info.pushpull.name,
                                        &server->currOp.info.pushpull.nameLen, GOEP_MAX_UNICODE_LEN,
                                        (U8 *)extPtr, extUcs2Len + sizeof(GoepUniType));
                    }
                }
            }
#endif /* QNX_BLUETOOTH_GOEP_UNICODE_HANDLING */
            return;

#if GOEP_MAX_TYPE_LEN > 0
        case OBEXH_TYPE:
            DoHeaderCopy(server->currOp.info.pushpull.type, 
                         &server->currOp.info.pushpull.typeLen, GOEP_MAX_TYPE_LEN, 
                         parms->u.headerRx.buff, parms->u.headerRx.currLen);                
            return;
#endif /* GOEP_MAX_TYPE_LEN > 0 */
        case OBEXH_LENGTH:
            server->currOp.info.pushpull.objectLen = parms->u.headerRx.value;
        }
        break;

    case GOEP_OPER_SETFOLDER:
        if (parms->u.headerRx.type == OBEXH_NAME) {
            if (parms->u.headerRx.totalLen == 0) {
                /* Empty Name header */
                server->currOp.info.setfolder.reset = TRUE;
            } else {
                server->currOp.info.setfolder.reset = FALSE;
            }

            DoUniHeaderCopy(server->currOp.info.setfolder.name, 
                            &server->currOp.info.setfolder.nameLen, GOEP_MAX_UNICODE_LEN,
                            parms->u.headerRx.buff, parms->u.headerRx.currLen);                
            return;
        }
        break;

#if OBEX_ACTION_SUPPORT == XA_ENABLED
    case GOEP_OPER_ACTION:
        switch (parms->u.headerRx.type) {
        case OBEXH_NAME:
            DoUniHeaderCopy(server->currOp.info.action.name, 
                            &server->currOp.info.action.nameLen, GOEP_MAX_UNICODE_LEN, 
                            parms->u.headerRx.buff, parms->u.headerRx.currLen);                
            return;

        case OBEXH_DEST_NAME:
            DoUniHeaderCopy(server->currOp.info.action.destName, 
                            &server->currOp.info.action.destNameLen, GOEP_MAX_UNICODE_LEN, 
                            parms->u.headerRx.buff, parms->u.headerRx.currLen);                
            return;

        case OBEXH_PERMISSIONS:
            server->currOp.info.action.perms = parms->u.headerRx.value;
            break;

        case OBEXH_ACTION_ID:
            server->currOp.info.action.actionId = (U8)parms->u.headerRx.value;
            break;
        }
        break;
#endif /* OBEX_ACTION_SUPPORT == XA_ENABLED */

    case GOEP_OPER_DISCONNECT:
        break;
    }

#if OBEX_AUTHENTICATION == XA_ENABLED
    /* We accept authentication headers with any operation. */
    switch (parms->u.headerRx.type) {
    case OBEXH_AUTH_CHAL:
        if (OBEXH_ParseAuthChallenge(&parms->server->handle, 
                                     &server->currOp.challenge)) {
            /* Full auth challenge has been received, indicate it. */
            server->flags |= GOEF_CHALLENGE;
            NotifyCurrServer(parms->server, GOEP_EVENT_AUTH_CHALLENGE);
        }
        return;

    case OBEXH_AUTH_RESP:
        if (OBEXH_ParseAuthResponse(&parms->server->handle, 
                                    &server->currOp.response)) {
            /* Full auth response has been received, indicate it. */
            server->flags |= GOEF_RESPONSE;
            NotifyCurrServer(parms->server, GOEP_EVENT_AUTH_RESPONSE);
        }
        return;
    }
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */

    if (parms->u.headerRx.type == OBEXH_TARGET ||
        parms->u.headerRx.type == OBEXH_CONNID) {
        /* We do not pass up Target or ConnId headers, in order 
         * for Directed connections and operations to setup properly.
         */
        return;
    }

#if OBEX_SRM_MODE == XA_ENABLED
    if (parms->u.headerRx.type == OBEXH_SRM_MODE) {
        if (parms->u.headerRx.value == 0x01) {   /* ENABLE */
            /* Responding to the SRM enable request from the Client 
             * occurs within OBEX, so we will merely enable SRM locally.
             */
            if ((server->currOp.oper == GOEP_OPER_CONNECT) || (server->tpType == OBEX_TP_BLUETOOTH)) {
                /* GOEP does not allow SRM enabled in connections, nor it is allowed for RFCOMM
                 * transport connections. Reject this. */
                (void)OBEX_ServerAbort(&server->obs, OBRC_FORBIDDEN, TRUE);
                return;
            } else {
                /* SRM active for the operation */
                server->flags |= GOEF_SRM_ACTIVE;
                server->currOp.srm.enabled = TRUE;
            }
        }
    }

    if (parms->u.headerRx.type == OBEXH_SRM_PARAMS) {
        server->currOp.srm.rcvdParam = (U8)parms->u.headerRx.value;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

    /* If we have not processed the OBEX header already we will accept it now, regardless
     * the current operation. 
     */
    server->currOp.header.type = parms->u.headerRx.type;
    if (OBEXH_Is1Byte(server->currOp.header.type) || 
        OBEXH_Is4Byte(server->currOp.header.type)) {
        /* 1-byte or 4-byte*/
        server->currOp.header.value = parms->u.headerRx.value;
    } else if (OBEXH_IsUnicode(server->currOp.header.type)) {
        /* Unicode */
        DoUniHeaderCopy(server->currOp.header.unicodeBuffer, &server->currOp.header.len, 
                        GOEP_MAX_UNICODE_LEN, parms->u.headerRx.buff, parms->u.headerRx.currLen);                
        /* Don't indicate the Unicode header until it is completely copied */
        if (parms->u.headerRx.remainLen > 0) return;
        /* Pass up the total length */
        server->currOp.header.totalLen = server->currOp.header.len;
        server->currOp.header.remainLen = 0;
        /* Point the header buffer pointer to the unicode buffer */
        server->currOp.header.buffer = (U8 *)server->currOp.header.unicodeBuffer;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED
        Assert(server->currOp.header.totalLen == parms->u.headerRx.totalLen);
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED */
    } else {
        /* Byte Sequence */
        Assert(OBEXH_IsByteSeq(server->currOp.header.type));
        server->currOp.header.totalLen = parms->u.headerRx.totalLen;
        server->currOp.header.remainLen = parms->u.headerRx.remainLen;
        server->currOp.header.len = parms->u.headerRx.currLen;
        server->currOp.header.buffer = parms->u.headerRx.buff;
    }
    NotifyCurrServer(parms->server, GOEP_EVENT_HEADER_RX);

    /* Clear out the header information in preparation for a new header */
    OS_MemSet((U8 *)&server->currOp.header, 0, sizeof(server->currOp.header));
}

/*---------------------------------------------------------------------------
 *            AssociateServer
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Associate a specific profile server based on the target header
 *            for this particular Obex Server.
 *
 * Return:    void
 *
 */
static void AssociateServer(ObexServerApp *serverApp)
{
#if OBEX_SERVER_CONS_SIZE > 0
    U8 i,j, numProfiles;
#endif /* OBEX_SERVER_CONS_SIZE > 0 */
    GoepServerObexCons *server = (GoepServerObexCons*)serverApp;

    Assert(server->connId < GOEP_NUM_OBEX_CONS);

    /* Try to associate the operation with a server */
#if OBEX_SERVER_CONS_SIZE > 0
    numProfiles = 0;

    for (i = 0; i < GOEP_MAX_PROFILES; i++) {
        if (server->profiles[i]) {
            /* Track the number of registered profiles */
            numProfiles++;

            Assert(server->profiles[i]->numTargets <= OBEX_SERVER_CONS_SIZE);
            for (j = 0; j < server->profiles[i]->numTargets; j++) {
                if (server->profiles[i]->target[j] == OBEX_ServerGetConnInfo(serverApp)) {
                    /* We have found a matching target header for the active connection
                     * ID of the current operation.
                     */
                    server->currOp.handler = server->profiles[i];
                    return;
                }
            }
            if (!server->profiles[i]->numTargets && !OBEX_ServerGetConnInfo(serverApp)) {
                /* We do not have active connection ID for this operation and this 
                 * profile has no target headers; therefore, set this profile as our handler.
                 */
                server->currOp.handler = server->profiles[i];
                return;
            }
        }
    }

    /* If the operation is for an already connected service, but we can't find a service match, 
     * check if only one service is registered. If so, assume this was the intended service, 
     * but somehow the remote device failed to issue a Connecton ID header.
     */
    if ((numProfiles == 1) && (server->currOp.oper != GOEP_OPER_CONNECT)) {
        for (i = 0; i < GOEP_MAX_PROFILES; i++) {
            if (server->profiles[i]) {
                server->currOp.handler = server->profiles[i];
                break;
            }
        }
    }
#else /* OBEX_SERVER_CONS_SIZE > 0 */
    /* Assign Object Push Server */
    server->currOp.handler = server->profiles[GOEP_PROFILE_OPUSH];
#endif /* OBEX_SERVER_CONS_SIZE > 0 */
}

/*---------------------------------------------------------------------------
 *            StartServerOperation
 *---------------------------------------------------------------------------
 *
 */
static void StartServerOperation(ObexServerApp *serverApp, GoepOperation Op)
{
#if OBEX_SESSION_SUPPORT == XA_ENABLED
    GoepSessionOperation    sessionOpcode = 0;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

    GoepServerObexCons *server = (GoepServerObexCons*)serverApp;

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    if (Op == GOEP_OPER_SESSION) {
        /* Save the session opcode before clearing the operation structure */
        sessionOpcode = server->currOp.session.sessionOpcode;
    }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

    OS_MemSet((U8 *)&server->currOp, 0, sizeof(server->currOp));

    /* Set event to NONE so we can track when the START event is delivered. */
    if (Op == GOEP_OPER_ABORT) {
        /* We will generate the GOEP_EVENT_START automatically when 
         * OBSE_ABORT_START is received, so there is no need to set 
         * GOEP_EVENT_NONE to generate it again.
         */
        server->currOp.event = GOEP_EVENT_START;
    } else {
        server->currOp.event = GOEP_EVENT_NONE;
    }
    server->currOp.oper = Op;

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    if (Op == GOEP_OPER_SESSION) {
        /* Restore the session opcode */
        server->currOp.session.sessionOpcode = sessionOpcode;
    }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

    /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
    server->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
}

#if OBEX_SESSION_SUPPORT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            ResumeServerOperation
 *---------------------------------------------------------------------------
 *
 */
static void ResumeServerOperation(ObexServerApp *serverApp, ObexOpcode Op)
{
    GoepServerObexCons *server = (GoepServerObexCons*)serverApp;

    OS_MemSet((U8 *)&server->currOp, 0, sizeof(server->currOp));

    switch (Op) {
    case OB_OPCODE_CONNECT:
        server->currOp.oper = GOEP_OPER_CONNECT;
        break;
    case OB_OPCODE_DISCONNECT:
        server->currOp.oper = GOEP_OPER_DISCONNECT;
        break;
    case OB_OPCODE_PUT:
        server->currOp.oper = GOEP_OPER_PUSH;
        break;
    case OB_OPCODE_GET:
        server->currOp.oper = GOEP_OPER_PULL;
        break;
    case OB_OPCODE_SET_PATH:
        server->currOp.oper = GOEP_OPER_SETFOLDER;
        break;
    case OB_OPCODE_ABORT:
        server->currOp.oper = GOEP_OPER_ABORT;
        break;
#if OBEX_ACTION_SUPPORT == XA_ENABLED
    case OB_OPCODE_ACTION:
        server->currOp.oper = GOEP_OPER_ACTION;
        break;
#endif /* OBEX_ACTION_SUPPORT == XA_ENABLED */
    }

    /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
    server->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
}
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            NotifyAllServers
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Notify all profile servers for this Obex Server of the
 *            GOEP event.
 *
 * Return:    void
 *
 */
static void NotifyAllServers(ObexServerApp *serverApp, GoepEventType Event)
{
    U8  i;
    GoepServerObexCons *server = (GoepServerObexCons*)serverApp;

    Assert(server->connId < GOEP_NUM_OBEX_CONS);

    server->currOp.event = Event; 
    for (i = 0; i < GOEP_MAX_PROFILES; i++) {
        if (server->profiles[i]) {
            server->currOp.handler = server->profiles[i];

#if BT_SECURITY == XA_ENABLED
            if (Event == GOEP_EVENT_TP_DISCONNECTED) {
                /* The transport was disconnected, so we should clear each service's
                 * authorized flag
                 */
                server->profiles[i]->authorized = FALSE;
            }
#endif /* BT_SECURITY == XA_ENABLED */

            server->profiles[i]->appParent(&server->currOp);
        }
    }
}

/*---------------------------------------------------------------------------
 *            NotifyCurrServer
 *---------------------------------------------------------------------------
 *
 */
static void NotifyCurrServer(ObexServerApp *serverApp, GoepEventType Event)
{
    ObStatus    status;
    GoepServerObexCons *server = (GoepServerObexCons*)serverApp;

    if (server->currOp.oper == GOEP_OPER_PUSH || 
        server->currOp.oper == GOEP_OPER_PULL) {
        /* Indicate the status of the final bit in the current OBEX packet */
        server->currOp.info.pushpull.finalBit = 
            ((serverApp->handle.parser.opcode & 0x80) ? TRUE : FALSE);
    }

    /* If this is the first event for this operation, find the correct
     * server and make sure we always indicate a START event first.
     */
    if ((Event != GOEP_EVENT_DISCOVERY_FAILED) && 
#ifdef QNX_BLUETOOTH_ODR_PROTOCOL_COLLISION
        (Event != GOEP_EVENT_PROTOCOL_COLLISION) &&
#endif
        (Event != GOEP_EVENT_NO_SERVICE_FOUND) &&
        (server->currOp.event == GOEP_EVENT_NONE)) {
        Assert(server->currOp.handler == 0);
        AssociateServer(serverApp);

        /* Be sure to indicate a start event before any other event. */
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        /* Don't indicate a start event again for a resumed operation */
        if ((Event != GOEP_EVENT_START) &&
            (Event != GOEP_EVENT_RESUME_OPER)) {
#else /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        if (Event != GOEP_EVENT_START) {
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
            if (server->currOp.handler) {
                server->currOp.event = GOEP_EVENT_START;
                server->currOp.handler->appParent(&server->currOp);
            }
        }
    }

    server->currOp.event = Event;

    /* If we found a server, indicate the event now. */
    if (server->currOp.handler) {
        server->currOp.handler->appParent(&server->currOp);
        return;
    }

    /* We did not find a suitable server, handle operation rejection. */
    if (Event == GOEP_EVENT_CONTINUE) {
        status = OBEX_ServerAbort(serverApp, OBRC_SERVICE_UNAVAILABLE, FALSE);
        if (status != OB_STATUS_SUCCESS) {
            Report(("GOEP: Failed call to OBEX_ServerAbort: %d", status));
        }
        OBEX_ServerSendResponse(&server->obs);
        return;
    }
}
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

#if OBEX_ROLE_CLIENT == XA_ENABLED

#if OBEX_SESSION_SUPPORT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            ResumeClientOperation
 *---------------------------------------------------------------------------
 *
 */
static void ResumeClientOperation(ObexClientApp *clientApp, ObexOpcode Op)
{
    GoepClientObexCons *client = (GoepClientObexCons*)clientApp;

    OS_MemSet((U8 *)&client->currOp, 0, sizeof(client->currOp));

    switch (Op) {
    case OB_OPCODE_CONNECT:
        client->currOp.oper = GOEP_OPER_CONNECT;
        break;
    case OB_OPCODE_DISCONNECT:
        client->currOp.oper = GOEP_OPER_DISCONNECT;
        break;
    case OB_OPCODE_PUT:
        client->currOp.oper = GOEP_OPER_PUSH;
        break;
    case OB_OPCODE_GET:
        client->currOp.oper = GOEP_OPER_PULL;
        break;
    case OB_OPCODE_SET_PATH:
        client->currOp.oper = GOEP_OPER_SETFOLDER;
        break;
    case OB_OPCODE_ABORT:
        client->currOp.oper = GOEP_OPER_ABORT;
        break;
#if OBEX_ACTION_SUPPORT == XA_ENABLED
    case OB_OPCODE_ACTION:
        client->currOp.oper = GOEP_OPER_ACTION;
        break;
#endif /* OBEX_ACTION_SUPPORT == XA_ENABLED */
    }

    /* Clear authentication flags */
#if OBEX_AUTHENTICATION == XA_ENABLED
    client->flags &= ~(GOEF_CHALLENGE|GOEF_RESPONSE);
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
}
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GoepClntCallback
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function processes OBEX client protocol events.
 *
 * Return:    void
 *
 */
static void GoepClntCallback(ObexClientCallbackParms *parms)
{
#if OBEX_SESSION_SUPPORT == XA_ENABLED
    ObStatus            status;
    U8                  sessionId[16];
    GoepConnectReq     *connReq = 0;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
#if (OBEX_SESSION_SUPPORT == XA_ENABLED) || ((BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED))
    GoepClientApp      *cApp = 0;
#endif /* (OBEX_SESSION_SUPPORT == XA_ENABLED) || ((BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED)) */
    ObexTpConnInfo      tpInfo;
    GoepClientObexCons *client = (GoepClientObexCons*)parms->client;

    Report(("GOEP: Client Callback - OBEX Event: %d\n", parms->event));

    Assert(IsObexLocked());
    switch (parms->event) {
    case OBCE_SEND_COMMAND:
        Assert(client->currOp.event != GOEP_EVENT_NONE);
        NotifyCurrClient(parms->client, GOEP_EVENT_CONTINUE);
        break;

    case OBCE_CONNECTED:
        /* A Transport Layer Connection has been established. */
        Assert((client->flags & GOEF_ACTIVE) == 0);
        client->flags |= GOEF_ACTIVE;

        /* Grab the active transport type to determine whether Session + SRM are allowed */
        AssertEval(OBEX_GetTpConnInfo(&client->obc.handle, &tpInfo));
        client->tpType = tpInfo.tpType;

        NotifyAllClients(parms->client, GOEP_EVENT_TP_CONNECTED);
        break;

    case OBCE_DISCONNECT:
#if BT_SECURITY == XA_ENABLED
        if (client->flags & GOEF_SEC_ACCESS_REQUEST) {
            AssertEval(SEC_CancelAccessRequest(&(client->currOp.handler->secToken)) == BT_STATUS_SUCCESS);
            client->flags &= ~GOEF_SEC_ACCESS_REQUEST;
        }
#endif /* BT_SECURITY == XA_ENABLED */

        client->tpType = OBEX_TP_NONE;

        client->flags &= ~GOEF_ACTIVE;
        client->flags &= ~(GOEF_CLIENT_BT_TP_INITIATED|GOEF_CLIENT_L2BT_TP_INITIATED);
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        /* Session is not active */
        client->flags &= ~GOEF_SESSION_ACTIVE;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        client->currOp.discReason = parms->u.discReason;
        NotifyAllClients(parms->client, GOEP_EVENT_TP_DISCONNECTED);
        break;
        
    case OBCE_COMPLETE:
        if (client->flags & GOEF_CONNECT_ISSUED) {
            /* Connect completed */
            client->flags &= ~GOEF_CONNECT_ISSUED;
        }

#if OBEX_SRM_MODE == XA_ENABLED
        /* If the Server did not respond to our SRM enable request, then 
         * we will clear the flag and assume SRM is not supported.
         */
        if (client->flags & GOEF_SRM_REQUESTED) {
            client->flags &= ~GOEF_SRM_REQUESTED;
        }

        /* If SRM was enabled only for this operation, then it will be disabled
         * by OBEX automatically.
         */
        if (client->flags & GOEF_SRM_ACTIVE) {
            client->flags &= ~GOEF_SRM_ACTIVE;
        }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if OBEX_SESSION_SUPPORT == XA_ENABLED
        if (client->currOp.oper == GOEP_OPER_SESSION) {
            if (client->flags & GOEF_CREATE_ISSUED) {
                /* Store the parameters for the GOEP Connect */
                cApp = client->currOp.handler;
                connReq = client->currOp.handler->connect;

                /* Clear the session to avoid sending the Create Session again */
                connReq->session = 0;
                /* Clear the operation handler to avoid reporting BUSY */
                client->currOp.handler = 0;

                /* Connect GOEP */
                (void)GOEP_Connect(cApp, connReq);
                
                /* Restore the previous handler and operation */
                client->currOp.handler = cApp;
                client->currOp.oper = GOEP_OPER_SESSION;

                /* Create Session completed */
                client->flags &= ~GOEF_CREATE_ISSUED;
                /* Active Session exists */
                client->flags |= GOEF_SESSION_ACTIVE;
            } else if (client->flags & GOEF_RESUME_ISSUED) {
                /* Resume Session completed */
                client->flags &= ~GOEF_RESUME_ISSUED;
                /* Active Session exists */
                client->flags |= GOEF_SESSION_ACTIVE;
                /* Restore the OBEX Connection Id for this session */
                cApp = client->currOp.handler;
                cApp->obexConnId = cApp->savedObexConnId;
            } else if (client->flags & GOEF_CLOSE_ISSUED) {
                /* Active Session closed */
                client->flags &= ~(GOEF_CLOSE_ISSUED|GOEF_SESSION_ACTIVE);
            }

            if (client->currOp.session.sessionOpcode == GOEP_OPER_SUSPEND_SESSION) {
                /* Reliable Session has been gracefully suspended */
                client->flags &= ~GOEF_SESSION_ACTIVE;
            }
        }

        if ((client->currOp.oper == GOEP_OPER_DISCONNECT) && (client->flags & GOEF_SESSION_ACTIVE)) {
            /* Store the parameters for the Close Session */
            cApp = client->currOp.handler;

            if (cApp && cApp->session) {
                /* Create the Session Id for the reliable session */
                status = OBEX_CreateSessionId(cApp->session, sessionId);
                if (status == OB_STATUS_SUCCESS) {
                    /* Close the reliable session */
                    status = OBEX_ClientCloseSession(&client->obc, sessionId);
                    if (status == OB_STATUS_PENDING) {
                        client->flags |= GOEF_CLOSE_ISSUED;
                    }
                }
            }
        }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

        /* The requested operation has completed. */
        client->currOp.reason = OBRC_SUCCESS;
        NotifyCurrClient(parms->client, GOEP_EVENT_COMPLETE);

#if OBEX_SESSION_SUPPORT == XA_ENABLED
        if (client->flags & GOEF_CLOSE_ISSUED) {
            /* Restore the current operation, since we started the Close Session prior
             * to the COMPLETE event indication.
             */
            client->currOp.handler = cApp;
            client->currOp.oper = GOEP_OPER_SESSION;
            client->currOp.session.sessionOpcode = GOEP_OPER_CLOSE_SESSION;
        }

        if ((client->flags & GOEF_CONNECT_ISSUED) && cApp) {
            /* Restore the current operation, since we started the Connect prior
             * to the COMPLETE event indication.
             */
            client->currOp.handler = cApp;
            client->currOp.oper = GOEP_OPER_CONNECT;
        }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        break;
        
    case OBCE_ABORTED:
#if BT_SECURITY == XA_ENABLED
        if (client->flags & GOEF_SEC_ACCESS_REQUEST) {
            AssertEval(SEC_CancelAccessRequest(&(client->currOp.handler->secToken)) == BT_STATUS_SUCCESS);
            client->flags &= ~GOEF_SEC_ACCESS_REQUEST;

            /* Clear the authorized flag, since security did not complete */
            client->currOp.handler->authorized = FALSE;
        }
#endif /* BT_SECURITY == XA_ENABLED */

        /* Clear a connection request if it existed */
        client->flags &= ~(GOEF_CONNECT_ISSUED);

#if OBEX_SESSION_SUPPORT == XA_ENABLED
        /* Clear any related session flags */
        client->flags &= ~(GOEF_CREATE_ISSUED|GOEF_RESUME_ISSUED|GOEF_CLOSE_ISSUED);
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

        /* The requested operation was aborted. */
        client->currOp.reason = parms->u.abortReason;
        NotifyCurrClient(parms->client, GOEP_EVENT_ABORTED);
        break;
        
#ifdef QNX_BLUETOOTH_ODR_PROTOCOL_COLLISION
    case OBCE_PROTOCOL_COLLISION:
        Assert((client->flags & GOEF_ACTIVE) == 0);
#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED)
        if ((client->flags & GOEF_CLIENT_L2BT_TP_INITIATED) && (client->flags & GOEF_CLIENT_BT_TP_INITIATED)) {
            /* OBEX/L2CAP connection attempt failed; try OBEX/RFCOMM automatically */
            cApp = client->currOp.handler;
            cApp->targetConn->type = OBEX_TP_BLUETOOTH;
            cApp->targetConn->proto.bt.addr = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.rm->bdAddr;
            cApp->targetConn->proto.bt.sdpQuery = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.parms;
            cApp->targetConn->proto.bt.sdpQueryLen = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.plen;
            cApp->targetConn->proto.bt.sdpQueryType = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.type;
            client->flags &= ~(GOEF_CLIENT_BT_TP_INITIATED|GOEF_CLIENT_L2BT_TP_INITIATED);
            client->currOp.handler = 0; /* Clear the current handler for the connection attempt */
            (void)GOEP_TpConnect(cApp, cApp->targetConn);
        } else 
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) */
        {
            client->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            NotifyCurrClient(parms->client, GOEP_EVENT_PROTOCOL_COLLISION);
        }
        break;
#endif

    case OBCE_NO_SERVICE_FOUND:
        Assert((client->flags & GOEF_ACTIVE) == 0);
#if (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED)
        if ((client->flags & GOEF_CLIENT_L2BT_TP_INITIATED) && (client->flags & GOEF_CLIENT_BT_TP_INITIATED)) {
            /* OBEX/L2CAP connection attempt failed; try OBEX/RFCOMM automatically */
            cApp = client->currOp.handler;
            cApp->targetConn->type = OBEX_TP_BLUETOOTH;
            cApp->targetConn->proto.bt.addr = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.rm->bdAddr;
            cApp->targetConn->proto.bt.sdpQuery = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.parms;
            cApp->targetConn->proto.bt.sdpQueryLen = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.plen;
            cApp->targetConn->proto.bt.sdpQueryType = parms->client->trans.ObexClientL2BtTrans.sdpQueryToken.type;
            client->flags &= ~(GOEF_CLIENT_BT_TP_INITIATED|GOEF_CLIENT_L2BT_TP_INITIATED);
            client->currOp.handler = 0; /* Clear the current handler for the connection attempt */
            (void)GOEP_TpConnect(cApp, cApp->targetConn);
        } else 
#endif /* (BT_STACK == XA_ENABLED) && (OBEX_RFCOMM_TRANSPORT == XA_ENABLED) && (OBEX_L2CAP_TRANSPORT == XA_ENABLED) */
        {
            client->flags &= ~(GOEF_SERVER_BT_TP_INITIATED|GOEF_SERVER_L2BT_TP_INITIATED);
            NotifyCurrClient(parms->client, GOEP_EVENT_NO_SERVICE_FOUND);
        }
        break;

    case OBCE_DISCOVERY_FAILED:
        Assert((client->flags & GOEF_ACTIVE) == 0);
        client->flags &= ~(GOEF_CLIENT_BT_TP_INITIATED|GOEF_CLIENT_L2BT_TP_INITIATED);
        NotifyCurrClient(parms->client, GOEP_EVENT_DISCOVERY_FAILED);
        break;

    case OBCE_HEADER_RX:
        GoepClientProcessHeaders(parms);
        break;

#if OBEX_SESSION_SUPPORT == XA_ENABLED
    case OBCE_SESSION_SUSPENDED:
        /* Session is not active */
        client->flags &= ~GOEF_SESSION_ACTIVE;
        client->currOp.session.session = parms->u.session;
        /* Save the OBEX Connection Id for this session */
        cApp = client->currOp.handler;
        if (cApp) {
            cApp->savedObexConnId = cApp->obexConnId;
            NotifyCurrClient(parms->client, GOEP_EVENT_SESSION_SUSPENDED);
        } else {
            NotifyAllClients(parms->client, GOEP_EVENT_SESSION_SUSPENDED);
        }
        break;

    case OBCE_SESSION_PARMS_RX:
        client->currOp.session.sessionParms = parms->u.sessionParms;
        NotifyCurrClient(parms->client, GOEP_EVENT_SESSION_PARMS_RX);
        break;

    case OBCE_SUSPENDED:
        /* Session is not active */
        client->flags &= ~GOEF_SESSION_ACTIVE;
        client->currOp.session.suspended.headerBuff = parms->u.suspended.headerBuff;
        client->currOp.session.suspended.headerLen = parms->u.suspended.headerLen;
        client->currOp.session.suspended.protoBuff = parms->u.suspended.protoBuff;
        client->currOp.session.suspended.protoLen = parms->u.suspended.protoLen;
        client->currOp.session.suspended.session = parms->u.suspended.session;
        client->currOp.session.suspended.obsh = parms->u.suspended.obsh;
        /* Save the OBEX Connection Id for this session */
        cApp = client->currOp.handler;
        if (cApp) {
            cApp->savedObexConnId = cApp->obexConnId;
        }
        NotifyCurrClient(parms->client, GOEP_EVENT_SUSPENDED);
        break;

    case OBCE_SESSION_ERROR:
        /* If we have already cleared the operation handler by now, then skip this */
        if (!client->currOp.handler) break;

        client->currOp.session.session = parms->u.session;
        NotifyCurrClient(parms->client, GOEP_EVENT_SESSION_ERROR);
        break;

    case OBCE_RESUME_OPER:
        /* Restore the operation being resumed */
        ResumeClientOperation(parms->client, parms->opcode);

        /* Restore the handler */
        client->currOp.handler = client->resumeHandler;
        client->resumeHandler = 0;

        NotifyCurrClient(parms->client, GOEP_EVENT_RESUME_OPER);
        break;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */

    default:
        Assert(0);
        break;
    }
}

/*---------------------------------------------------------------------------
 *            GoepClientProcessHeaders
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Process the incoming headers based on the current operation.
 *
 * Return:    void
 *
 */
void GoepClientProcessHeaders(ObexClientCallbackParms *parms)
{
    GoepClientObexCons *client = (GoepClientObexCons*)parms->client;

    switch (client->currOp.oper) {

    case GOEP_OPER_CONNECT:
#if GOEP_MAX_WHO_LEN > 0
        if (parms->u.headerRx.type == OBEXH_WHO) {
            DoHeaderCopy(client->currOp.info.connect.who, 
                         &client->currOp.info.connect.whoLen, GOEP_MAX_WHO_LEN, 
                         parms->u.headerRx.buff, parms->u.headerRx.currLen);
            return;
        }
#endif /* GOEP_MAX_WHO_LEN > 0 */
        if (parms->u.headerRx.type == OBEXH_CONNID) {
            client->currOp.handler->obexConnId = parms->u.headerRx.value;
            return;
        }
        break;
       
    case GOEP_OPER_PULL:
        if (parms->u.headerRx.type == OBEXH_LENGTH) {
            /* Set the incoming object length so the progress meter works. */
            client->currOp.info.pull.objectLen = parms->u.headerRx.value;
        }
        break;
    }

#if OBEX_AUTHENTICATION == XA_ENABLED
    /* We accept authentication headers with any operation */
    switch (parms->u.headerRx.type) {
    case OBEXH_AUTH_CHAL:
        if (OBEXH_ParseAuthChallenge(&parms->client->handle, 
                                     &client->currOp.challenge)) {
            /* Full auth challenge has been received, indicate it. */
            client->flags |= GOEF_CHALLENGE;
            NotifyCurrClient(parms->client, GOEP_EVENT_AUTH_CHALLENGE);
        }
        return;

    case OBEXH_AUTH_RESP:
        if (OBEXH_ParseAuthResponse(&parms->client->handle, 
                                    &client->currOp.response)) {
            /* Full auth response has been received, indicate it. */
            client->flags |= GOEF_RESPONSE;
            NotifyCurrClient(parms->client, GOEP_EVENT_AUTH_RESPONSE);
        }
        return;
    }
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */

    if (parms->u.headerRx.type == OBEXH_TARGET ||
        parms->u.headerRx.type == OBEXH_CONNID) {
        /* We do not pass up Target or ConnId headers, in order 
         * for Directed connections and operations to setup properly.
         */
        return;
    }

#if OBEX_SRM_MODE == XA_ENABLED
    if ((client->currOp.oper != GOEP_OPER_DISCONNECT) &&
        (parms->u.headerRx.type == OBEXH_SRM_MODE)) {
        if (parms->u.headerRx.value == 0x01) {   /* ENABLE */
            if (client->flags & GOEF_SRM_REQUESTED) {
                /* SRM enabled for the operation */
                client->flags |= GOEF_SRM_ACTIVE;

                client->flags &= ~GOEF_SRM_REQUESTED;
                client->currOp.srm.enabled = TRUE;
            } else {
                /* GOEP does not allow the Server to initiate SRM.  Ignore this. */
                (void)OBEX_ClientTpDisconnect(&client->obc, TRUE);
            }
        } else if (parms->u.headerRx.value == 0x02) {   /* SRM SUPPORTED */
            /* Informative only */
            client->currOp.srm.avail = TRUE;
        }
    }

    if (parms->u.headerRx.type == OBEXH_SRM_PARAMS) {
        client->currOp.srm.rcvdParam = (U8)parms->u.headerRx.value;
    }
#endif /* OBEX_SRM_MODE == XA_ENABLED */

     /* If we have not processed the OBEX header already we will accept it now, regardless
     * the current operation. 
     */
    client->currOp.header.type = parms->u.headerRx.type;
    if (OBEXH_Is1Byte(client->currOp.header.type) || 
        OBEXH_Is4Byte(client->currOp.header.type)) {
        /* 1-byte or 4-byte */
        client->currOp.header.value = parms->u.headerRx.value;
    } else if (OBEXH_IsUnicode(client->currOp.header.type)) {
        /* Unicode - also sets the length */
        DoUniHeaderCopy(client->currOp.header.unicodeBuffer, &client->currOp.header.len,
                        GOEP_MAX_UNICODE_LEN, parms->u.headerRx.buff, parms->u.headerRx.currLen);                
        /* Don't indicate the Unicode header until it is completely copied */
        if (parms->u.headerRx.remainLen > 0) return;

        /* Pass up the total length */
        client->currOp.header.totalLen = client->currOp.header.len;
        client->currOp.header.remainLen = 0;
        /* Point the header buffer pointer to the unicode buffer */
        client->currOp.header.buffer = (U8 *)client->currOp.header.unicodeBuffer;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED
        Assert(client->currOp.header.totalLen == parms->u.headerRx.totalLen);
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_DISABLED */
    } else {
        /* Byte Sequence */
        Assert(OBEXH_IsByteSeq(client->currOp.header.type));
        client->currOp.header.totalLen = parms->u.headerRx.totalLen;
        client->currOp.header.remainLen = parms->u.headerRx.remainLen;
        client->currOp.header.len = parms->u.headerRx.currLen;
        client->currOp.header.buffer = parms->u.headerRx.buff;
    }
    NotifyCurrClient(parms->client, GOEP_EVENT_HEADER_RX);

    /* Clear out the header information in preparation for a new header */
    OS_MemSet((U8 *)&client->currOp.header, 0, sizeof(client->currOp.header));
}

/*---------------------------------------------------------------------------
 *            NotifyAllClients
 *---------------------------------------------------------------------------
 *
 */
static void NotifyAllClients(ObexClientApp *clientApp, GoepEventType Event)
{
    U8                  i;
    GoepClientEvent     tempOp;
    GoepClientObexCons  *client = (GoepClientObexCons*)clientApp;

    Assert(client->connId < GOEP_NUM_OBEX_CONS);

    client->currOp.event = Event;
    client->currOp.handler = 0;
#if OBEX_SESSION_SUPPORT == XA_ENABLED
    client->resumeHandler = 0;
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
    client->currOp.oper = GOEP_OPER_NONE;

    tempOp = client->currOp;

    for (i = 0; i < GOEP_MAX_PROFILES; i++) {
        if (client->profiles[i]) {
            client->profiles[i]->obexConnId = OBEX_INVALID_CONNID;
            tempOp.handler = client->profiles[i];

            if ((Event == GOEP_EVENT_TP_DISCONNECTED) &&
                ((client->profiles[i]->connState == CS_CONNECTED) || 
                 (client->profiles[i]->connState == CS_CONNECTING))) {

#if BT_SECURITY == XA_ENABLED
                /* The transport was disconnected, so we should clear each service's
                 * authorized flag 
                 */
                client->profiles[i]->authorized = FALSE;
#endif /* BT_SECURITY == XA_ENABLED */

                /* Deliver disconnect event */
                client->profiles[i]->connState = CS_DISCONNECTED;
                client->profiles[i]->appParent(&tempOp);
            }
            else if ((Event == GOEP_EVENT_TP_CONNECTED) && 
#if OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED
                     ((client->profiles[i]->connState == CS_CONNECTING) ||
                      (client->profiles[i]->connState == CS_DISCONNECTED))) {
#else /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */
                      (client->profiles[i]->connState == CS_CONNECTING)) {
#endif /* OBEX_ALLOW_SERVER_TP_CONNECT == XA_ENABLED */
                /* Deliver connect event */
                client->profiles[i]->connState = CS_CONNECTED;
                client->profiles[i]->appParent(&tempOp);
            }
#if OBEX_SESSION_SUPPORT == XA_ENABLED
            else if (Event == GOEP_EVENT_SESSION_SUSPENDED) {
                /* Deliver suspended event */
                client->profiles[i]->appParent(&tempOp);
            }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        }
    }
}

/*---------------------------------------------------------------------------
 *            NotifyCurrClient
 *---------------------------------------------------------------------------
 *
 */
static void NotifyCurrClient(ObexClientApp *clientApp, GoepEventType Event)
{
    GoepClientEvent  tempOp;
    GoepClientObexCons  *client = (GoepClientObexCons*)clientApp;

    if (((client->flags & GOEF_ACTIVE) == 0) && (!client->currOp.handler)) {
        /* Transport is disconnected and no handler exists */
        return;
    } 
#ifdef QNX_BLUETOOTH_MAP_TEMPFIX_MNC_DISCONNECT
    if ( NULL == client->currOp.handler ) {
        Report(( "PR227205" ));
        return;
    }
#endif

    Assert(client->currOp.handler);

    client->currOp.event = Event;
    tempOp = client->currOp;

    if (Event >= GOEP_EVENT_COMPLETE) {
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        Assert((Event == GOEP_EVENT_TP_CONNECTED) || 
               (Event == GOEP_EVENT_TP_DISCONNECTED) || 
               (Event == GOEP_EVENT_ABORTED) || 
               (Event == GOEP_EVENT_DISCOVERY_FAILED) ||
               (Event == GOEP_EVENT_NO_SERVICE_FOUND) ||
#ifdef QNX_BLUETOOTH_ODR_PROTOCOL_COLLISION
               (Event == GOEP_EVENT_PROTOCOL_COLLISION) ||
#endif
               (Event == GOEP_EVENT_SUSPENDED) ||
               (Event == GOEP_EVENT_COMPLETE));
#else /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        Assert((Event == GOEP_EVENT_TP_CONNECTED) || 
               (Event == GOEP_EVENT_TP_DISCONNECTED) || 
               (Event == GOEP_EVENT_ABORTED) || 
               (Event == GOEP_EVENT_DISCOVERY_FAILED) ||
               (Event == GOEP_EVENT_NO_SERVICE_FOUND) ||
#ifdef QNX_BLUETOOTH_ODR_PROTOCOL_COLLISION
               (Event == GOEP_EVENT_PROTOCOL_COLLISION) ||
#endif
               (Event == GOEP_EVENT_COMPLETE));
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
        if ((Event == GOEP_EVENT_DISCOVERY_FAILED) || 
#ifdef QNX_BLUETOOTH_ODR_PROTOCOL_COLLISION
            (Event == GOEP_EVENT_PROTOCOL_COLLISION) ||
#endif
            (Event == GOEP_EVENT_NO_SERVICE_FOUND)) {
            /* Reset the connection state for this profile */
            client->currOp.handler->connState = CS_DISCONNECTED;
        }
        /* Were done, remove the handler before indicating the event. */
        client->currOp.handler = 0;
#if OBEX_SESSION_SUPPORT == XA_ENABLED
        if (client->currOp.oper != GOEP_OPER_SESSION) {
            client->resumeHandler = 0;
        }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
    }

    /* Indicate the event */
    tempOp.event = Event;
    tempOp.handler->appParent(&tempOp);
}

#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#define LEN_UNICODE_ODD_FLAG    0x8000
#define LEN_UNICODE_MASK        0x7FFF
/*---------------------------------------------------------------------------
 *            DoUniHeaderCopy
 *---------------------------------------------------------------------------
 *
 */
static void DoUniHeaderCopy(GoepUniType *Dst, U16 *Len, U16 MaxLen, 
                            U8 *Src, U16 SrcLen)
{
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U8  *src;
    U16  toCopy;

    src = Src;
    toCopy = SrcLen;

    /* If we have run out of space to write data, return */
    if ((*Len & LEN_UNICODE_MASK) == MaxLen) {
        *Len &= LEN_UNICODE_MASK;
        return;
    }
    
    /* Advance destination ptr past written data. */
    Dst += (*Len & LEN_UNICODE_MASK);

    /* Convert to ASCII as we copy */
    while (toCopy) {
        if (*Len & LEN_UNICODE_ODD_FLAG) {    /* Unicode Odd */
            /* This is the byte to store */
            *Dst++ = *src;
            *Len &= ~LEN_UNICODE_ODD_FLAG;
            (*Len)++;
        } else {
            *Len |= LEN_UNICODE_ODD_FLAG;
        }
        src++;
        toCopy--;

        if ((*Len & LEN_UNICODE_MASK) == MaxLen) {
            *Len &= LEN_UNICODE_MASK;
            break;
        }
    }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    /* Since we are copying the bytes one at a time, we must make sure
     * that MaxLen reflects a larger type that GoepUniType may have 
     * referenced (i.e. U16).  Doing so will make sure that we can 
     * copy every byte within the GoepUniType buffer provided.
     */
    DoHeaderCopy((U8 *)Dst, Len, (U16)(MaxLen*sizeof(GoepUniType)), Src, SrcLen);
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
}

/*---------------------------------------------------------------------------
 *            DoHeaderCopy
 *---------------------------------------------------------------------------
 *
 */
static void DoHeaderCopy(U8 *Dst, U16 *DstLen, U16 MaxLen, U8 *Src, U16 SrcLen)
{
    U16 toCopy;

    /* Advance destination ptr past written data. */
    Dst += *DstLen;

    toCopy = (U16)min((MaxLen - *DstLen), SrcLen);
    OS_MemCopy(Dst, Src, toCopy);
    *DstLen = (U16)(*DstLen + toCopy);
}

/*---------------------------------------------------------------------------
 *            GetFreeConnId
 *---------------------------------------------------------------------------
 *
 */
static BOOL GetFreeConnId(U8 *id, U8 type)
{
    U8 i;

    for (i = 0; i < GOEP_NUM_OBEX_CONS; i++) {
        if (type == GOEP_SERVER) {
#if OBEX_ROLE_SERVER == XA_ENABLED 
            if (GOES(servers)[i].connCount == 0) {
                /* Free server */
                *id = i;
                return TRUE;
            }
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */
        }
        else if (type == GOEP_CLIENT) {
#if OBEX_ROLE_CLIENT == XA_ENABLED
            if (GOEC(clients)[i].connCount == 0) {
                /* Free client */
                *id = i;
                return TRUE;
            }
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */
        }
    }

    return FALSE;
}

#if OBEX_ROLE_CLIENT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            BuildConnectReq
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function builds the OBEX headers used in the Connect
 *            Request packet.
 *
 */
static ObStatus BuildConnectReq(GoepClientObexCons *client, GoepConnectReq *connect)
{
    ObexProtocolHdr     header;
    ObStatus            status = OB_STATUS_SUCCESS;

    if (connect) {
        /* Build OBEX Connect headers */
        if (connect->target) {
            header.type = OBEXH_TARGET;
            header.target = (U8 *)connect->target;
            header.targetLen = connect->targetLen;
            status = OBEX_ClientBuildProtocolHeader(&client->obc, &header);
            if (status != OB_STATUS_SUCCESS) {
                return status;
            }
        }

        if (connect->who) {
            OBEXH_BuildByteSeq(&client->obc.handle, OBEXH_WHO, 
                               connect->who, connect->whoLen);
        }
#if OBEX_AUTHENTICATION == XA_ENABLED
        if (connect->response) {
            if ((client->flags & GOEF_CHALLENGE) == 0) {
                status = OB_STATUS_INVALID_PARM;
                return status;
            }
            OBEXH_BuildAuthResponse(&client->obc.handle, connect->response, 
                                    client->currOp.challenge.nonce);
        }

        if (connect->challenge) {
            OBEXH_BuildAuthChallenge(&client->obc.handle, connect->challenge, 
                                     client->nonce);
        }
#endif /* OBEX_AUTHENTICATION == XA_ENABLED */
    }

    return status;
}
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_SRM_MODE == XA_ENABLED
#if OBEX_ROLE_CLIENT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            ClientBuildSrmHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function builds the OBEX SRM Parameter header for 
 *            transmission by the GOEP Client.  
 *
 * Return:    OB_STATUS_SUCCESS - if the OBEX SRM Parameter Headers was built successfully
 *            OB_STATUS_FAILED - indicates that the header could not be built successfully
 *            OB_STATUS_NOT_FOUND - if no header needs to be built
 */
static ObStatus ClientBuildSrmHeader(GoepClientObexCons *client) {
    ObStatus        status = OB_STATUS_FAILED;

    Assert(client);

    if (client->srmSettings.flags != OSF_PARAMETERS) {
        /* No header to build */
        return OB_STATUS_NOT_FOUND;
    }

    if (!client->srmParamAllowed) {
        /* SRM wait parameter is not allowed any more for this operation */
        return OB_STATUS_FAILED;
    }

    /* Build the SRM Parameter header */
    status = OBEX_ClientSetSrmSettings(&client->obc, &client->srmSettings);

    /* Clear the SRM Parameter values now that the header is built */
    OS_MemSet((U8 *)&client->srmSettings, 0, sizeof(ObexSrmSettings));

    return status;
}
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_ROLE_SERVER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            ServerBuildSrmHeader()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function builds the OBEX SRM Parameter header for 
 *            transmission by the GOEP Server.  
 *
 * Return:    OB_STATUS_SUCCESS - if the OBEX SRM Parameter Headers was built successfully
 *            OB_STATUS_FAILED - indicates that the header could not be built successfully
 *            OB_STATUS_NOT_FOUND - if no header needs to be built
 */
static ObStatus ServerBuildSrmHeader(GoepServerObexCons *server) {
    ObStatus        status = OB_STATUS_FAILED;

    Assert(server);

    if (server->srmSettings.flags != OSF_PARAMETERS) {
        /* No header to build */
        return OB_STATUS_NOT_FOUND;
    }

    if (!server->srmParamAllowed) {
        /* SRM wait parameter is not allowed any more for this operation */
        return OB_STATUS_FAILED;
    }

    /* Build the SRM Parameter header */
    status = OBEX_ServerSetSrmSettings(&server->obs, &server->srmSettings);

    /* Clear the SRM Parameter values now that the header is built */
    OS_MemSet((U8 *)&server->srmSettings, 0, sizeof(ObexSrmSettings));

    return status;
}
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */
#endif /* OBEX_SRM_MODE == XA_ENABLED */

#if GOEP_ADDITIONAL_HEADERS > 0
#if OBEX_ROLE_SERVER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            ServerBuildHeaders()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function builds Byte Sequence, UNICODE, 1-byte, and 4-byte 
 *            OBEX headers for transmission by the GOEP Server.  
 *
 * Return:    True if the OBEX Headers were built successfully. False indicates 
 *            that the headers could not be built successfully.
 */
static BOOL ServerBuildHeaders(GoepServerObexCons *server) {
    U8              i;
    U32             value;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U16             len;
    U16             uniName[GOEP_MAX_UNICODE_LEN];
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    BOOL            status = TRUE;

    Assert(server);

    for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) {
        if (server->queuedHeaders[i].type != 0) {
            switch (server->queuedHeaders[i].type & 0xC0) {
            case 0x00:
                /* Unicode */
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
                /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the header
                 * in Unicode form to prevent memory corruption problems.
                 */
                if ((server->queuedHeaders[i].len * 2) > GOEP_MAX_UNICODE_LEN) {
                    status = FALSE;
                    break;
                }
            
                if (server->queuedHeaders[i].len == 0) {
                    /* Empty Unicode header */
                    goto Empty;
                }

                len = (U16)(AsciiToUnicode(uniName, (const char *)server->queuedHeaders[i].buffer) + 2);
                if (len >= 2) {
                    status = OBEXH_BuildUnicode(&server->obs.handle, server->queuedHeaders[i].type, 
                                                (const U8 *)uniName, len);
                } else {
Empty:
                    status = OBEXH_BuildUnicode(&server->obs.handle, server->queuedHeaders[i].type, 0, 0);
                }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
                if (server->queuedHeaders[i].len >= 2) {
                    status = OBEXH_BuildUnicode(&server->obs.handle, server->queuedHeaders[i].type, 
                                                server->queuedHeaders[i].buffer, server->queuedHeaders[i].len);
                } else {
                    status = OBEXH_BuildUnicode(&server->obs.handle, server->queuedHeaders[i].type, 0, 0);
                }
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
                break;

            case 0x40:
                /* Byte Sequence */
                status = OBEXH_BuildByteSeq(&server->obs.handle, server->queuedHeaders[i].type,
                                            server->queuedHeaders[i].buffer, server->queuedHeaders[i].len);
                break;

            case 0x80:
                /* 1-byte */
                status = OBEXH_Build1Byte(&server->obs.handle, server->queuedHeaders[i].type,
                                          server->queuedHeaders[i].buffer[0]);
                break;

            case 0xC0:
                /* 4-byte */
                value = BEtoHost32(server->queuedHeaders[i].buffer);
                status = OBEXH_Build4Byte(&server->obs.handle, server->queuedHeaders[i].type,
                                          value);
                break;
            }

            if (status == FALSE) {
                /* Clear the unsuccessful header */
                OS_MemSet((U8 *)&server->queuedHeaders[i], 0, sizeof(server->queuedHeaders[i]));
                return FALSE;
            }
        }
    }

    if (status == TRUE) {
        /* Clear all of the built headers, since they have been added successfully */
        for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) 
            OS_MemSet((U8 *)&server->queuedHeaders[i], 0, sizeof(server->queuedHeaders[i]));
    }

    return status;
}
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

#if OBEX_ROLE_CLIENT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            ClientBuildHeaders()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  This function builds Byte Sequence, UNICODE, 1-byte, and 4-byte 
 *            OBEX headers for transmission by the GOEP Client. Only a GET
 *            request can have BODY headers that span multiple OBEX packets.
 *            This function will queue as much header data as possible in the 
 *            current OBEX packet, with the remainder of the BODY header 
 *            information being sent in subsequent packets.  However, if a 
 *            BODY header exists, it will be the only header sent in the GET
 *            request.  This ensures that any headers sent after the BODY 
 *            header (i.e. End Of Body) will fit into one GET request packet.
 *
 * Return:    True if the OBEX Headers were built successfully. False indicates 
 *            that the headers could not be built successfully.
 */
static BOOL ClientBuildHeaders(GoepClientObexCons *client, U16 *more) {
    U8              i, index;
    U32             value;
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
    U16             len;
    U16             uniName[GOEP_MAX_UNICODE_LEN];
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
    U16             transmitLen = 0;
    BOOL            status = TRUE;

    Assert(client && more);

    /* Assume no more header information to send */
    *more = 0;

    for (i = 0; i < GOEP_ADDITIONAL_HEADERS; i++) {
        if (client->queuedHeaders[i].type != 0) {
            switch (client->queuedHeaders[i].type & 0xC0) {
            case 0x00:
                /* Unicode */
#if GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED
                /* Make sure GOEP_MAX_UNICODE_LEN is large enough for the header
                 * in Unicode form to prevent memory corruption problems.
                 */
                if ((client->queuedHeaders[i].len * 2) > GOEP_MAX_UNICODE_LEN) {
                    status = FALSE;
                    break;
                }
            
                if (client->queuedHeaders[i].len == 0) {
                    /* Empty Unicode header */
                    goto Empty;
                }

                len = (U16)(AsciiToUnicode(uniName, (const char *)client->queuedHeaders[i].buffer) + 2);
                if (len >= 2) {
                    status = OBEXH_BuildUnicode(&client->obc.handle, client->queuedHeaders[i].type, 
                                                (U8 *)uniName, len);
                } else {
Empty:
                    status = OBEXH_BuildUnicode(&client->obc.handle, client->queuedHeaders[i].type, 0, 0);
                }
#else /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
                if (client->queuedHeaders[i].len >= 2)
                    status = OBEXH_BuildUnicode(&client->obc.handle, client->queuedHeaders[i].type, 
                                                client->queuedHeaders[i].buffer, client->queuedHeaders[i].len);
                else status = OBEXH_BuildUnicode(&client->obc.handle, client->queuedHeaders[i].type, 0, 0);
#endif /* GOEP_DOES_UNICODE_CONVERSIONS == XA_ENABLED */
                break;

            case 0x40:                
                /* Byte Sequence */
                if ((client->currOp.oper == GOEP_OPER_PULL) && 
                    (client->queuedHeaders[i].type == OBEXH_BODY)) {
                    /* Calculate how much Body header data can fit into an OBEX packet */
                    transmitLen = (U16)min(client->queuedHeaders[i].len, 
                                      (OBEXH_GetAvailableTxHeaderLen(&client->obc.handle) - 3));
                } else {
                    /* The entire header must fit in one OBEX packet, if not we should not add the header */
                    transmitLen = client->queuedHeaders[i].len;
                }

                /* Byte Sequence */
                status = OBEXH_BuildByteSeq(&client->obc.handle, client->queuedHeaders[i].type,
                                            client->queuedHeaders[i].buffer, transmitLen);

                if (status == TRUE) {
                    if ((client->currOp.oper == GOEP_OPER_PULL) && 
                        (client->queuedHeaders[i].type == OBEXH_BODY)) {
                        if ((client->queuedHeaders[i].len - transmitLen) > 0) {
                            /* Update the length and buffer, since additional data needs 
                             * to be sent in our next packet.
                             */
                            client->queuedHeaders[i].len = (U16)(client->queuedHeaders[i].len - transmitLen);
                            client->queuedHeaders[i].buffer += transmitLen;
                            *more = client->queuedHeaders[i].len;
                            goto Done;
                        }

                        /* Other headers may exist after the BODY header.  However, since we don't know
                         * how much space is available in the packet after the BODY is complete, we'll 
                         * wait to send any remaining headers in our next packet.  All remaining headers
                         * after the BODY must fit into one packet.
                         */
                        *more = client->queuedHeaders[i].len;
                        /* Clear the complete BODY header. */
                        i++;
                        goto Done;
                    }
                }
                break;

            case 0x80:
                /* 1-byte */
                status = OBEXH_Build1Byte(&client->obc.handle, client->queuedHeaders[i].type,
                                          client->queuedHeaders[i].buffer[0]);
                break;
                
            case 0xC0:
                /* 4-byte */
                value = BEtoHost32(client->queuedHeaders[i].buffer);
                status = OBEXH_Build4Byte(&client->obc.handle, client->queuedHeaders[i].type, value);
                break;
            }

            if (status == FALSE) {
                /* Clear the failed queued header. */
                OS_MemSet((U8 *)&client->queuedHeaders[i], 0, sizeof(client->queuedHeaders[i]));
                return FALSE;
            }
        }
    }

 Done:
    if (status == TRUE) {
        /* Clear all of the built headers, since they have been added successfully */
        for (index = 0; index < i; index++) 
            OS_MemSet((U8 *)&client->queuedHeaders[index], 0, sizeof(client->queuedHeaders[index]));
    }
    
    return status;
}
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */
#endif /* GOEP_ADDITIONAL_HEADERS > 0 */

#if BT_SECURITY == XA_ENABLED
#if OBEX_ROLE_CLIENT == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GoepClientSecAccessRequest()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Security access checking used for all GOEP client operations.
 */
static BtStatus GoepClientSecAccessRequest(GoepClientApp *Client)
{
    BtStatus            status;
    BOOL                ok;
    ObexTpConnInfo      tpInfo;
    GoepClientObexCons *client = &GOEC(clients)[Client->connId];

    if (client->flags & GOEF_SEC_ACCESS_REQUEST) {
        /* Another SEC_AccessRequest has been issued, but has not returned yet */
        return OB_STATUS_BUSY;
    }

    ok = OBEX_GetTpConnInfo(&client->obc.handle, &tpInfo);
    if (!ok) {
        /* Transport connection is not up */
        return OB_STATUS_NO_CONNECT;
    }

    /* Check security for this GoepClientApp */
    Client->secToken.id = SEC_GOEP_ID;
    Client->secToken.remDev = tpInfo.remDev;
    Client->secToken.channel = (U32)Client;
    Client->secToken.incoming = FALSE;

    status = SEC_AccessRequest(&(Client->secToken));
    if ((status != BT_STATUS_SUCCESS) && (status != BT_STATUS_PENDING)) {
        /* Failure */
        status = BT_STATUS_RESTRICTED;
    } else if (status == BT_STATUS_PENDING) {
        /* Pending, so set the access request flag */
        client->flags |= GOEF_SEC_ACCESS_REQUEST;
    }

    return status;
}
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */

#if OBEX_ROLE_SERVER == XA_ENABLED
/*---------------------------------------------------------------------------
 *            GoepServerSecAccessRequest()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Security access checking used for all GOEP server operations.
 */
static BtStatus GoepServerSecAccessRequest(GoepServerObexCons *Server)
{
    BOOL                ok;
    BtStatus            status;
    ObexTpConnInfo      tpInfo;

    if (Server->flags & GOEF_SEC_ACCESS_REQUEST) {
        /* Another SEC_AccessRequest has been issued, but has not returned yet */
        return OB_STATUS_BUSY;
    }

    /* Check security for this GoepServerApp */
    ok = OBEX_GetTpConnInfo(&Server->obs.handle, &tpInfo);
    if (!ok) {
        /* Failure */
        status = BT_STATUS_RESTRICTED;
        goto Exit;
    }

    Server->currOp.handler->secToken.remDev = tpInfo.remDev;
    Server->currOp.handler->secToken.id = SEC_GOEP_ID;
    Server->currOp.handler->secToken.channel = (U32)Server->currOp.handler;
    Server->currOp.handler->secToken.incoming = TRUE;

    status = SEC_AccessRequest(&(Server->currOp.handler->secToken));
    if ((status != BT_STATUS_SUCCESS) && (status != BT_STATUS_PENDING)) {
        /* Failure */
        status = BT_STATUS_RESTRICTED;
    } else if (status == BT_STATUS_PENDING) {
        /* Pending, so set the access request flag */
        Server->flags |= GOEF_SEC_ACCESS_REQUEST;
    }

Exit:
    return status;
}
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

/*---------------------------------------------------------------------------
 *            GoepSecCallback()
 *---------------------------------------------------------------------------
 *
 * Synopsis:  Callback for security check.
 */
void GoepSecCallback(const BtEvent *Event)
{
    ObStatus             status;
#if OBEX_ROLE_CLIENT == XA_ENABLED
    GoepClientObexCons  *client = 0;
    GoepClientApp       *clientApp = 0;
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */
#if OBEX_ROLE_SERVER == XA_ENABLED 
    GoepServerObexCons  *server = 0;
    GoepServerApp       *serverApp = 0;
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */

    Report(("GOEP: GoepSecCallback: Event: %d, ErrCode: %d", Event->eType, Event->errCode));
    
    if (Event->p.secToken->incoming == FALSE) {
#if OBEX_ROLE_CLIENT == XA_ENABLED
        /* This is an outgoing connection (GOEP Client) */
        clientApp = (GoepClientApp *)Event->p.secToken->channel;
        client = &GOEC(clients)[clientApp->connId];
        if (Event->eType == BTEVENT_ACCESS_APPROVED) {
            /* Security approved, now send the OBEX Connect operation */
            client->flags &= ~GOEF_SEC_ACCESS_REQUEST;
            /* Complete either the Connect or Session, based on which is issued first */
            if (client->flags & GOEF_CONNECT_ISSUED) {
                status = OBEX_ClientConnect(&client->obc);
            }
#if OBEX_SESSION_SUPPORT == XA_ENABLED
            else if (client->flags & GOEF_CREATE_ISSUED) {
                if (clientApp->connect) {
                    /* Don't reissue the Create Session */
                    clientApp->connect->session = 0;
                }
                status = GOEP_Connect(clientApp, clientApp->connect);
                /* Now clear the connect values */
                clientApp->connect = 0;
            } else if (client->flags & GOEF_RESUME_ISSUED) {
                status = GOEP_ResumeSession(clientApp, clientApp->session, 
                                            clientApp->resumeParms, clientApp->timeout);

                /* Now clear the saved session values */
                clientApp->timeout = 0;
                clientApp->session = 0;
                clientApp->resumeParms = 0;
            }
#endif /* OBEX_SESSION_SUPPORT == XA_ENABLED */
            else {
                /* Unexpected operation */
                Assert(0);
            }
        } else {
            /* Security failed */
            client->flags &= ~GOEF_SEC_ACCESS_REQUEST;
            /* Clear the authorized flag, since security failed */
            clientApp->authorized = FALSE;
            /* Pass up the failure event */
            client->currOp.reason = OBRC_TRANSPORT_SECURITY_FAILURE;
            NotifyCurrClient(&client->obc, GOEP_EVENT_ABORTED);
        }
#endif /* OBEX_ROLE_CLIENT == XA_ENABLED */
    } 
#if OBEX_ROLE_SERVER == XA_ENABLED
    else {
        /* This is an incoming connection (GOEP Server) */
        serverApp = (GoepServerApp *)Event->p.secToken->channel;
        server = &GOES(servers)[serverApp->connId];
        if (Event->eType == BTEVENT_ACCESS_APPROVED) {
            /* Security approved - now pass up the CONTINUE event */
            server->flags &= ~GOEF_SEC_ACCESS_REQUEST;
            NotifyCurrServer(&server->obs, GOEP_EVENT_CONTINUE);
        } else {
            /* Security failed - reject the operation, and pass up the CONTINUE event */
            server->flags &= ~GOEF_SEC_ACCESS_REQUEST;
            /* Clear the authorized flag, since security failed */
            serverApp->authorized = FALSE;

            /* Ensure we force a server rejection in this case. Based on the number of profile
             * connections and the status of those connections, it's possible that the
             * context of this call will require special processing to complete.
             */
            status = OBEX_ServerAbort(&server->obs, OBRC_TRANSPORT_SECURITY_FAILURE, TRUE);
            if (status != OB_STATUS_SUCCESS) {
                Report(("GOEP: Failed call to OBEX_ServerAbort: %d", status));
            }

            NotifyCurrServer(&server->obs, GOEP_EVENT_CONTINUE);
        }
    }
#endif /* OBEX_ROLE_SERVER == XA_ENABLED */
}
#endif /* BT_SECURITY == XA_ENABLED */

/**************************************************************************/
/*                                                                        */
/*       Copyright (c) Microsoft Corporation. All rights reserved.        */
/*                                                                        */
/*       This software is licensed under the Microsoft Software License   */
/*       Terms for Microsoft Azure RTOS. Full text of the license can be  */
/*       found in the LICENSE file at https://aka.ms/AzureRTOS_EULA       */
/*       and in the root directory of this software.                      */
/*                                                                        */
/**************************************************************************/


/**************************************************************************/
/**************************************************************************/
/**                                                                       */
/** NetX Secure Component                                                 */
/**                                                                       */
/**    Transport Layer Security (TLS)                                     */
/**                                                                       */
/**************************************************************************/
/**************************************************************************/

#define NX_SECURE_SOURCE_CODE

#include "nx_secure_tls.h"

/**************************************************************************/
/*                                                                        */
/*  FUNCTION                                               RELEASE        */
/*                                                                        */
/*    _nx_secure_tls_session_receive                      PORTABLE C      */
/*                                                           6.1          */
/*  AUTHOR                                                                */
/*                                                                        */
/*    Timothy Stapko, Microsoft Corporation                               */
/*                                                                        */
/*  DESCRIPTION                                                           */
/*                                                                        */
/*    This function receives data from an active TLS session, handling    */
/*    all decryption and verification before returning the data to the    */
/*    caller in the supplied NX_PACKET structure.                         */
/*                                                                        */
/*  INPUT                                                                 */
/*                                                                        */
/*    tls_session                           TLS control block             */
/*    packet_ptr_ptr                        Pointer to return packet      */
/*    wait_option                           Indicates how long the caller */
/*                                          should wait for the response  */
/*                                                                        */
/*  OUTPUT                                                                */
/*                                                                        */
/*    status                                Completion status             */
/*                                                                        */
/*  CALLS                                                                 */
/*                                                                        */
/*    _nx_secure_tls_handshake_process      Process TLS handshake         */
/*    _nx_secure_tls_session_receive_records                              */
/*                                          Receive TLS records           */
/*                                                                        */
/*  CALLED BY                                                             */
/*                                                                        */
/*    Application Code                                                    */
/*                                                                        */
/*  RELEASE HISTORY                                                       */
/*                                                                        */
/*    DATE              NAME                      DESCRIPTION             */
/*                                                                        */
/*  05-19-2020     Timothy Stapko           Initial Version 6.0           */
/*  09-30-2020     Timothy Stapko           Modified comment(s),          */
/*                                            supported chained packet,   */
/*                                            fixed renegotiation bug,    */
/*                                            resulting in version 6.1    */
/*                                                                        */
/**************************************************************************/
UINT  _nx_secure_tls_session_receive(NX_SECURE_TLS_SESSION *tls_session, NX_PACKET **packet_ptr_ptr,
                                     ULONG wait_option)
{
UINT status;
#ifndef NX_SECURE_TLS_DISABLE_SECURE_RENEGOTIATION
UINT local_initiated_renegotiation = NX_FALSE;
#endif /* NX_SECURE_TLS_DISABLE_SECURE_RENEGOTIATION */

    /* Session receive logic:
     * 1. Receive incoming packets
     * 2. Process records and receive while full record is not yet received.
     * 3. If renegotiation initiated, process the renegotiation handshake.
     *    3a. Process entire handshake (receive TCP packets, process records)
     *    3b. Once handshake processed, receive any new packets, but only if
     *        the remote host initiated the renegotiation.
     */


#ifndef NX_SECURE_TLS_DISABLE_SECURE_RENEGOTIATION
#ifndef NX_SECURE_TLS_CLIENT_DISABLED
    if (tls_session -> nx_secure_tls_socket_type == NX_SECURE_TLS_SESSION_TYPE_CLIENT &&
        tls_session -> nx_secure_tls_client_state == NX_SECURE_TLS_CLIENT_STATE_RENEGOTIATING)
    {
        local_initiated_renegotiation = NX_TRUE;
    }
#endif

#ifndef NX_SECURE_TLS_SERVER_DISABLED
    if (tls_session -> nx_secure_tls_socket_type == NX_SECURE_TLS_SESSION_TYPE_SERVER &&
        tls_session -> nx_secure_tls_server_state == NX_SECURE_TLS_SERVER_STATE_HELLO_REQUEST)
    {
        local_initiated_renegotiation = NX_TRUE;
    }
#endif
#endif /* NX_SECURE_TLS_DISABLE_SECURE_RENEGOTIATION */

    /* Try receiving records from the remote host. */
    status = _nx_secure_tls_session_receive_records(tls_session, packet_ptr_ptr, wait_option);

#ifndef NX_SECURE_TLS_DISABLE_SECURE_RENEGOTIATION
    /* See if we have a renegotiation handshake. Continue processing following the
       hello message that was received. */
    if (status == NX_SUCCESS && tls_session -> nx_secure_tls_renegotiation_handshake)
    {

        /* Clear flag to prevent infinite recursion. */
        tls_session -> nx_secure_tls_renegotiation_handshake = NX_FALSE;

        /* Process the handshake. */
        status = _nx_secure_tls_handshake_process(tls_session, wait_option);

        if (status != NX_SUCCESS)
        {
            return(status);
        }

        /* If this renegotiation was initiated by us, don't receive additional data as
           that will be up to the application. */
        if (!local_initiated_renegotiation)
        {
            /* Handle any data that followed the re-negotiation handshake. */
            status = _nx_secure_tls_session_receive_records(tls_session, packet_ptr_ptr, wait_option);
        }
    }
    else
#endif /* NX_SECURE_TLS_DISABLE_SECURE_RENEGOTIATION */
    {
        if (status == NX_SECURE_TLS_POST_HANDSHAKE_RECEIVED)
        {
            /* Continue processing while we are receiving post-handshake messages. */
            while (status == NX_SECURE_TLS_POST_HANDSHAKE_RECEIVED)
            {
                status = _nx_secure_tls_session_receive_records(tls_session, packet_ptr_ptr, wait_option);
            }
        }
    }


    return(status);
}


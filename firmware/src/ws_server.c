/*******************************************************************************
  MPLAB Harmony Application Source File
  
  Company:
    Microchip Technology Inc.
  
  File Name:
    ws_server.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It 
    implements the logic of the application's state machine and it may call 
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
Copyright (c) 2013-2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE.
IN NO EVENT SHALL MICROCHIP OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR
OTHER LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE OR
CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT OF
SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.
 *******************************************************************************/
// DOM-IGNORE-END


// *****************************************************************************
// *****************************************************************************
// Section: Included Files 
// *****************************************************************************
// *****************************************************************************

#include "ws_server.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************
void clientWorker(NET_PRES_SKT_HANDLE_T clientSocket);

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.
    
    Application strings and buffers are be defined outside this structure.
 */

WS_SERVER_DATA ws_serverData;

// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
 */

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions.
 */


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void WS_SERVER_Initialize ( void )

  Remarks:
    See prototype in ws_server.h.
 */

void WS_SERVER_Initialize(void)
{
    /* Place the App state machine in its initial state. */
    ws_serverData.state = WS_SERVER_STATE_INIT;
    ws_serverData.StreamActive = false;
    ws_serverData.delay = 10;
    
    /* TODO: Initialize your application's state machine and other
     * parameters.
     */
}

/******************************************************************************
  Function:
    void WS_SERVER_Tasks ( void )

  Remarks:
    See prototype in ws_server.h.
 */

void WS_SERVER_Tasks(void)
{
    NET_PRES_SKT_ERROR_T error;
    bool appInitialized = true;

    /* Check the application's current state. */
    switch (ws_serverData.state)
    {
        /* Application's initial state. */
    case WS_SERVER_STATE_INIT:
        appInitialized = true;
        if (appInitialized)
        {
            ws_serverData.state = WS_STATE_OPENING_SERVER_SOCKET;
        }
        break;

    case WS_STATE_OPENING_SERVER_SOCKET:
        SYS_CONSOLE_PRINT("Waiting for Client Connection on port: %d\r\n", WEBSOCKET_SERVER_PORT);
        ws_serverData.socket = NET_PRES_SocketOpen(0, NET_PRES_SKT_UNENCRYPTED_STREAM_SERVER, IP_ADDRESS_TYPE_IPV4, WEBSOCKET_SERVER_PORT, 0, &error);
        if (ws_serverData.socket == NET_PRES_INVALID_SOCKET)
        {
            SYS_CONSOLE_MESSAGE("Couldn't open server socket\r\n");
            break;
        }
        ws_serverData.state = WS_STATE_WAITING_FOR_CONNECTION;
        break;

    case WS_STATE_WAITING_FOR_CONNECTION:
        if (!NET_PRES_SocketIsConnected(ws_serverData.socket))
        {
            return;
        }
        ws_serverData.state = WS_STATE_SERVING_RXTX;
        SYS_CONSOLE_MESSAGE("Received a clear text connection\r\n");
        break;

    case WS_STATE_SERVING_RXTX:
        if (!NET_PRES_SocketIsConnected(ws_serverData.socket))
        {
            ws_serverData.state = WS_STATE_CLOSING_CONNECTION;            
            break;
        }
        if (NET_PRES_SocketReadIsReady(ws_serverData.socket) != 0)
        {
            clientWorker(ws_serverData.socket);
        }
        break;

    case WS_STATE_CLOSING_CONNECTION:
        
        NET_PRES_SocketClose(ws_serverData.socket);
        ws_serverData.state = WS_STATE_OPENING_SERVER_SOCKET;
        SYS_CONSOLE_MESSAGE("Clear Text connection was closed\r\n");
        break;

        /* The default state should never be executed. */
    default:
    {
        /* TODO: Handle error in application's state machine. */
        break;
    }
    }
}



/*******************************************************************************
 End of File
 */

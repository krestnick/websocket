/*
 * Copyright (c) 2014 Putilov Andrey
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
//#include <sys/socket.h>
//#include <netinet/in.h>
//#include <arpa/inet.h>
//#include <unistd.h>

#include "ws_server.h"
#include "websocket.h"

#define PORT 8088
#define BUF_LEN 0xFFFF
//#define PACKET_DUMP

uint8_t gBuffer[BUF_LEN];
extern WS_SERVER_DATA ws_serverData;
NET_PRES_SKT_HANDLE_T ActiveclientSocket = 0;

void error(const char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

int safeSend(int clientSocket, const uint8_t *buffer, size_t bufferSize)
{
    ssize_t written;
#ifdef PACKET_DUMP
    SYS_CONSOLE_MESSAGE("out packet:\n");
    buffer[bufferSize] = 0;
    SYS_CONSOLE_PRINT("%s\n", buffer);
    SYS_CONSOLE_MESSAGE("\n");
#endif
    while (bufferSize > NET_PRES_SocketWriteIsReady(clientSocket, bufferSize, 0));

    written = NET_PRES_SocketWrite(clientSocket, buffer, bufferSize); //MR:
    if (written == -1)
    {
        NET_PRES_SocketClose(clientSocket); //MR:
        SYS_CONSOLE_MESSAGE("send failed");
        return EXIT_FAILURE;
    }
    if (written != bufferSize)
    {
        NET_PRES_SocketClose(clientSocket); //MR:
        SYS_CONSOLE_MESSAGE("written not all bytes");

        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void clientWorker(NET_PRES_SKT_HANDLE_T clientSocket)
{
    memset(gBuffer, 0, BUF_LEN);
    size_t readedLength = 0;
    size_t frameSize = BUF_LEN;
    enum wsState state = WS_STATE_OPENING;
    uint8_t *data = NULL;
    size_t dataSize = 0;
    enum wsFrameType frameType = WS_INCOMPLETE_FRAME;
    struct handshake hs;
    uint32_t value;
    nullHandshake(&hs);
    bool two_frames = false;
    ssize_t readed;
    ssize_t readed_second;
    size_t payloadLength;
    char *secondframe = NULL;

    ActiveclientSocket = clientSocket;

#define prepareBuffer frameSize = BUF_LEN; memset(gBuffer, 0, BUF_LEN);
#define initNewFrame frameType = WS_INCOMPLETE_FRAME; readedLength = 0; memset(gBuffer, 0, BUF_LEN);

    while (frameType == WS_INCOMPLETE_FRAME)
    {

        if (two_frames)
        {
            /* copy second frame to primary buffer */
            memcpy(gBuffer, secondframe, readed_second);
            free(secondframe);
            two_frames = false;
        }
        else
        {
            //----------------------------------------------------------------------
            // !!! Blocking for Incoming Data !!!
            while (NET_PRES_SocketReadIsReady(clientSocket) == 0);
            //----------------------------------------------------------------------
            readed = NET_PRES_SocketRead(clientSocket, gBuffer + readedLength, BUF_LEN - readedLength); //0; //recv(clientSocket, gBuffer+readedLength, BUF_LEN-readedLength, 0); //MR:
            /* if incoming data is a valid websocket packet */
            if (gBuffer[0] == 0x81)
            {
                payloadLength = gBuffer[1] & 0x7F;
                /* if buffer contains more than one frame.
                 * this could happen when the client send a text websocket packet
                 * and closing websocket packet very fast after each other. 
                 */
                if ((payloadLength + 6) != readed)
                {
                    /* adjust buffer length to first frame*/
                    readed_second = readed - (payloadLength + 6);
                    readed = payloadLength + 6;
                    secondframe = malloc(readed_second);
                    assert(secondframe);
                    /* copy second frame to temporary buffer */
                    memcpy(secondframe, &gBuffer[readed], readed_second);
                    /* set signal for processing second frame*/
                    two_frames = true;
                }
            }
        }

        if (!readed)
        {
            NET_PRES_SocketClose(clientSocket); //MR:
            SYS_CONSOLE_MESSAGE("recv failed");
            return;
        }

        readedLength += readed;
        assert(readedLength <= BUF_LEN);

        if (state == WS_STATE_OPENING)
        {
            frameType = wsParseHandshake(gBuffer, readedLength, &hs);
        }
        else
        {
            frameType = wsParseInputFrame(gBuffer, readedLength, &data, &dataSize);
        }

        if ((frameType == WS_INCOMPLETE_FRAME && readedLength == BUF_LEN) || frameType == WS_ERROR_FRAME)
        {
            if (frameType == WS_INCOMPLETE_FRAME)
                SYS_CONSOLE_MESSAGE("buffer too small\r\n");
            else
                SYS_CONSOLE_MESSAGE("error in incoming frame\r\n");

            if (state == WS_STATE_OPENING)
            {
                prepareBuffer;
                frameSize = sprintf((char *) gBuffer,
                        "HTTP/1.1 400 Bad Request\r\n"
                        "%s%s\r\n\r\n",
                        versionField,
                        version);
                safeSend(clientSocket, gBuffer, frameSize);
                break;
            }
            else
            {
                prepareBuffer;
                wsMakeFrame(NULL, 0, gBuffer, &frameSize, WS_CLOSING_FRAME);
                if (safeSend(clientSocket, gBuffer, frameSize) == EXIT_FAILURE)
                    break;
                state = WS_STATE_CLOSING;
                initNewFrame;
            }
        }

        if (state == WS_STATE_OPENING)
        {
            //assert(frameType == WS_OPENING_FRAME);
            if (frameType == WS_OPENING_FRAME)
            {
                // if resource is right, generate answer handshake and send it
                if (strcmp(hs.resource, "/echo") != 0)
                {
                    frameSize = sprintf((char *) gBuffer, "HTTP/1.1 404 Not Found\r\n\r\n");
                    safeSend(clientSocket, gBuffer, frameSize);
                    break;
                }

                prepareBuffer;
                wsGetHandshakeAnswer(&hs, gBuffer, &frameSize);
                freeHandshake(&hs);
                if (safeSend(clientSocket, gBuffer, frameSize) == EXIT_FAILURE)
                    break;
                state = WS_STATE_NORMAL;
                initNewFrame;
            }
        }
        else
        {
            if (frameType == WS_CLOSING_FRAME)
            {
                if (state == WS_STATE_CLOSING)
                {
                    break;
                }
                else
                {
                    prepareBuffer;
                    wsMakeFrame(NULL, 0, gBuffer, &frameSize, WS_CLOSING_FRAME);
                    safeSend(clientSocket, gBuffer, frameSize);
                    break;
                }
            }
            else if (frameType == WS_TEXT_FRAME)
            {
                uint8_t *recievedString = NULL;
                recievedString = malloc(dataSize + 1);
                assert(recievedString);
                memcpy(recievedString, data, dataSize);
                recievedString[ dataSize ] = 0;

                //--------------------------------------------------------------
                // Command Parser
                if (strstr((const char *) recievedString, "StartStream"))
                {
                    SYS_CONSOLE_MESSAGE("Stream Started\n\r");
                    ws_serverData.StreamActive = true;
                }
                else if (strstr((const char *) recievedString, "StopStream"))
                {
                    SYS_CONSOLE_MESSAGE("Stream Stopped\n\r");
                    ws_serverData.StreamActive = false;
                }
                else if (strstr((const char *) recievedString, "SetDelay"))
                {
                    value = atoi((const char *) (recievedString + 9));
                    SYS_CONSOLE_PRINT("Set Streamer Task Delay: %d\n\r", value);
                    ws_serverData.delay = value;
                }
                else if (strstr((const char *) recievedString, "reset"))
                {
                    SYS_CONSOLE_MESSAGE("Reset\n\r");
                    vTaskDelay(1000);
                    SYS_RESET_SoftwareReset();
                }
                //--------------------------------------------------------------

                prepareBuffer;
                wsMakeFrame(recievedString, dataSize, gBuffer, &frameSize, WS_TEXT_FRAME);
                free(recievedString);
                if (safeSend(clientSocket, gBuffer, frameSize) == EXIT_FAILURE)

                    break;
                initNewFrame;

            }
        }
    } // read/write cycle

    NET_PRES_SocketClose(clientSocket); //MR:
}

void wsSendDataCallback(const uint8_t *DataString, size_t DataLength)
{
    size_t StreamFrameSize = BUF_LEN;
    static uint8_t gBufferStream[BUF_LEN];

    if (ws_serverData.StreamActive == true)
    {
        memset(gBufferStream, 0, BUF_LEN);
        wsMakeFrame(DataString, DataLength, gBufferStream, &StreamFrameSize, WS_TEXT_FRAME);
        safeSend(ActiveclientSocket, gBufferStream, StreamFrameSize);
    }

}

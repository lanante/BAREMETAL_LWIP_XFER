/******************************************************************************
*
* Copyright (C) 2017 Xilinx, Inc.  All rights reserved.
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
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

#ifndef __UDP_PERF_SERVER_H_
#define __UDP_PERF_SERVER_H_

#include "lwipopts.h"
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/udp.h"
#include "lwip/inet.h"
#include "xil_printf.h"

enum { XFER_START, XFER_DATA, XFER_BAR, XFER_BA, XFER_REQ, XFER_END,XFER_STATE,TX_ST };
enum { IDLE, WAIT, SEND_DATA, SEND_BAR, END };
extern uint8_t xfer_state;
extern struct udp_pcb *pcb;
extern void udp_xfer_send(uint32_t *payload,uint32_t xfer_size);
extern void start_application(ip_addr_t local_device_ipaddr, uint16_t port);
/* server port to listen on/connect to */
#define DEFAULT_REMOTE_HOST_IP "192.168.0.2"
#define DEFAULT_REMOTE_HOST_PORT 7


#endif /* __XFER_UDP_H_ */

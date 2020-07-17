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

/** Connection handle for a UDP Server session */

#include "xfer_udp.h"

extern struct netif server_netif;
static struct udp_pcb *pcb;
static struct perf_stats server;
/* Report interval in ms */
#define REPORT_INTERVAL_TIME (INTERIM_REPORT_INTERVAL * 1000)

static uint8_t PID;
static uint8_t BITMAP[8192];

enum{XFER_ST, XFER_DATA, XFER_BAR, XFER_BA};

static void reset_bitmap(void)
{
	uint16_t i;
for (i=0;i<8192;i++)
{
	BITMAP[i]=0;
}
}

static uint8_t isbitmap_full(uint16_t NUM_SEG)
{
	uint8_t res;
	uint16_t i;
	uint16_t TOTAL=0;
for (i=0;i<1000;i++)
{
	TOTAL=TOTAL+BITMAP[i];
}
res=(TOTAL==NUM_SEG)?1:0;
return res;
}

/** Receive data on a udp session */
static void udp_recv_cb(void *arg, struct udp_pcb *tpcb,
		struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
	static u8_t first = 1;
	uint32_t *PKT;
    uint8_t CMD=0;
    uint16_t SEG_NUM=0;
	static uint16_t NUM_SEG=0;
	struct pbuf * pBA_H;
	struct pbuf * pBA_D;

uint8_t MASK;
uint16_t INDEX;
uint8_t BYTEMAP;
uint32_t BA_HEADER=0x00000003;

	PKT = p->payload;
	CMD=PKT[0];

	switch (CMD)
	{
	case XFER_ST: // XFER_ST
		NUM_SEG=(PKT[0] >> 16) & 0x0000FFFF;
		reset_bitmap();
		udp_sendto(tpcb, p, addr, port);  //SEND ACK
		pbuf_free(p);

		break;
	case XFER_DATA: //DATA
		SEG_NUM=(PKT[0] >> 16) & 0x0000FFFF;
		MASK=1<<(SEG_NUM%8);
		INDEX=SEG_NUM/8;
		BYTEMAP=BITMAP[INDEX] | (MASK);
		BITMAP[INDEX]=BYTEMAP;
		pbuf_free(p);

		break;

	case XFER_BAR: //BAR
		pBA_H = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t), PBUF_RAM);
		pBA_D = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t)*8192, PBUF_RAM);

	    memcpy(pBA_H->payload, &BA_HEADER, sizeof(uint32_t));
	    pBA_D->payload=BITMAP;
	    pbuf_cat(pBA_H,pBA_D);
	    udp_sendto(tpcb, pBA_H, addr, port);  //SEND BA
		pbuf_free(p);
		pbuf_free(pBA_H);

		break;

	}



		return;
}

void start_application(void)
{
	err_t err;

	/* Create Server PCB */
	pcb = udp_new();
	if (!pcb) {
		xil_printf("UDP server: Error creating PCB. Out of Memory\r\n");
		return;
	}

	err = udp_bind(pcb, IP_ADDR_ANY, UDP_CONN_PORT);
	if (err != ERR_OK) {
		xil_printf("UDP server: Unable to bind to port");
		xil_printf(" %d: err = %d\r\n", UDP_CONN_PORT, err);
		udp_remove(pcb);
		return;
	}

	/* specify callback to use for incoming connections */
	udp_recv(pcb, udp_recv_cb, NULL);

	return;
}

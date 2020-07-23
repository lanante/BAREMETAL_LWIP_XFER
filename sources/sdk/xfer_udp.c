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
#include "math.h"
extern struct netif server_netif;
extern uint8_t rx_flag;
extern uint8_t tx_flag;

struct udp_pcb *pcb;
static struct perf_stats server;
/* Report interval in ms */
#define REPORT_INTERVAL_TIME (INTERIM_REPORT_INTERVAL * 1000)

static uint8_t PID;
static uint8_t BITMAP[8192];
static uint16_t *RESP_LIST; //array of SEG_NUMS to be transmitted

static uint32_t TX_DUMMY[4096];
static uint32_t *REQ_BITMAP;

#define MAX_BURST 512

enum{XFER_ST, XFER_DATA, XFER_BAR, XFER_BA,XFER_REQ,XFER_END};
enum{IDLE, WAIT, SEND_DATA, SEND_BAR, END};

static void reset_bitmap(void)
{
	uint16_t i;
for (i=0;i<8192;i++)
{
	BITMAP[i]=0;
}
}

static void set_TXDUMMY(uint32_t inc)
{
	uint32_t i;
for (i=0;i<4096;i++)
{
	TX_DUMMY[i]=i+inc*1024;
}
}

static uint16_t set_resp_list(uint32_t *BITMAP32,uint16_t *RESP_LIST,uint16_t NUM_SEG )
{
	uint32_t i,j=0;
	uint32_t INDEX, OFFSET;
	uint8_t DATA;
for (i=0;i<NUM_SEG;i++)
{
	INDEX=i/32;
	OFFSET=i%32;
	DATA=(BITMAP32[INDEX]>>(OFFSET))&0x1;
if (DATA)
{
	RESP_LIST[j]=i;
	j++;
	if (j>=MAX_BURST)
	{
		return MAX_BURST;
	}
}

}
return j;
}

static uint32_t count_bitmap(void )
{
	uint32_t i,SUM=0;
for (i=0;i<8192;i++)
{
	SUM=SUM+__builtin_popcount(BITMAP[i]);
}

return SUM;
}

static uint32_t count_reqbitmap(uint32_t BYTEMAP_SIZE )
{
	uint32_t i,SUM=0,j;
for (i=0;i<BYTEMAP_SIZE/4;i++)
{
		SUM=SUM+__builtin_popcount(REQ_BITMAP[i]);
if ((i>1)&(SUM>10))
{
	j=0;
}
}

return SUM;
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
	struct pbuf * pH;
	struct pbuf * pD;
     uint32_t i=0,j=0;

uint8_t MASK;
uint16_t INDEX;
uint16_t OFFSET;
uint8_t BYTEMAP;
uint32_t BA_HEADER=0x00000003;
uint32_t REQ_HEADER=0x00000004;
uint32_t END_HEADER=0x00000005;
uint32_t DATA_HEADER=0x00000001;
uint32_t HEADER;
uint32_t BYTEMAP_SIZE;
uint32_t temp1,temp2,temp3;
uint16_t RESP_LENGTH;
	PKT = p->payload;
	CMD=PKT[0];

	switch (CMD)
	{
	case XFER_REQ:
		NUM_SEG=(PKT[0] >> 16) & 0x0000FFFF;
		BYTEMAP_SIZE=NUM_SEG/8;
		temp1=count_bitmap();
		RESP_LIST=malloc(sizeof(uint16_t)*MAX_BURST);


		if (NUM_SEG%8!=0) BYTEMAP_SIZE++;
	//	REQ_BITMAP=malloc(sizeof(uint8_t)*BYTEMAP_SIZE);
	//	memcpy(REQ_BITMAP, &PKT[1], sizeof(uint8_t)*BYTEMAP_SIZE);
	//	temp2=count_bitmap();
	//	temp3=count_reqbitmap(BYTEMAP_SIZE);
		RESP_LENGTH=set_resp_list(&PKT[1],RESP_LIST,NUM_SEG);
	 //   memcpy(pH->payload, &REQ_HEADER, sizeof(uint32_t));
	 //   memcpy(pD->payload, TX_DUMMY, sizeof(uint32_t)*100);
	  //  pbuf_cat(pH,pD);

		for (i=0;i<RESP_LENGTH;i++)
		{
			SEG_NUM=RESP_LIST[i];
			INDEX=SEG_NUM/8;
			OFFSET=(SEG_NUM%8);
			MASK=1<<OFFSET;

			set_TXDUMMY(RESP_LIST[i]);
			HEADER=(DATA_HEADER&0x0000FFFF) | ((SEG_NUM << 16)&0xFFFF0000);
			pH = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t), PBUF_RAM);
			pD = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t)*1024, PBUF_RAM);
			memcpy(pH->payload, &HEADER, sizeof(uint32_t));
			 memcpy(pD->payload, TX_DUMMY, sizeof(uint32_t)*1024);
				pbuf_cat(pH,pD);
		    udp_sendto(tpcb, pH, addr, port);  //SEND BA
			pbuf_free(pH);
			}

		//SEND RX_END
		pbuf_free(pH);
		pH = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t), PBUF_RAM);
	    memcpy(pH->payload, &END_HEADER, sizeof(uint32_t));
	    udp_sendto(tpcb, pH, addr, port);  //SEND BA
		pbuf_free(pH);
		free(RESP_LIST);


		pbuf_free(p);
		break;
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
		pH = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t), PBUF_RAM);
		pD = pbuf_alloc(PBUF_TRANSPORT, sizeof(uint32_t)*8192, PBUF_RAM);

	    memcpy(pH->payload, &BA_HEADER, sizeof(uint32_t));
	    pD->payload=BITMAP;
	    pbuf_cat(pH,pD);
	    udp_sendto(tpcb, pH, addr, port);  //SEND BA
		pbuf_free(p);
		pbuf_free(pH);

		break;

	}




		return;
}

void start_application(void)
{
	err_t err;
uint32_t i;
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

	for (i=0;i<4096;i++)
	{
		TX_DUMMY[i]=i;
	}

	return;
}

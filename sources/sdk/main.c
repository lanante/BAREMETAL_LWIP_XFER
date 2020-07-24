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

#include "lwip/inet.h"
#include "lwip/init.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/udp.h"
#include "lwipopts.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "sleep.h"
#include "xfer_udp.h"
#include "xil_cache.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xtime_l.h"
#include <stdio.h>

#define DEFAULT_LOCAL_IP "192.168.0.10"
#define DEFAULT_LOCAL_MAC_ADDRESS "00:0a:35:00:01:02"
#define DEFAULT_LOCAL_DEVICE_LISTEN_PORT 5001

#define DEFAULT_IP_MASK "255.255.255.0"
#define DEFAULT_GW_ADDRESS "192.168.1.1"

void platform_enable_interrupts(void);

struct netif server_netif;

static void assign_default_ip(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw) {
  int err;

  err = inet_aton(DEFAULT_LOCAL_IP, ip);
  if (!err)
    xil_printf("Invalid default IP address: %d\r\n", err);

  err = inet_aton(DEFAULT_IP_MASK, mask);
  if (!err)
    xil_printf("Invalid default IP MASK: %d\r\n", err);

  err = inet_aton(DEFAULT_GW_ADDRESS, gw);
  if (!err)
    xil_printf("Invalid default gateway address: %d\r\n", err);
}

int main(void) {
  struct netif *netif;
  char mac_addr_str[] = DEFAULT_LOCAL_MAC_ADDRESS;
  /* the mac address of the board. this should be unique per board */
  unsigned char mac_addr[6];

  sscanf(mac_addr_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac_addr[0],
         &mac_addr[1], &mac_addr[2], &mac_addr[3], &mac_addr[4], &mac_addr[5]);
  netif = &server_netif;

  init_platform();

  /* initialize lwIP */
  lwip_init();

  /* Add network interface to the netif_list, and set it as default */
  if (!xemac_add(netif, NULL, NULL, NULL, mac_addr, XPAR_XEMACPS_0_BASEADDR)) {
    xil_printf("Error adding N/W interface\r\n");
    return -1;
  }
  netif_set_default(netif);

  /* now enable interrupts */
  platform_enable_interrupts();

  /* specify that the network if is up */
  netif_set_up(netif);

  assign_default_ip(&(netif->ip_addr), &(netif->netmask), &(netif->gw));

  /* start the application*/
  start_application((netif->ip_addr), DEFAULT_LOCAL_DEVICE_LISTEN_PORT);

  XTime tNow, tDiff, tPrev;
  uint32_t payload;

  uint32_t rx_dummy_num_seg = 1000;
  payload = (XFER_START & 0x0000FFFF) | ((rx_dummy_num_seg << 16) & 0xFFFF0000);

  while (1) {

    XTime_GetTime(&tNow);
    tDiff = ((tNow - tPrev) / (COUNTS_PER_SECOND / 1000000));

    if (tDiff > 1000000) {
      tPrev = tNow;

      if (xfer_state != 1) {
        udp_xfer_send(&payload, sizeof(payload));
      }
    }

    xemacif_input(netif);
  }

  /* never reached */
  cleanup_platform();

  return 0;
}

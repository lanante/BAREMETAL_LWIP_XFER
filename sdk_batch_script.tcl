#!/usr/bin/tclsh


sdk setws project.sdk
sdk createhw -name hw -hwspec project.sdk/base_zynq_wrapper.hdf
sdk createbsp -name bsp -hwproject hw -proc ps7_cortexa9_0 -os standalone


setlib -bsp bsp -lib lwip202 
configbsp -bsp bsp lwip_dhcp false
configbsp -bsp bsp dhcp_does_arp_check false
configbsp -bsp bsp mem_size 524288 
configbsp -bsp bsp memp_n_pbuf 1024 
configbsp -bsp bsp memp_n_tcp_seg 1024 
configbsp -bsp bsp n_rx_descriptors 512 
configbsp -bsp bsp n_tx_descriptors 512 
configbsp -bsp bsp pbuf_pool_size 16384 
configbsp -bsp bsp phy_link_speed CONFIG_LINKSPEED1000 
updatemss -mss project.sdk/bsp/system.mss
regenbsp -bsp bsp 




sdk createapp -name sw -app {Empty Application} -hwproject hw -proc ps7_cortexa9_0 -lang c -bsp bsp
configapp -app sw -set build-config debug
importsources -name sw -linker-script -path sources/sdk



projects -build



exit











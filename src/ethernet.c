#include "ethernet.h"
#include "utils.h"
#include "driver.h"
#include "arp.h"
#include "ip.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 处理一个收到的数据包
 *        你需要判断以太网数据帧的协议类型，注意大小端转换
 *        如果是ARP协议数据包，则去掉以太网包头，发送到arp层处理arp_in()
 *        如果是IP协议数据包，则去掉以太网包头，发送到IP层处理ip_in()
 * 
 * @param buf 要处理的数据包
 */
void ethernet_in(buf_t *buf)
{
	char *ethernet_head = buf-> data;
	char *mac_head = ethernet_head ;
	unsigned char *p = mac_head + 12;//type
	if(p[0] == 0x08 && p[1] == 0x00){
		//根据类型判断：ip
		buf_remove_header(buf,14);
		ip_in(buf);
	}
	else if(p[0] == 0x08 && p[1] == 0x06){
		//arp
		buf_remove_header(buf,14);
		arp_in(buf);
	}
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你需添加以太网包头，填写目的MAC地址、源MAC地址、协议类型
 *        添加完成后将以太网数据帧发送到驱动层
 * 
 * @param buf 要处理的数据包
 * @param mac 目标mac地址
 * @param protocol 上层协议
 */
void ethernet_out(buf_t *buf, const uint8_t *mac, net_protocol_t protocol)
{
	buf_add_header(buf,14);
	char *ethernet_head = buf-> data;
	unsigned char *p = ethernet_head;
	for(int i = 0;i < 6;i ++){
		p[i] = mac[i];
	}
	p = p + 6;
	for(int i = 0;i < 6;i ++){
		p[i] = (uint8_t)net_if_mac[i];
	}
	p[6] = (protocol & 0xff00) >> 8;
	p[7] = (protocol & 0x00ff);
	driver_send(buf);

}

/**
 * @brief 初始化以太网协议
 * 
 * @return int 成功为0，失败为-1
 */
int ethernet_init()
{
    buf_init(&rxbuf, ETHERNET_MTU + sizeof(ether_hdr_t));
    return driver_open();
}

/**
 * @brief 一次以太网轮询
 * 
 */
void ethernet_poll()
{
    if (driver_recv(&rxbuf) > 0)
        ethernet_in(&rxbuf);
}

#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief 初始的arp包
 * 
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type = swap16(ARP_HW_ETHER),
    .pro_type = swap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = DRIVER_IF_IP,
    .sender_mac = DRIVER_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表
 * 
 */
arp_entry_t arp_table[ARP_MAX_ENTRY];

/**
 * @brief 长度为1的arp分组队列，当等待arp回复时暂存未发送的数据包
 * 
 */
arp_buf_t arp_buf;

/**
 * @brief 更新arp表
 *        你首先需要依次轮询检测ARP表中所有的ARP表项是否有超时，如果有超时，则将该表项的状态改为无效。
 *        接着，查看ARP表是否有无效的表项，如果有，则将arp_update()函数传递进来的新的IP、MAC信息插入到表中，
 *        并记录超时时间，更改表项的状态为有效。
 *        如果ARP表中没有无效的表项，则找到超时时间最长的一条表项，
 *        将arp_update()函数传递进来的新的IP、MAC信息替换该表项，并记录超时时间，设置表项的状态为有效。
 * 
 * @param ip ip地址
 * @param mac mac地址
 * @param state 表项的状态
 */
void arp_update(uint8_t *ip, uint8_t *mac, arp_state_t state)
{
    time_t t_now = time(NULL);//获取当前的时间
    for(int i = 0;i < ARP_MAX_ENTRY;i ++){
        t_now = time(NULL);
        if((t_now - arp_table[i].timeout) > ARP_TIMEOUT_SEC){
            //查看是否有超时的表项
            arp_table[i].state = ARP_INVALID;
        }
    }
    int flag = 0;
    for(int i = 0;i < ARP_MAX_ENTRY;i ++){
        if(arp_table[i].state == ARP_INVALID){
            t_now = time(NULL);
            memcpy(arp_table[i].ip,ip,NET_IP_LEN);
            memcpy(arp_table[i].mac,mac,NET_MAC_LEN);
            arp_table[i].state = state;
            arp_table[i].timeout = t_now;
            flag = 1;
            break;
        }
    }
    //没有无效的表项，找时间最长的哪一项
    if(flag == 0){
        int k = 0;
        int max = 0;
        for(int i = 0;i < ARP_MAX_ENTRY;i ++){
            t_now = time(NULL);
            if((t_now - arp_table[i].timeout) > max){
                max = t_now - arp_table[i].timeout;
                k = i;
            }
        }
        //已经选出最大的了
        memcpy(arp_table[k].ip,ip,NET_IP_LEN);
        memcpy(arp_table[k].mac,mac,NET_MAC_LEN);
        arp_table[k].state = state;
        arp_table[k].timeout = t_now;
    }

}

/**
 * @brief 从arp表中根据ip地址查找mac地址
 * 
 * @param ip 欲转换的ip地址
 * @return uint8_t* mac地址，未找到时为NULL
 */
static uint8_t *arp_lookup(uint8_t *ip)
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        if (arp_table[i].state == ARP_VALID && memcmp(arp_table[i].ip, ip, NET_IP_LEN) == 0)
            return arp_table[i].mac;
    return NULL;
}

/**
 * @brief 发送一个arp请求
 *        你需要调用buf_init对txbuf进行初始化
 *        填写ARP报头，将ARP的opcode设置为ARP_REQUEST，注意大小端转换
 *        将ARP数据报发送到ethernet层
 * 
 * @param target_ip 想要知道的目标的ip地址
 */
static void arp_req(uint8_t *target_ip)
{
    //init 初始化
    buf_init(&txbuf, sizeof(arp_pkt_t));

    //填写ARP报头s
    arp_pkt_t *arp = (arp_pkt_t *)(&txbuf) -> data;
    *arp = arp_init_pkt;
    memcpy(arp -> target_ip,target_ip,NET_IP_LEN);
    arp -> opcode = swap16(ARP_REQUEST);

    //将数据发送到ethernet层
    ethernet_out(&txbuf,request_mac,NET_PROTOCOL_ARP);


}


/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，查看报文是否完整，
 *        检查项包括：硬件类型，协议类型，硬件地址长度，协议地址长度，操作类型
 *        
 *        接着，调用arp_update更新ARP表项
 *        查看arp_buf是否有效，如果有效，则说明ARP分组队列里面有待发送的数据包。
 *        即上一次调用arp_out()发送来自IP层的数据包时，由于没有找到对应的MAC地址进而先发送的ARP request报文
 *        此时，收到了该request的应答报文。然后，根据IP地址来查找ARM表项，如果能找到该IP地址对应的MAC地址，
 *        则将缓存的数据包arp_buf再发送到ethernet层。
 * 
 *        如果arp_buf无效，还需要判断接收到的报文是否为request请求报文，并且，该请求报文的目的IP正好是本机的IP地址，
 *        则认为是请求本机MAC地址的ARP请求报文，则回应一个响应报文（应答报文）。
 *        响应报文：需要调用buf_init初始化一个buf，填写ARP报头，目的IP和目的MAC需要填写为收到的ARP报的源IP和源MAC。
 * 
 * @param buf 要处理的数据包
 */
void arp_in(buf_t *buf)
{
    arp_pkt_t *arp = (arp_pkt_t *)buf -> data;
    int opcode = swap16(arp -> opcode);
    if(arp -> hw_type != swap16(ARP_HW_ETHER)
    || arp -> pro_type != swap16(NET_PROTOCOL_IP)
    || arp -> hw_len != NET_MAC_LEN
    || arp -> pro_len != NET_IP_LEN
    || (opcode != ARP_REQUEST && opcode != ARP_REPLY)
    )
    return ;
    arp_update(arp -> sender_ip,arp -> sender_mac, ARP_VALID);
    if(arp_buf.valid == 1){
        for(int i = 0;i < ARP_MAX_ENTRY;i ++){
            if(arp_table[i].state == ARP_VALID){
                if(memcmp(arp_table[i].ip,arp_buf.ip,NET_IP_LEN) == 0){
                    arp_buf.valid = 0;
                    ethernet_out(&arp_buf.buf,arp_table[i].mac,arp_buf.protocol);
                    return;
                }

            }
        }
    }
    else{
        if(opcode == ARP_REQUEST){
            if(memcmp(arp -> target_ip,net_if_ip,NET_IP_LEN) == 0){
                static buf_t buf_new;
                buf_init(&buf_new, sizeof(arp_pkt_t));
                arp_pkt_t *arp_new = (arp_pkt_t *)(&buf_new) -> data;
                *arp_new = arp_init_pkt;
                arp_new->opcode = swap16(ARP_REPLY);
                memcpy(arp_new->target_ip, arp->sender_ip, NET_IP_LEN);
                memcpy(arp_new->target_mac, arp->sender_mac, NET_MAC_LEN);
                ethernet_out(&buf_new, arp_new->target_mac, NET_PROTOCOL_ARP);
            }
        }
    }
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你需要根据IP地址来查找ARP表
 *        如果能找到该IP地址对应的MAC地址，则将数据报直接发送给ethernet层
 *        如果没有找到对应的MAC地址，则需要先发一个ARP request报文。
 *        注意，需要将来自IP层的数据包缓存到arp_buf中，等待arp_in()能收到ARP request报文的应答报文
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void arp_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    //根据IP地址来查找ARP表
    for(int i = 0;i < ARP_MAX_ENTRY;i ++){
        if(memcmp(ip,arp_table[i].ip,NET_IP_LEN) == 0){
            if(arp_table[i].state == ARP_VALID){
                //能找到该IP地址对应的MAC地址，将该数据包直接发给ethernet层
                ethernet_out(buf,arp_table[i].mac,protocol);
                return ;
            }
        }
    }
    //没有找到，发送request报文
    
    arp_req(ip);
    if(protocol == NET_PROTOCOL_IP){
        //需要将来自ip层的数据包缓存到arp_buf中
        arp_buf.buf = *buf;
        memcpy(&arp_buf.ip,ip,NET_IP_LEN);
        arp_buf.protocol = protocol;
        arp_buf.valid = 1;
    }
    

}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init()
{
    for (int i = 0; i < ARP_MAX_ENTRY; i++)
        arp_table[i].state = ARP_INVALID;
    arp_buf.valid = 0;
    arp_req(net_if_ip);
}
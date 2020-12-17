#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include <string.h>

/**
 * @brief 处理一个收到的数据包
 *        你首先需要做报头检查，检查项包括：版本号、总长度、首部长度等。
 * 
 *        接着，计算头部校验和，注意：需要先把头部校验和字段缓存起来，再将校验和字段清零，
 *        调用checksum16()函数计算头部检验和，比较计算的结果与之前缓存的校验和是否一致，
 *        如果不一致，则不处理该数据报。
 * 
 *        检查收到的数据包的目的IP地址是否为本机的IP地址，只处理目的IP为本机的数据报。
 * 
 *        检查IP报头的协议字段：
 *        如果是ICMP协议，则去掉IP头部，发送给ICMP协议层处理
 *        如果是UDP协议，则去掉IP头部，发送给UDP协议层处理
 *        如果是本实验中不支持的其他协议，则需要调用icmp_unreachable()函数回送一个ICMP协议不可达的报文。
 *          
 * @param buf 要处理的包
 */
void ip_in(buf_t *buf)
{
    //收到一个处理包
    ip_hdr_t *ip_header = (ip_hdr_t *)buf -> data;
    //做报头检查
    if((ip_header -> version != IP_VERSION_4) 
    ||((ip_header -> hdr_len) * IP_HDR_LEN_PER_BYTE < sizeof(ip_hdr_t)) 
    ||(ip_header -> total_len < sizeof(ip_hdr_t))
    )
        return ;

    //计算头部校验和
    uint16_t temp_checksum = ip_header ->hdr_checksum;
    ip_header -> hdr_checksum = 0;
    uint16_t count_checksum = checksum16(buf,sizeof(ip_hdr_t));
    
    if(temp_checksum != count_checksum){
        //如果计算结果与之前的结果不一致，不处理
        return ;
    }
    //检查收到的数据包的目的ip地址是否为本机ip地址,只处理目的ip为本机ip地址的

    if(memcmp(ip_header ->dest_ip,net_if_ip,NET_IP_LEN) != 0){
        return ;
    }
    
    //检查ip报头的协议字段
    uint8_t src_ip_temp[NET_IP_LEN]; 
    memcpy(src_ip_temp,(ip_header -> src_ip),NET_IP_LEN);
    if((ip_header -> protocol) == NET_PROTOCOL_ICMP ){
        //如果是ICMP协议，则去掉IP头部，发送给ICMP协议层处理
        buf_remove_header(buf,(ip_header -> hdr_len)*IP_HDR_LEN_PER_BYTE);
        icmp_in(buf, src_ip_temp);
    }
    else if((ip_header -> protocol) == NET_PROTOCOL_UDP){
       //如果是UDP协议，则去掉IP头部，发送给UDP协议层处理
        buf_remove_header(buf,(ip_header -> hdr_len)*IP_HDR_LEN_PER_BYTE);
        udp_in(buf, src_ip_temp);
    }
    else{
        //如果是本实验中不支持的其他协议，则需要调用icmp_unreachable()函数回送一个ICMP协议不可达的报文。
        icmp_unreachable(buf,src_ip_temp,ICMP_CODE_PORT_UNREACH);
    }


}

/**
 * @brief 处理一个要发送的分片
 *        你需要调用buf_add_header增加IP数据报头部缓存空间。
 *        填写IP数据报头部字段。
 *        将checksum字段填0，再调用checksum16()函数计算校验和，并将计算后的结果填写到checksum字段中。
 *        将封装后的IP数据报发送到arp层。
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf)
{
    //调用buf_add_header增加ip数据报头部缓存空间
    buf_add_header(buf,sizeof(ip_hdr_t));
    //填写ip的头部字段
    ip_hdr_t *ip_header = (ip_hdr_t *)buf -> data;

    ip_header -> hdr_len = sizeof(ip_hdr_t)/IP_HDR_LEN_PER_BYTE;
    ip_header -> version = IP_VERSION_4;
    ip_header -> tos = 0;
    ip_header -> total_len = swap16(buf ->len);
    ip_header -> id = swap16(id);
    ip_header -> flags_fragment = (mf ? IP_MORE_FRAGMENT : 0) | swap16(offset);
    ip_header -> ttl = IP_DEFALUT_TTL;
    ip_header -> protocol = protocol;
    ip_header -> hdr_checksum = 0;
    ip_header -> hdr_checksum = checksum16(ip_header,sizeof(ip_hdr_t));
    memcpy(ip_header -> src_ip,net_if_ip,NET_IP_LEN);
    memcpy(ip_header -> dest_ip,ip,NET_IP_LEN);
    arp_out(buf,ip,NET_PROTOCOL_IP);
    
}

/**
 * @brief 处理一个要发送的数据包
 *        你首先需要检查需要发送的IP数据报是否大于以太网帧的最大包长（1500字节 - ip包头长度）。
 *        
 *        如果超过，则需要分片发送。 
 *        分片步骤：
 *        （1）调用buf_init()函数初始化buf，长度为以太网帧的最大包长（1500字节 - ip包头头长度）
 *        （2）将数据报截断，每个截断后的包长度 = 以太网帧的最大包长，调用ip_fragment_out()函数发送出去
 *        （3）如果截断后最后的一个分片小于或等于以太网帧的最大包长，
 *             调用buf_init()函数初始化buf，长度为该分片大小，再调用ip_fragment_out()函数发送出去
 *             注意：最后一个分片的MF = 0
 *    
 *        如果没有超过以太网帧的最大包长，则直接调用调用ip_fragment_out()函数发送出去。
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol)
{
    int max_len = ETHERNET_MTU - sizeof(ip_hdr_t);
    
    if(buf -> len > max_len){
        //如果超过，采取分片发送
        static buf_t ip_buf;;
        //调用buf_init()函数初始化buf，长度为以太网帧的最大包长（1500字节 - ip包头头长度）
        buf_init(&ip_buf, max_len);
        //将数据包长截断
        uint16_t offset = 0;
        int id = 0;
        int len;
        for(len = buf -> len;len > max_len;len --){
            //要留出最后一块，来将他的Mf改成0
            ip_buf.len = max_len;
            ip_buf.data = buf -> data;
            ip_fragment_out(&ip_buf,ip,protocol,id,offset,1);
            buf -> data += max_len;
            offset += max_len / 8;
        }
        if(len > 0){
            ip_buf.len = len;
            ip_buf.data = buf -> data;
            ip_fragment_out(&ip_buf,ip,protocol,id,offset,0);
    }
    else{
        //如果没有超过最大包长，直接调用它将它发送出去
        ip_fragment_out(buf,ip,protocol,0,0,0);
    }
    
}

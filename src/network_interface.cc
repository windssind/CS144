#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address ),ARP(),waitingList(),SendingFrames(),ARPTimerPass()
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  EthernetFrame frame;
  // 先查询是否在ARP表中
  EthernetAddress next_hop_Ethernet;
  EthernetAddress src = this->ethernet_address_;
  uint32_t next_Addr = next_hop.ipv4_numeric();
  auto it  = ARP.find(next_Addr); // find 返回的是一个迭代器
  if (it==ARP.end()){//mac表没找到并且5s之内没有发送过相同的message，然后构建ARP信息，准备返回，然后直接返回
    //如果5s内已经发过去了，就不要再发了
    if(ARPTimerPass.find(next_Addr)!=ARPTimerPass.end()) return ;
    
    waitingList.insert(std::make_pair(next_Addr,dgram));
    frame.header={ETHERNET_BROADCAST,src,EthernetHeader::TYPE_ARP};
    ARPMessage ARPmsg {.opcode = ARPMessage::OPCODE_REQUEST,
                      .sender_ethernet_address = src,
                      .sender_ip_address=ip_address_.ipv4_numeric(),
                      .target_ip_address= next_Addr};
    frame.payload = serialize(ARPmsg);
    ARPTimerPass.insert(std::make_pair(next_Addr,0));//给发送的也加上
    SendingFrames.push(frame);
    return ; 
  }
  next_hop_Ethernet = it->second.address;
  //然后构建信息，将这个加入待发射的表中
  frame.header = {next_hop_Ethernet,src,EthernetHeader::TYPE_IPv4};
  frame.payload = serialize(dgram);//或者是parse（）然后再serialize
  SendingFrames.push(frame);
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{ 
  EthernetAddress src_EthernetAddress = frame.header.src;
  EthernetAddress dst_EthernetAddress = frame.header.dst;
  bool isToMe = ethernet_address_ ==dst_EthernetAddress;
  bool isBroadcast = dst_EthernetAddress == ETHERNET_BROADCAST;
  bool isIPv4= frame.header.type == EthernetHeader::TYPE_IPv4;
  bool isARP = frame.header.type == EthernetHeader::TYPE_ARP;
  // 将这个常用的条件直接先弄出来
  if (isToMe && isIPv4){
    IPv4Datagram recv ;
    if(!parse(recv,frame.payload)){
        cerr<< "parse frame fail (IPV4)"<<endl;
        exit(0);
      }
    return recv;
  }else if(isARP && (isToMe || isBroadcast)){ // 那这个就是收到ARP了
    ARPMessage recv_ARP ;
    parse(recv_ARP,frame.payload);
    bool isBrocastToMe = recv_ARP.target_ip_address == ip_address_.ipv4_numeric();
    //受到ARP消息，无论是reply还是request，都要更新
    if (isBroadcast && isBrocastToMe){ // Request
      ARP.insert(std::make_pair(recv_ARP.sender_ip_address,EthernetAddressWithTime{recv_ARP.sender_ethernet_address,0}));
      SendARP(src_EthernetAddress,ARPMessage::OPCODE_REPLY,recv_ARP.sender_ip_address);
    }else if (!isBroadcast && isToMe){//Reply
      ARP.insert(std::make_pair(recv_ARP.sender_ip_address,EthernetAddressWithTime{recv_ARP.sender_ethernet_address,0}));
    }
    // 看更新后的表，如果能找到对应的，就将原来的加入sendingframes
    for (auto it = waitingList.begin();it!=waitingList.end();){
      IP ip = it->first;
      if (ip == recv_ARP.sender_ip_address){
        // 创建一个新的frame
        EthernetFrame retrans_frame;
        retrans_frame.header.src=ethernet_address_;
        retrans_frame.header.dst = ARP[ip].address;
        retrans_frame.header.type = EthernetHeader::TYPE_IPv4;
        retrans_frame.payload = serialize(it->second);
        SendingFrames.push(retrans_frame);
        waitingList.erase(it++);// 消除
      }else it++;
    }
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  //先对ARP表中的数值进行更新
  for (auto pair = ARP.begin();pair!=ARP.end();){
    pair->second.timePass += ms_since_last_tick;
    if ( pair->second.timePass >= 30000){
      ARP.erase(pair++);
    }else pair++;
  }

  for (auto pair = ARPTimerPass.begin();pair!=ARPTimerPass.end();){
    pair->second += ms_since_last_tick;
    if ( pair->second >= 5000){
      ARPTimerPass.erase(pair++);
    } else pair++;
  }
  // 然后对ARPTimePass

}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (SendingFrames.size() > 0){
    EthernetFrame tmp = SendingFrames.front();
    SendingFrames.pop();
    return tmp;
  }else {
    return nullopt;
  }
  
}

void NetworkInterface:: SendARP(EthernetAddress receiver_ethernet,uint16_t OPCODE ,IP target_ip){
  EthernetFrame frame;
  frame.header.type = EthernetHeader::TYPE_ARP;
  frame.header.src = ethernet_address_;
  frame.header.dst = receiver_ethernet;
  ARPMessage ARPMsg={.opcode = OPCODE,
    .sender_ethernet_address = ethernet_address_,
    .sender_ip_address = ip_address_.ipv4_numeric(),
    .target_ethernet_address = receiver_ethernet,
    .target_ip_address = target_ip,
  };
  frame.payload = serialize(ARPMsg);
  SendingFrames.push(frame);
}



//需要额外维护一个5s内已经发送过的ARP请求的IP地址？
//或者可以维护一个
// parse函数和serialize函数 , 先用serilize函数将所有的都聚合，再转化成frame
// TODO:修改recv的结构，查看有无判断两个ethernet__address相同的方法
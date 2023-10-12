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
  if (it==ARP.end() && ARPTimerPass.find(next_Addr)==ARPTimerPass.end()){//mac表没找到并且5s之内没有发送过相同的message，然后构建ARP信息，准备返回，然后直接返回
    waitingList.insert(std::make_pair(next_Addr,dgram));
    frame.header={ETHERNET_BROADCAST,src,EthernetHeader::TYPE_ARP};
    ARPMessage ARPmsg {.opcode = ARPMessage::OPCODE_REQUEST,
                      .sender_ethernet_address = src,
                      .sender_ip_address=dgram.header.src,
                      .target_ip_address= dgram.header.dst};
    if(!parse(ARPmsg,frame.payload)){
      cerr<<"parse failed"<<endl;
      exit(0);
    }
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
  if (std::equal(dst_EthernetAddress.begin(),dst_EthernetAddress.end(),ETHERNET_BROADCAST.begin(),ETHERNET_BROADCAST.end())){
    // 等于广播地址，返回一个ARPMESSAGE
    //广播地址的header那些信息在payload里面，这里写错了
    //还要判断这个广播是不是给自己的
    //TODO：怎么获取广播中的ARPMSG的内容呢？
    ARPMessage recv_ARP ;
    if(!parse(recv_ARP,frame.payload)){
      cerr<<"parse ARP failed"<<endl;
    }
    ARP.insert(std::make_pair(recv_ARP.sender_ip_address,recv_ARP.sender_ethernet_address));// ARP消息无论是reply还是request都要维护
    //request需要返回一个ARP信息
    if (recv_ARP.opcode == ARPMessage::OPCODE_REQUEST){ 
      ARPMessage send_ARP;
      EthernetFrame ret_Msg ;
      //维护映射的地址
      send_ARP = {.opcode = ARPMessage::OPCODE_REPLY,
            .sender_ethernet_address = ethernet_address_,
            .sender_ip_address = ip_address_.ipv4_numeric(),
            .target_ethernet_address = recv_ARP.sender_ethernet_address,
            .target_ip_address = recv_ARP.sender_ip_address
      };
      ret_Msg.header = {src_EthernetAddress,dst_EthernetAddress,EthernetHeader::TYPE_ARP};
      ret_Msg.payload = serialize(send_ARP);
      SendingFrames.push(ret_Msg);
    }
    // 看更新后的表，如果能找到对应的，就将原来的加入sendingframes
    for (auto &pair : waitingList){
      if (pair.first == recv_ARP.sender_ip_address){
        // 创建一个新的frame
        EthernetFrame retrans_frame;
        /*frame.header ={
          .src = ethernet_address_,
          .dst = recv_ARP.dst,
          .type = EthernetHeader::TYPE_IPv4,
        };*/
        parse(pair.second , retrans_frame.payload);
        SendingFrames.push(retrans_frame);
        waitingList.erase(pair.first);// 消除
      }
    }
    //如果是IPV4帧并且是发送给我的话的话，直接将这个传输给上层就行了
  }else if (std::equal(dst_EthernetAddress.begin(),dst_EthernetAddress.end(),ethernet_address_.begin(),ethernet_address_.end()) && frame.header.type == EthernetHeader::TYPE_IPv4){
    IPv4Datagram recv ;
    if(!parse(recv,frame.payload)){
      cerr<< "parse frame fail (IPV4)"<<endl;
      exit(0);
    }
    return recv;
  }
  return nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  //先对ARP表中的数值进行更新
  for (auto &pair : ARP){
    pair.second.timePass += ms_since_last_tick;
    if (pair.second.timePass >= 30000){
      ARP.erase(pair.first);
    }
  }
  // 然后对ARPTimePass
  for (auto &pair :ARPTimerPass){
    pair.second += ms_since_last_tick;
    if (pair.second>=5000){
      ARPTimerPass.erase(pair.first);
    }
  }
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

//需要额外维护一个5s内已经发送过的ARP请求的IP地址？
//或者可以维护一个

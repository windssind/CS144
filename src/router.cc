#include "router.hh"
#include "address.hh"
#include <iostream>
#include <limits>
#include <optional>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  routing_table.push_back(Entry{interface_num,next_hop , prefix_length,route_prefix});
  sort(routing_table.begin(),routing_table.end());
}

void Router::route() {
  // Todo : Finish route
  int num_Of_Interfaces = interfaces_.size();
  for (int i = 0;i<num_Of_Interfaces;++i){  
    while (true){
      optional <InternetDatagram> dgram = interfaces_[i].maybe_receive(); 
      if (!dgram.has_value()) break; // 如果没有东西了，就跳出循环，寻找下一个物理接口
      else{
        InternetDatagram& internetdgram = dgram.value();
        if (internetdgram.header.ttl <= 1 ) continue;
        int len = routing_table.size();
        for (int j= 0 ;j < len ;++j){
          int ret_interface_num;
          Entry entry = routing_table[j];
          if (((ret_interface_num=Match(internetdgram.header.dst , entry))>=0)){
            internetdgram.header.ttl--;
            internetdgram.header.compute_checksum();
            Address next_hop = entry.next_hop.value_or(Address::from_ipv4_numeric(internetdgram.header.dst)); 
            interfaces_[ret_interface_num].send_datagram(internetdgram,next_hop); // 如果next_hop是空的话，应该返回什么呢？ next_hop为空代表自己就是最终的
            break;
          }
        }  
      }
    }    
  }
}

Router::Router():interfaces_(),routing_table(){};

int Router::Match(uint32_t ip_address , Entry entry) {
  if (entry.prefix_length == 0) return entry.interface_num; 
  int32_t ip_prefix = ip_address & ((int)0X80000000>>(entry.prefix_length - 1));
  int32_t entry_prefix = entry.prefix & ((int)0X80000000>>(entry.prefix_length - 1));
  if (ip_prefix == entry.prefix) return entry.interface_num;
  else return -1;
}

// todo:为什么会有那么多相同的interface呢?
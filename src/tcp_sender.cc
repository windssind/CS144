#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

#define min (x,y)  (x)<(y)?(x):(y)
using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ),
  retransmit_Num(0),outStandingSegs()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return outStandingSegs.size();
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmit_Num ; 
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (!data.has_value()) return nullopt ;
  return TCPSenderMessage{seqno,SYN,payload,FIN}; // 直接用聚合初始化的方法来返回
}

void TCPSender::push( Reader& outbound_stream )
{
  uint64_t bufferNum = outbound_stream.bytes_buffered();
  uint64_t Max_Bytes ; //最大传输值为windowsize和现有的bufferNum的较小值
  if (window_Size == 1) Max_Bytes= 1;
  else Max_Bytes= min(bufferNum,window_Size); //如果窗口满了，就默认值为1
  for(int i=0;i<Max_Bytes;){
    uint64_t Segbytes= min(Max_Bytes - i,TCPConfig::MAX_PAYLOAD_SIZE); ; //这是组成这个分组需要的bytes数
    string data ;
    data.push_back(Segbytes);
    payload = Buffer (data);
    i+=Segbytes;
  }
  outStandingSegs.push (TCPSenderMessage{seqno,SYN,payload,FIN}); //将这个加入等待列表
  // 考虑
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  // Your code here.
  return {};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  (void)ms_since_last_tick;
}

#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

#define min(x,y)  (x)<(y)?(x):(y)
using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ),
  retransmit_Num(0),timePass(0),window_Size(1),nowIndex(0),seqno(isn_),SYN(true),FIN(false),timer(),payload(),outStandingSegs()
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
  //if (!payload.has_value()) return nullopt ;
  return TCPSenderMessage{seqno,SYN,payload,FIN}; // 直接用聚合初始化的方法来返回
}

void TCPSender::push( Reader& outbound_stream )
{
  uint64_t bufferNum = outbound_stream.bytes_buffered();
  uint64_t Max_Bytes ; //最大传输值为windowsize和现有的bufferNum的较小值
  if (window_Size == 1) Max_Bytes= 1;
  else Max_Bytes= min(bufferNum,window_Size); //如果窗口满了，就默认值为1
  for(uint64_t i=0;i<Max_Bytes;){
    uint64_t Segbytes= min(Max_Bytes - i,TCPConfig::MAX_PAYLOAD_SIZE); ; //这是组成这个分组需要的bytes数
    string data ;
    for (uint64_t j =0;j<Segbytes;++j){
      data.push_back(*(outbound_stream.peek().data()));
      outbound_stream.pop(1);
    }
    
    payload = Buffer (data);
    i+=Segbytes;
    if (outbound_stream.is_finished()) FIN = true ; 
    outStandingSegs.push (maybe_send().value()); //将这个加入等待列表
    if (!timer.isOn()) timer.start(initial_RTO_ms_);
  }
  
  // 考虑定时器
  if (timer.isTimeOut(timePass)){
    seqno = outStandingSegs.front().seqno; 
    SYN = outStandingSegs.front().SYN;
    payload = outStandingSegs.front().payload;
    FIN = outStandingSegs.front().FIN ;
    maybe_send();
    // reset RTO
    timer.duoble_RTO();
    timePass=0;
    retransmit_Num++;
  }
  // 如果oustanding里面没有东西了，就停止timer
  if (outStandingSegs.size() == 0) timer.stop();
  //重置两个标志为
  SYN = false;
  FIN = false;
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage{seqno,SYN,Buffer(),FIN};  // 怎么构建空字符串呢？
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  uint64_t delta = msg.ackno.value().unwrap(isn_,nowIndex)
                  - seqno.unwrap(isn_,nowIndex); // 看看确认号和seqno有变化没
  seqno = msg.ackno.value();
  nowIndex+=delta; // 及时更新nowIndex，便于后面unwrap
  window_Size = msg.window_size;
  // 再清除oustanding中的已经被确认的分组
  if (delta > 0){
    for (uint64_t i=0;i<delta;++i){
      outStandingSegs.pop();
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  timePass+=ms_since_last_tick;
}

//TODO:fixed all the bugs
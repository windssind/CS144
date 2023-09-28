#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

#define min(x,y)  (x)<(y)?(x):(y)
using namespace std;



bool    Timer:: isTimeOut(uint64_t timePass){
      return timePass >= RTO && on ;
    }

void    Timer:: duoble_RTO(){
      this->RTO = RTO+RTO ; 
    }
void     Timer::start(uint64_t RTO_){
      this->RTO = RTO_;
      this->on = true;
    }
void    Timer:: stop(){
      this->on=false;
    }
bool    Timer:: isOn(){
      return on;
    }
void     Timer::set(size_t initial_RTO_ms){
      RTO = initial_RTO_ms ;
    }

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ),
  retransmit_Num(0),timePass(0),window_Size(1),nowIndex(0),seqnoInFlight(0),send_base(isn_),nextSeqno(isn_), SYN(true),FIN(false),hasConnected(false),timer(),outStandingSegs(),Msgs()
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return seqnoInFlight ; 
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmit_Num ; 
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  //if (!payload.has_value()) return nullopt ;
  if (!Msgs.empty()){
    TCPSenderMessage retMsg = Msgs.front();
    Msgs.pop_front();
    return retMsg; // 直接用聚合初始化的方法来返回
  }else {
    return nullopt;
  }
  
}

void TCPSender::push( Reader& outbound_stream )
{
  if (!hasConnected) {
    SYN=true;
    hasConnected = true;
  }else {
    SYN=false;
  }
  uint64_t bufferNum = outbound_stream.bytes_buffered();
  uint64_t Max_Bytes ; //最大传输值为windowsize和现有的bufferNum的较小值
  if (window_Size == 1) Max_Bytes= 1;
  else Max_Bytes= min(bufferNum,window_Size); //如果窗口满了，就默认值为1
  for(uint64_t i=0;i<Max_Bytes;){
    uint64_t Segbytes= min(Max_Bytes - i,TCPConfig::MAX_PAYLOAD_SIZE); ; //这是组成这个分组需要的bytes数
    string data ;
    for (uint64_t j =0;j<Segbytes;++j){
      data.append(outbound_stream.peek());
      outbound_stream.pop(1);
    } // 构造data
    if (outbound_stream.is_finished()) FIN = true ;
    TCPSenderMessage msgWaitForAcked = TCPSenderMessage{nextSeqno,SYN,Buffer(data),FIN};
    uint64_t msgLen = msgWaitForAcked.sequence_length();
    Msgs.push_back(msgWaitForAcked);
    nextSeqno= nextSeqno + msgLen; // seqno 递增
    window_Size -= msgLen;
    // 这个是outstanding
    outStandingSegs.push_back(msgWaitForAcked); //将这个加入等待列表
    seqnoInFlight += msgLen ; 
    if (!timer.isOn()) timer.start(initial_RTO_ms_);
    i+=Segbytes;
  }
  // 修改完毕
  // 考虑定时器
  /*if (timer.isTimeOut(timePass)){
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
  FIN = false;*/
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage{nextSeqno,false,Buffer(),false};  // 怎么构建空字符串呢？
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  uint64_t delta = msg.ackno.value().unwrap(isn_,nowIndex)
                  - send_base.unwrap(isn_,nowIndex); // 看看确认号和seqno有变化没
  send_base = msg.ackno.value();
  nowIndex+=delta; // 及时更新nowIndex，便于后面unwrap
  window_Size = msg.window_size;
  // 再清除oustanding中的已经被确认的分组
  if (delta > 0){
    for (uint64_t i=0;i<delta;){
      uint64_t ackBytes = outStandingSegs.front().sequence_length();
      seqnoInFlight -= ackBytes ; 
      outStandingSegs.pop_front();
      i+=ackBytes ;
    }
    timer.set(initial_RTO_ms_);
    if (outStandingSegs.size() == 0) timer.stop();
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if (timer.isOn()){
    timePass+=ms_since_last_tick;
  }
  if (timer.isTimeOut(timePass)){
    timePass = 0;
    Msgs.push_front(outStandingSegs.front());
    timer.duoble_RTO();
  }
  // 需要在这里进行重传信息
}

//TODO:为什么第一条信息的size为1,data初始化的时候length就是0,在stream peek之后才修改为了length=1,为什么呢？这是由C++的一些特性决定的，回去看看peek函数
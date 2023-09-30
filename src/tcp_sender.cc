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
  retransmit_Num(0),timePass(0),window_Size(1),left_Window_Size(1),nowIndex(0),seqnoInFlight(0),send_base(isn_),nextSeqno(isn_),hasConnected(false),hasClosed(false),hasSendFIN(false),timer(),outStandingSegs(),Msgs()
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
  if (Msgs.empty()) return nullopt;
  //if (msgLen > window_Size) return nullopt ;
  // 可以正常发送
  TCPSenderMessage retMsg = Msgs.front();
  Msgs.pop_front();
  return retMsg; // 直接用聚合初始化的方法来返回
}

void TCPSender::push( Reader& outbound_stream )
{
  if (outbound_stream.is_finished()) hasClosed =true;
  while ((left_Window_Size >0 && outbound_stream.bytes_buffered()>0) || !hasConnected || (hasClosed&&!hasSendFIN &&left_Window_Size>0)){
    bool SYN = hasConnected ? false : true ; 
    left_Window_Size-=SYN;
    string data;
   for (uint64_t i=0;i< TCPConfig::MAX_PAYLOAD_SIZE ;++i ){
      if (outbound_stream.bytes_buffered()==0 || left_Window_Size ==0) break;
      data.append(outbound_stream.peek());
      outbound_stream.pop(1);
      left_Window_Size--;
   } 
    if (outbound_stream.is_finished()) hasClosed =true;
    bool FIN = hasClosed && left_Window_Size > 0 ;
    left_Window_Size-=FIN;
    TCPSenderMessage msgWaitForAcked = TCPSenderMessage{nextSeqno,SYN,Buffer(data),FIN};
    if (!hasConnected && SYN) hasConnected = true ; 
    if (FIN) hasSendFIN = true ;
    uint64_t msgLen = msgWaitForAcked.sequence_length();
    seqnoInFlight += msgLen ;
    Msgs.push_back(msgWaitForAcked);
    outStandingSegs.push_back(msgWaitForAcked);
    nextSeqno= nextSeqno + msgLen; // seqno 递增
    // 这个是outstanding
    if (!timer.isOn()) timer.start(initial_RTO_ms_);
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage{nextSeqno,false,Buffer(),false};  // 怎么构建空字符串呢？
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (!msg.ackno.has_value()) {
    window_Size = msg.window_size;
    left_Window_Size = window_Size == 0 ? 1 :window_Size ;// 这个对吗？没有是什么情况
    return;
  }
  uint64_t recv_Index = msg.ackno.value().unwrap(isn_,nowIndex);
  uint64_t send_Index = send_base.unwrap(isn_,nowIndex); // 看看确认号和seqno有变化没
  uint64_t next_Index = nextSeqno.unwrap(isn_,nowIndex);
  if (recv_Index <send_Index) return ;// 没用的信息                
  if (recv_Index > next_Index) return ; //这个更离谱，直接超过了原来的
  if (recv_Index + msg.window_size <= next_Index && msg.window_size !=0){
    send_base = msg.ackno.value();
    return ;
    // left_Window_Size保持不变
  }
  uint64_t delta = recv_Index - send_Index;
  send_base = msg.ackno.value();
  window_Size = msg.window_size ;
  nowIndex+=delta; // 及时更新nowIndex，便于后面unwrap
  left_Window_Size = window_Size==0? 1 : recv_Index + window_Size - next_Index;
  // 再清除oustanding中的已经被确认的分组
  if (delta > 0){
    while (true){
      if (outStandingSegs.empty()) break;
      TCPSenderMessage front =  outStandingSegs.front();
      uint64_t front_Size = front.sequence_length();
      if (recv_Index >= front.seqno.unwrap(isn_,nowIndex) + front_Size){
          seqnoInFlight -= front_Size;
          outStandingSegs.pop_front();
          timer.set(initial_RTO_ms_);
          retransmit_Num=0;
          timePass = 0;
          if (outStandingSegs.empty())timer.stop();
      }else break;
    }
  }
}

void TCPSender::tick( const size_t ms_since_last_tick )
{
  if (timer.isOn()){
    timePass+=ms_since_last_tick;
  }
  if (timer.isTimeOut(timePass)){
    timePass = 0;
    retransmit_Num++;
    Msgs.push_front(outStandingSegs.front());
    if (window_Size != 0) timer.duoble_RTO();
  }
  // 需要在这里进行重传信息
}

//TODO:TODO:本来因为窗口满了push掉的没有完全组装成message，在ack有了新的窗口容量后，就可以传过去了
// TODO:sender和receiver的时候都是SYN和FIN非拆毁嗯容易出错，要不要另外弄一个函数？
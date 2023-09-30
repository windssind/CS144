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
  /*if (outbound_stream.is_finished()) hasClosed = true ; 
  uint64_t bufferNum = outbound_stream.bytes_buffered();
  uint64_t Max_Bytes ; //最大传输值为windowsize和现有的bufferNum的较小值
  // NOte:: hasClosed might be instable
  Max_Bytes= min(bufferNum + !hasConnected + (hasClosed &&!hasSendFIN),left_Window_Size); //如果窗口满了，就默认值为1*/ // 这是这次push中可以发送的最大bit值 // 主要还是Max——Bytes指代不明，Max——Bytes这时如果你要认定他为本次传输的最多要传输自己字节，那么由于FIN和SYN未确定，那么Max——Byres不能单纯用BUfferNnum来衡量
  // TODO：如果是后面push完才finsih，这里的Max——Bytes就会少1
  // 数据分组成为SenderMSGs*/
  /*uint64_t i=0;
  for(;i < Max_Bytes;){ // 如果是有东西要送或者finish或者还没有连接，都需要进入这里发送信息
    //不好的实现，引发很多bug阿if (bufferNum == 0 && !SYN) return; // 排除特殊情况，如果已经connected并且buffer为0,就不需要再push。直接return就好了
    uint64_t Segbytes= min(Max_Bytes - i,TCPConfig::MAX_PAYLOAD_SIZE); //这是组成这个分组需要的bytes数,bytes数还需要小于窗口
    bool SYN = hasConnected ? false : true ; 
    i+=SYN;
    string data ;
    for (uint64_t j=i;j<Segbytes && outbound_stream.bytes_buffered() > 0 ;++j){
      data.append(outbound_stream.peek()); // 这里可以直接这样写
      outbound_stream.pop(1);
      i++;
    }
    if (outbound_stream.is_finished()) hasClosed = true ; 
    bool FIN = hasClosed && i +1 == Max_Bytes ? true : false ;
    i+=FIN ; 
    TCPSenderMessage msgWaitForAcked = TCPSenderMessage{nextSeqno,SYN,Buffer(data),FIN};
    if (!hasConnected && SYN) hasConnected = true ; 
    if (FIN) hasSendFIN = true ;
    uint64_t msgLen = msgWaitForAcked.sequence_length();
    seqnoInFlight += msgLen ;
    left_Window_Size -= msgLen ; //猜想：一次能写入的message最大是窗口。。。。
    Msgs.push_back(msgWaitForAcked);
    outStandingSegs.push_back(msgWaitForAcked);
    nextSeqno= nextSeqno + msgLen; // seqno 递增
    // 这个是outstanding
    if (!timer.isOn()) timer.start(initial_RTO_ms_);
  }*/
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
    /*for (uint64_t i=0;i<delta;){
      uint64_t ackBytes = outStandingSegs.front().sequence_length();
      seqnoInFlight -= ackBytes ; 
      outStandingSegs.pop_front();
      i+=ackBytes ;
    }
    timer.set(initial_RTO_ms_);
    retransmit_Num=0;
    timePass = 0; //当收到新的确认消息后，重置timePass和retransmit-num
    if (outStandingSegs.size() == 0) timer.stop();*/
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
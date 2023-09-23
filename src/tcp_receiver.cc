#include "tcp_receiver.hh"
#include  <optional>
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  uint64_t AbSlSeqno ; 
  if (message.FIN == true) FIN = true;
  // 这个是初始握手的情况
  if (message.SYN) { // 暂时就想着是SYN的话，就设置，然后结束，数据段没有东西的
    ISN= message.seqno ;
    ackno = (message.seqno+message.sequence_length()); 
    reassembler.insert(0,(string)(std::string_view)(message.payload),FIN,inbound_stream);      // 也要记得设置确认号码
    if (FIN){
      inbound_stream.close();
    }
    return ;
  } // 两个都是封装好的Wrap32类型
  
  // 只有有初始值的时候才进行正常操作，不然就忽略
  if (ISN.has_value()){
    uint64_t preNeedIndex = reassembler.get_needIndex();
    AbSlSeqno = message.seqno.unwrap(ISN.value(),reassembler.get_needIndex());
    long int delta = AbSlSeqno - ackno.value().unwrap(ISN.value(),preNeedIndex) - message.SYN; // 这个是有问题的
    reassembler.insert((uint64_t)(preNeedIndex + delta),(string)(std::string_view)(message.payload),FIN,inbound_stream);// TODO：存疑，第一个参数 很明显是错误的，在streamindex和absolute之间的index应该如何转化
    //这行代码有问题，因为传来的message不一定是会被立刻使用ackno = ackno.value()+message.sequence_length();
    uint64_t nextNeedIndex = reassembler.get_needIndex();
    ackno = ISN.value() + nextNeedIndex + 1; // 修改成这个的理由
    if (FIN){
      if (reassembler.isFin()){
        ackno = ISN.value() + (uint32_t)(nextNeedIndex+2) ;
      }
    }
    // TODO :解决提早FIN，最后再补齐时候的问题
    //ackno = ackno.value()+ (uint32_t)message.sequence_length();
  }
}

// 不包括send，只是构造一个message,让sender去处理
TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  TCPReceiverMessage message;
  if (ISN.has_value()) message.ackno = ackno ;
  uint64_t capacity= inbound_stream.available_capacity();
  message.window_size= (capacity>USHRT_MAX? USHRT_MAX : capacity);
  return message ; 
}

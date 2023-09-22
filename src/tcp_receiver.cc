#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  // What should I do ?
  /*1.reveive ,and insert to reassembler
  */
  uint64_t AbSlSeqno ; 
  bool isLastbytes;
  uint64_t dataLen = message.payload.size();
  if (message.SYN) ISN= message.seqno; // 两个都是封装好的Wrap32类型
  else AbSlSeqno = message.seqno.unwrap(ISN,reassembler.get_needIndex());
  reassembler.insert(AbSlSeqno,message.payload.buffer_,isLastbytes,inbound_stream);
  // insert结束后返回ACKno
  backMessage.window_size = inbound_stream.available_capacity();
  if (ISN.has_value())backMessage.ackno = reassembler.get_needIndex();
  //send(inbound_stream);
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  
}

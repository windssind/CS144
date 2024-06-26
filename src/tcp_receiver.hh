#pragma once

#include "reassembler.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <optional>

class TCPReceiver
{
private :
  std::optional <Wrap32> ISN;
  std::optional <Wrap32> ackno;
  bool SYN;
  bool FIN ;
public:
  /*
   * The TCPReceiver receives TCPSenderMessages, inserting their payload into the Reassembler
   * at the correct stream index.
   */
  TCPReceiver():ISN(),ackno(),SYN(false),FIN(false){};
  void receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream );

  /* The TCPReceiver sends TCPReceiverMessages back to the TCPSender. */
  TCPReceiverMessage send( const Writer& inbound_stream ) const;
};

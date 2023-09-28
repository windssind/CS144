#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <deque>
#include <optional>
class Timer{
  private :
    uint64_t RTO;
    bool on ; 
  public:
    bool isTimeOut(uint64_t timePass);
    void duoble_RTO();
    void start(uint64_t RTO_);
    void stop();
    bool isOn();
    void set(size_t initial_RTO_ms);
  };

class TCPSender
{private:
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  uint64_t retransmit_Num ; 
  uint64_t timePass ;
  uint16_t window_Size ;  
  uint64_t nowIndex ;
  uint64_t seqnoInFlight;
  Wrap32 send_base ; 
  Wrap32 nextSeqno ;
  bool SYN ;
  bool FIN ;
  bool hasConnected;
  Timer timer; 
  std::deque < TCPSenderMessage > outStandingSegs;
  std::deque < TCPSenderMessage > Msgs;
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );
  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};


#include "wrapping_integers.hh"
#include <math.h>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + n ;
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t base = checkpoint & 0xFFFFFFFF00000000 ;// 这里用位运算会
  uint64_t Abslseqno;
  uint64_t delta = this->raw_value_ - zero_point.raw_value_;
  uint64_t possibleNum1 = (base + delta >= (1UL<<32) ? base +delta - (1UL<<32) : delta); // 这个就是错误的，unsigned阿
  uint64_t possibleNum2 = base + delta ;
  uint64_t possibleNum3 = base + delta + (1UL<<32);
  /*if (checkpoint <= possibleNum1){
    Abslseqno = possibleNum1 ;
  }else {
    Abslseqno = possibleNum2;
  }*/ // 原版，想当然了，应该是最近的一个，所以从这里可以理解为什么窗口数需要
  if (abs((long)possibleNum1-(long)checkpoint)<=abs((long)possibleNum2-(long)checkpoint)&&abs((long)possibleNum1-(long)checkpoint)<=abs((long)possibleNum3-(long)checkpoint)){
    Abslseqno = possibleNum1;
  }else if(abs((long)possibleNum2-(long)checkpoint)<=abs((long)possibleNum1-(long)checkpoint)&&abs((long)possibleNum2-(long)checkpoint)<=abs((long)possibleNum3-(long)checkpoint)){
    Abslseqno = possibleNum2;
  }else{
    Abslseqno = possibleNum3;
  }
  return Abslseqno;
}

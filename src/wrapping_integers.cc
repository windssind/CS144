#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { n + zero_point};
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t base = checkpoint & 0xFFFFFFFF00000000 ;// 这里用位运算会
  return raw_value_-zero_point +base;
}

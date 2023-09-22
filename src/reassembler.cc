#include "reassembler.hh"
#include <iostream>

using namespace std;
Reassembler ::Reassembler():needIndex (0) ,endIndex(0xFFFFFFFF), sizeArr (),indexArr (),subStrings(),bytesPending(0){};
void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{ 
  if (is_last_substring) endIndex = first_index + data.length();
  if((data=cutBeyondCapacity(first_index , data,output.available_capacity()))=="") {
    if (needIndex>=endIndex){
      output.close();
    }
    return ;
  }
 // 超过容量的不要,返回空串意味着全部超标，都不要 // 一个潜在的bug，可能会向0下面溢出
  first_index = needIndex > first_index ? needIndex :first_index; //重新修改first_index
 // 到这里的data已经是经过修剪，多余部分舍弃之后的结果了
  //对修剪后的data进行合并
  merge(&data,&first_index);
  // 合并之后将合并之后的push回vector中
  //看看能否插进去
  uint64_t newLen = data.length();
  // push的data有问题
  if (first_index==this->needIndex){ //
    output.push(data);
    needIndex+=newLen; // needIndex 进行更新
  }else {// 否则就是没找到,将这个插入即可
    subStrings.push_back(data);
    sizeArr.push_back(newLen);
    indexArr.push_back(first_index);
    bytesPending+=newLen;
  }
  
  if (needIndex>=endIndex){
    output.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return bytesPending;
}

void Reassembler::removeSubString(uint64_t index){
  subStrings.erase(subStrings.begin()+index);
  indexArr.erase(indexArr.begin()+index);
  sizeArr.erase(sizeArr.begin()+index);
}

string Reassembler::cutBeyondCapacity(uint64_t first_index , string data,uint64_t availableCapacity){
  uint64_t dataLen=data.length();
  if (needIndex + availableCapacity <= first_index || first_index + dataLen <= needIndex ) return ""; // 返回空串
  // 下面这两行代码还是想了一段时间
  uint64_t firstBehind = needIndex  > first_index ? needIndex -first_index : 0;
  uint64_t tailBeyond = dataLen +first_index > needIndex +availableCapacity ? dataLen +first_index- needIndex-availableCapacity : 0;
  uint64_t newLen  = dataLen - tailBeyond - firstBehind;
  return data.substr(firstBehind , newLen);
}

void Reassembler:: merge(string *data , uint64_t * first_index){
  uint64_t lenArr = subStrings.size();
  for (uint64_t i=0;i<lenArr;++i){
      uint64_t ToBeMergedIndex = indexArr [i];
      uint64_t ToBeMergedSize = sizeArr [i];
      string stringToBeMerged=subStrings [i];
      uint64_t dataLen =data->length();
      if (*first_index + dataLen < ToBeMergedIndex// 这个是在两端 
        || *first_index > ToBeMergedIndex + ToBeMergedSize) continue;
      else if (*first_index > ToBeMergedIndex && *first_index +dataLen < ToBeMergedIndex +ToBeMergedSize){ // data 内含在中间
        *data = stringToBeMerged ;
        *first_index = ToBeMergedIndex; 
      }else if (*first_index <= ToBeMergedIndex && *first_index + dataLen >= ToBeMergedIndex + ToBeMergedSize ) { //这个是完全包含TobeMerged,即不需要更新data
        //*data =stringToBeMerged ;
        //*first_index=ToBeMergedIndex;
      }
      else if (*first_index <= ToBeMergedIndex && *first_index + dataLen >=ToBeMergedIndex ){ 
        uint64_t overlappedLen = *first_index + dataLen - ToBeMergedIndex;
        *data = (*data ).append(stringToBeMerged.substr(overlappedLen, ToBeMergedSize -overlappedLen));
        //first_index = first_index;
      }else if (*first_index >= ToBeMergedIndex && *first_index + dataLen >= ToBeMergedIndex + ToBeMergedSize){ //这个是右边超车
        uint64_t overlappedLen = ToBeMergedIndex + ToBeMergedSize - *first_index;
        *data = stringToBeMerged.append((*data).substr(overlappedLen, dataLen - overlappedLen));
        *first_index = ToBeMergedIndex;
      }
      removeSubString(i);
      bytesPending-=ToBeMergedSize ;
      --i;
      --lenArr;
    }
}

uint64_t Reassembler :: get_needIndex() {
  return this->needIndex;
}
// TODO：还要补充：合并后如果超出了的部分，也是unacceppted
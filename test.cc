#include <deque>
#include <stdio.h>
#include<string_view>
int main(){
    std:: deque < char> test;
    printf("%c\n",*(std::string_view(&test.front(),1).data()));
}
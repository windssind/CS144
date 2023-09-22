#include "socket.hh"

#include <cstdlib>
#include <iostream>
#include <span>
#include <string>

using namespace std;

void get_URL( const string& host, const string& path )
{
  // 创建一个套接字，然后连接到远程服务器，发送请求然后接受返回的信号
  TCPSocket sock;
  Address addrClient=Address(host,"http"); //第一个参数是host，第二个参数是采用的协议类型
  sock.connect(addrClient); //socket 连接到了本地
  sock.write("GET "+path+" HTTP/1.1\r\n"+"Host:"+host+"\r\n"+"Connection: clsoe\r\n\r\n");// 疑问，都没根web服务器连接呢，要发给水呢？
  while(1){
    string recv;
    sock.read(recv);
    cout<<recv;
    if (sock.eof()) break; // 如果已经读完了所有信息，就终止
  }
  sock.close();
}

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

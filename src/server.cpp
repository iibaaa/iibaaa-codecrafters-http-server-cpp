#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>


void send_response(int socket_fd, std::string response)
{
    send(socket_fd, response.c_str(), response.size(), 0);
}


int main(int argc, char **argv) {
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  // Uncomment this block to pass the first stage
  //
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting REUSE_PORT
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  struct sockaddr_in client_addr;
  int client_addr_len = sizeof(client_addr);
  
  std::cout << "Waiting for a client to connect...\n";
  
  // Update here for get clint socket;
  int client_socket = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
  std::cout << "Client connected\n";

  // std::string response = "HTTP/1.1 200 OK\r\n\r\n";
  // send(client_socket, response.c_str(), response.size(), 0);

  std::string res_200 = "HTTP/1.1 200 OK\r\n\r\n";
  std::string res_400 = "HTTP/1.1 400 Not Found\r\n\r\n";
  char buf[1024];
  std::string buffer= "";

  int receive_buf_size = recv(client_socket, buffer.data(), 1024, 0);
  // std::cout << "Received Msg : \n" << buffer << std::endl;
  
  int get_point = buffer.find("GET")+4;
  int http_point = buffer.find("HTTP")-1;

  std::string path = buffer.substr(get_point, http_point - get_point);

  std::cout <<  "Path : " << path << std::endl;

  if (path == "/")
  {
    std::cout << "Sending 200 response" << std::endl;
    send_response(client_socket, res_200);
  }
  else
  {
    std::cout << "Sending 400 response" << std::endl;
    send_response(client_socket, res_400);
  }



  close(server_fd);

  return 0;
}

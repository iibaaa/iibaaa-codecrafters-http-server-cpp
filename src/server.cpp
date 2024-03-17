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
#include <thread>
#include <fstream>


std::string res_200 = "HTTP/1.1 200 OK\r\n\r\n";
std::string res_404 = "HTTP/1.1 404 Not Found\r\n\r\n";

void send_response(int socket_fd, std::string &response)
{
	send(socket_fd, response.c_str(), response.size(), 0);
}

std::vector<std::string> splitString(const std::string &input)
{
	std::vector<std::string> result;
	size_t startPos = 0;
	std::string delimiter = "\r\n";
	size_t delimiterPos = input.find(delimiter, startPos);

	while (delimiterPos != std::string::npos)
	{
		std::string token = input.substr(startPos, delimiterPos - startPos);
		result.push_back(token);
		startPos = delimiterPos + delimiter.length();
		delimiterPos = input.find(delimiter, startPos);
	}

	if (startPos < input.length())
	{
		result.push_back(input.substr(startPos));
	}

	return result;
}

void threadHandleConnection(int client_sock, std::string file_folder)
{
	std::cout << "New Connection starting with " << client_sock << "." << std::endl;
	char buffer[1024] = {0};

	int receive_buf_size = recv(client_sock, buffer, 1023, 0);
	if (receive_buf_size < 0)
	{
		std::cout << "Receive Failed." << std::endl;
		close(client_sock);
		return ;
	}

	std::string bufferString(buffer, receive_buf_size);

	std::vector<std::string> splitResponse = splitString(bufferString);

	int getIdx = 0;
	for (int i = 0; i < splitResponse.size(); i++)
	{
		std::string line = splitResponse[i];
		if (line.find("GET") != std::string::npos)
		{
			getIdx = i;
			break;
		}
	}
	// Check if GET Exist In First Line
	std::string GETPART = splitResponse[getIdx];
	std::string HOSTPART = splitResponse[getIdx + 1];
	std::string USERPart = splitResponse[getIdx + 2];
	// If GET in text, find HTTP too and substring path
	auto getPosition = GETPART.find("GET");
	auto httpPosition = GETPART.find("HTTP");
	std::string path = "/";
	if (getPosition != std::string::npos && httpPosition != std::string::npos)
	{
		path = GETPART.substr(getPosition + 4, httpPosition - 5);
		std::cout << "Path Found :" << path << "." << std::endl;
	}

	if (path == "/")
	{
		std::cout << "Sending response 200" << std::endl;
		send_response(client_sock, res_200);
	}
	else if (path.find("files") != std::string::npos)
	{
		std::string file_name = path.substr(7);
		std::string file_abs_path = file_folder + "/" + file_name;
		std::cout << "Abs File Path : " << file_abs_path << std::endl;
		std::ifstream file(file_abs_path);
		std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
		if (file.is_open())
		{
			std::string response_msg = "";
			std::string line;
			while (std::getline(file, line)) {
				response_msg += line;
			}
			file.close();
			
			response = "HTTP/1.1 200 OK\r\n";
			response += "Content-Type: application/octet-stream\r\n";
			response += ("Content-Length: " + std::to_string(response_msg.size()) + "\r\n\r\n");
			response += response_msg;
			std::cout << "File Found" << std::endl;
		}
		send_response(client_sock, response);
	}
	else if (path.find("echo") != std::string::npos)
	{
		std::string response = "HTTP/1.1 200 OK\r\n";
		std::string response_msg = path.substr(path.find("echo") + 5);
		response += "Content-Type: text/plain\r\n";
		response += ("Content-Length: " + std::to_string(response_msg.size()) + "\r\n\r\n");
		response += response_msg;
		std::cout << "Response : \n"
				  << response << std::endl;
		send_response(client_sock, response);
	}
	else if (path == "/user-agent")
	{
		std::string response = "HTTP/1.1 200 OK\r\n";
		std::string response_msg = USERPart.substr(USERPart.find(":") + 2);

		response += "Content-Type: text/plain\r\n";
		response += ("Content-Length: " + std::to_string(response_msg.size()) + "\r\n\r\n");
		response += response_msg;
		std::cout << "Response : \n"
				  << response << std::endl;
		send_response(client_sock, response);
	}
	else
	{
		std::cout << "Sending response 404" << std::endl;
		send_response(client_sock, res_404);
	}

	close(client_sock);
	return;
}


int main(int argc, char **argv)
{
	// You can use print statements as follows for debugging, they'll be visible when running tests.
	std::cout << "Logs from your program will appear here!\n";

	// Uncomment this block to pass the first stage
	//
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		std::cerr << "Failed to create server socket\n";
		return 1;
	}

	// Since the tester restarts your program quite often, setting REUSE_PORT
	// ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0)
	{
		std::cerr << "setsockopt failed\n";
		return 1;
	}

	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(4221);

	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
	{
		std::cerr << "Failed to bind to port 4221\n";
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0)
	{
		std::cerr << "listen failed\n";
		return 1;
	}

	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	std::cout << "Waiting for a client to connect...\n";


	std::string file_folder = "";

	if (argc == 3 && std::string(argv[1]) == "--directory")
	{
		file_folder = argv[2];
	}

	std::vector<int> clients;
	std::vector<std::thread> connectionThreads;
	while (true)
	{	
		int client_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t *)&client_addr_len);

		// Update here for get clint socket;
		std::cout << "Client connected with " << client_socket << " ." << std::endl;

		// Add to clients lists
		clients.push_back(client_socket);

		// Create thread
		// Create and immediately detach thread
		std::thread thread([client_socket, file_folder]() {
			threadHandleConnection(client_socket, file_folder);
		});
		// Add To connectipn threads
		connectionThreads.push_back(std::move(thread));
	
	}

	for (auto &t : connectionThreads)
	{
		 if (t.joinable())
            t.join();
	}

	close(server_fd);

	return 0;
}

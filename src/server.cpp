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

enum Method
{
	GET,
	POST,
};

struct DataHolder
{
	Method method;
	std::string path;
	std::string user_agent;
	std::string  content_lenght;
	std::string content_type;
	std::string content;
};

void send_response(int socket_fd, std::string response)
{
	std::cout << "Response :\n" << response << "\n------" <<std::endl;
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

DataHolder parseRequest(std::string request)
{
	DataHolder temp;
	int start_p, end_p;
	std::vector<std::string> lineParsedRequest = splitString(request);

	// Check if GET or POST
	std::string tmp_string = lineParsedRequest[0];
	if (tmp_string.find("GET") != std::string::npos)
		temp.method = Method::GET;
	else
		temp.method = Method::POST;

	// Parse path, path starts with /
	start_p = tmp_string.find("/");
	end_p = tmp_string.find("HTTP");

	if (start_p != std::string::npos && end_p != std::string::npos)
		temp.path = tmp_string.substr(start_p , end_p - start_p -1);
	
	if (lineParsedRequest.size() > 2)
	{
		// Parse User Agent
		tmp_string = lineParsedRequest[2];
		start_p = tmp_string.find(":");
		if (start_p != std::string::npos)
			temp.user_agent = tmp_string.substr(start_p+2);
	}

	if (temp.method == Method::POST)
	{
		int content_idx = lineParsedRequest.size();
		for (int idx = 3; idx < lineParsedRequest.size(); idx++)
		{
			tmp_string = lineParsedRequest[idx];
			if (tmp_string.find("Content-Length") != std::string::npos)
			{
				// Parse Content Lenght
				start_p = tmp_string.find(":");
				if (start_p != std::string::npos)
					temp.content_lenght = tmp_string.substr(start_p+2);
				
				continue;
			}
			
			if (tmp_string.find("Content-Type") != std::string::npos)
			{
				// Parse Content Type
				start_p = tmp_string.find(":");
				if (start_p != std::string::npos)
					temp.content_type = tmp_string.substr(start_p+2);
				
				continue;
			}

			if (tmp_string == "" || idx > content_idx)
			{
				content_idx = idx;
				temp.content += tmp_string;
				continue;
			}
		}

		std::cout << "Content : \n" << temp.content << std::endl;
	}

	return temp;
}

void createFile(std::string file_path, std::string content)
{
	std::ofstream file(file_path);
	file << content;
	file.close();
}

std::string readFile(std::string file_path)
{
	std::string content = "NOFILE";
	std::fstream file(file_path);
	if (file.is_open())
	{
		content = "";
		std::string line;
		while (std::getline(file, line)) {
			content += line;
		}
		file.close();
	}
	return content;
}

void threadHandleConnection(int client_sock, std::string file_folder)
{
	std::cout << "New Connection starting with " << client_sock << "." << std::endl;
	char buffer[1024] = {0};

	int receive_buf_size = recv(client_sock, buffer, 1023, 0);
	if (receive_buf_size <= 0)
	{
		std::cout << "Receive Failed." << std::endl;
		close(client_sock);
		return;
	}

	std::string bufferString(buffer, receive_buf_size);
	
	std::cout << "Request Size: " << receive_buf_size << "\n" << bufferString << std::endl;

	DataHolder data = parseRequest(bufferString);

	switch(data.method)
	{
		case Method::GET:
		
		if (data.path == "/")
		{
			send_response(client_sock, "HTTP/1.1 200 OK\r\n\r\n");
			break;
		}

		if (data.path == "/user-agent")
		{
			std::string response = "HTTP/1.1 200 OK\r\n";
			std::string content = data.user_agent;

			response += "Content-Type: text/plain\r\n";
			response += ("Content-Length: " + std::to_string(content.size()) + "\r\n\r\n");
			response += content;
			send_response(client_sock, response);
			break;
		}

		if (data.path.find("files") != std::string::npos)
		{
			std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
			std::string file_abs_path = file_folder + data.path.substr(7);
			std::string content = readFile(file_abs_path);

			if (content != "NOFILE")
			{
				response = "HTTP/1.1 200 OK\r\n";
				response += "Content-Type: application/octet-stream\r\n";
				response += ("Content-Length: " + std::to_string(content.size()) + "\r\n\r\n");
	 			response += content;
			}	
			send_response(client_sock, response);
			break;
		}

		if (data.path.find("echo") != std::string::npos)
		{
			std::string response = "HTTP/1.1 200 OK\r\n";
			std::string content = data.path.substr(data.path.find("echo") + 5);

			response += "Content-Type: text/plain\r\n";
			response += ("Content-Length: " + std::to_string(content.size()) + "\r\n\r\n");
			response += content;
			send_response(client_sock, response);
			break;
		}

		
		send_response(client_sock, "HTTP/1.1 404 Not Found\r\n\r\n");
		break;

		case Method::POST:
			std::string file_abs_path = file_folder + data.path.substr(7);
			createFile(file_abs_path, data.content);
			send_response(client_sock, "HTTP/1.1 201 CREATED\r\n\r\n");
		break;
	};

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

	std::string file_folder = "";
	if (argc == 3 && std::string(argv[1]) == "--directory")
	{
		file_folder = argv[2];
		if (file_folder[-1] != '/')
			file_folder += "/";
	}

	std::vector<int> clients;
	std::vector<std::thread> connectionThreads;
	struct sockaddr_in client_addr;
	int client_addr_len = sizeof(client_addr);

	std::cout << "Waiting for a client to connect...\n";
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
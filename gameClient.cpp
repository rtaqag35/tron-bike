#include <iostream>
#include <cstdio>
#include <string>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

bool sendLine(int fd, const std::string &text)
{
	std::string out = text + "\n";
	size_t sent = 0;
	while (sent < out.size())
	{
		ssize_t n = send(fd, out.c_str() + sent, out.size() - sent, 0);
		if (n <= 0)
			return false;
		sent += n;
	}
	return true;
}

bool recvLine(int fd, std::string &pending, std::string &line)
{
	while (true)
	{
		size_t pos = pending.find('\n');
		if (pos != std::string::npos)
		{
			line = pending.substr(0, pos);
			pending.erase(0, pos + 1);
			return true;
		}
		char buffer[256];
		ssize_t n = recv(fd, buffer, sizeof(buffer), 0);
		if (n <= 0)
			return false;
		pending.append(buffer, n);
	}
}

int main()
{
	int client = socket(AF_INET, SOCK_STREAM, 0);
	if (client < 0)
	{
		perror("socket");
		return 1;
	}

	sockaddr_in server{};
	server.sin_family = AF_INET;
	server.sin_port = htons(9001);

	if (inet_pton(AF_INET, "127.0.0.1", &server.sin_addr) <= 0)
	{
		perror("inet_pton");
		close(client);
		return 1;
	}

	if (connect(client, (sockaddr *)&server, sizeof(server)) < 0)
	{
		perror("connect");
		close(client);
		return 1;
	}
	std::cout << "Connected to server!\n";

	std::string pending, line;
	while (recvLine(client, pending, line))
	{
		if (line == "END")
			break;

		if (line.rfind("MSG ", 0) == 0)
		{
			std::cout << line.substr(4) << "\n";
		}
		else if (line.rfind("ASK ", 0) == 0)
		{
			std::cout << line.substr(4) << std::flush;
			std::string input;
			if (!std::getline(std::cin, input))
				break;
			if (!sendLine(client, input))
				break;
		}
	}

	close(client);
	std::cout << "Disconnected.\n";
	return 0;
}
// System/compiler includes
#include <iostream>    // Basic console input/output
#include <string>      // String variables
#include <sstream>     // Converting variables
#include <vector>      // Dynamic arrays for Boost libraries
#include <WinSock2.h>  // For the sockets 
#include <WS2tcpip.h>  // For InetPton 
#include <Windows.h>   // Windows default header
#include <fstream>     // File in-/output
#include <chrono>      // Time (used for ping command) 
#include <thread>      // Multi-threading
#include <atomic>      // For atomic booleans that can be read thread-to-thread
#include <mutex>       // Locking and unlocking console
//#include <future>      // Thread return values

// Boost includes
#include <boost/algorithm/string.hpp>           // boost::split
#include <boost/algorithm/string/replace.hpp>   // boost::replace
#include <boost/filesystem.hpp>                 // Used for listing files

// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


// Initializing global variables
char end;
std::string serverip = "192.168.0.8"; // IP adress server will be running on, if not local
int port = 5555;                      // Port the server will be running on
char buffer[1024];                    // Buffer with max lenght of 1024 bytes
char buffer2[1024];                   // Buffer with max lenght of 1024 bytes
char pingBuffer[1024] = "ping";       // Ping buffer containing message "ping"
char pongBuffer[1024];                // Pong buffer (will be used on recv end from server) 
bool debug = false;                   // Show debug information on/off (default == off) 
bool local = false;                   // Run server locally (default == off) 

// Atomic variables
std::atomic<bool> terminate_thread(false);
std::atomic<bool> disconnect(false);

// Mutex
std::mutex mu;

// Global socket values
// Initializing socket variables
WSADATA WSAData;
SOCKET server;
SOCKADDR_IN seraddr;
SOCKADDR addr;

// Defining thread functions
void clientHandler(int argc, const char* argv[]);
void recvHandler();
void initClient();

// Main function
int main(int argc, const char* argv[]) {

	// Algorithm for checking arguments
	for (int i = 1; i < argc; ++i) {
		if (std::string(argv[i]) == "-l" || std::string(argv[i]) == "local") {
			local = true;
			serverip = "127.0.0.1";
		}
		else if (std::string(argv[i]) == "-ip") {
			serverip = argv[i + 1];
		}
		else if (std::string(argv[i]) == "-p") {
			std::stringstream iss;
			iss << argv[i + 1];
			iss >> port;
			if (iss.fail()) {
				std::cout << "Could not convert input to int. Returning 1." << std::endl;
				return 1;
			}
		}
		else if (std::string(argv[i]) == "-d") {
			debug = true;
		}
	}

	initClient();

	std::thread clientHndlr(clientHandler, argc, argv);
	std::thread recvHndlr(recvHandler);

	clientHndlr.join();
	recvHndlr.join();

	while (!terminate_thread);
	std::this_thread::sleep_for(std::chrono::seconds(1));
	clientHndlr.detach();
	recvHndlr.detach();



	// Locking the console until return
	std::cin >> end;
	return 0;
}

// Initialize global variables
void initClient() {
	/*struct addrinfo *result = NULL,
					*ptr = NULL,
					 hints;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	getaddrinfo("127.0.0.1", "5555", &hints, &result);

	ptr = result;*/
	// Initializing sockets
	seraddr.sin_family = AF_INET;
	seraddr.sin_port = htons(port);

	memcpy(&addr, &seraddr, sizeof(SOCKADDR_IN));

	// Start Winsock
	int iResult = WSAStartup(MAKEWORD(2, 0), &WSAData);
	std::cout << "WSAStartup"
		<< "\nVersion: " << WSAData.wVersion
		<< "\nDescription: " << WSAData.szDescription
		<< "\nStatus: " << WSAData.szSystemStatus << std::endl;

	if (iResult != 0) {
		printf("WSAStartup failed with error: %d\n", iResult);
		terminate_thread = true;
		return;
	}

	// Creat socket
	server = socket(AF_INET, SOCK_STREAM, 0);

	if (server == INVALID_SOCKET) {
		std::cout << "Invalid socket" << std::endl;
		terminate_thread = true;
		return;
	}
	else if (server == SOCKET_ERROR) {
		std::cout << "Socket error" << std::endl;
		terminate_thread = true;
		return;
	}

	// Initializing and configuring sockaddr
	if (local == true) {
		InetPton(AF_INET, "127.0.0.1", &seraddr.sin_addr.s_addr);
		std::cout << "Searching for connections on \"" << serverip << ":" << port << "\"" << std::endl;
	}
	else {
		InetPton(AF_INET, serverip.c_str(), &seraddr.sin_addr.s_addr);
		std::cout << "Searching for connections on \"" << serverip << ":" << port << "\"" << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::seconds(5));
}


// Initializing thread functions
void clientHandler(int argc, const char* argv[]) {
	// Connecting to server
	bool connected = false;
	int retry = 0;
	int res = connect(server, &addr, sizeof(addr));
	if (res != 0) {
		while (connected == false) {
			++retry;
			std::cout << "\r                                                                      " << std::flush;
			COORD p = { 0, 1 };
			SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), p);
			std::cout << "\rRetrying..." << std::flush;
			if (connect(server, (SOCKADDR*)&seraddr, sizeof(seraddr)) != 0) {
				if (retry == 1 || retry == 2) {
					for (int i = 5; i >= 0; --i) {
						std::cout << "\r                                                                      " << std::flush;
						COORD p = { 0, 1 };
						SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), p);
						std::cout << "\rFailed to connect to server! Retrying in " << i << std::flush;
						Sleep(1000);
					}
				}
				else if (retry == 3) {
					for (int i = 10; i >= 0; --i) {
						std::cout << "\r                                                                      " << std::flush;
						COORD p = { 0, 1 };
						SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), p);
						std::cout << "\rFailed to connect to server! Retrying in " << i << std::flush;
						Sleep(1000);
					}
				}
				else if (retry == 4 || retry == 5) {
					for (int i = 15; i >= 0; --i) {
						std::cout << "\r                                                                      " << std::flush;
						COORD p = { 0, 1 };
						SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), p);
						std::cout << "\rFailed to connect to server! Retrying in " << i << std::flush;
						Sleep(1000);
					}
				}
				else if (retry == 6) {
					std::cout << "\r                                                                      " << std::flush;
					COORD p = { 0, 1 };
					SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), p);
					std::cout << "\rFailed to connect to server! Retried 5 times. Exitting!" << std::endl;
					// Locking the console until return
					std::cin >> end;
					terminate_thread = false;
					goto bottom;
				}
			}
			else {
				break;
			}
		}
	}
	system("CLS");
	std::cout << "\rConnected to server! Listening on \"" << serverip << ":" << port << "\"" << std::endl;

	while (!terminate_thread) {
		// Getting input msg
		std::cout << "$>" << std::flush;
		std::cin.getline(buffer, 1024);

		// Algorithm that puts string into a vector<string> array to have command and argument(s) seperated
		std::string input = std::string(buffer);
		boost::replace_all(input, " ", "\t");
		std::vector<std::string> splitCommand;
		boost::split(splitCommand, input, boost::is_any_of("\t"));

		// Sending buffer
		int res = send(server, buffer, sizeof(buffer), 0);

		if (res == 0) {
			std::cout << "Server has forcefully closed connection." << std::endl;
			// Closing socket
			closesocket(server); // Disconnecting socket
			WSACleanup(); // Cleaning up WSA (WinSockAddr)
			std::cout << "Socket closed! No longer listening on " << serverip << ":" << port << std::endl;
			disconnect = true; // Stops the infinite loop to prevent sending messages to unconnected server
		}

		if ((strcmp(buffer, "disconnect") == 0) || (strcmp(buffer, "dc") == 0)) { // Comparing char to std::string with strcmp(const char* str1, const char* str2), if the same returns 0
			// Closing socket
			closesocket(server); // Disconnecting socket
			WSACleanup(); // Cleaning up WSA (WinSockAddr)
			std::cout << "Socket closed! No longer listening on " << serverip << ":" << port << std::endl;
			disconnect = true; // Stops the infinite loop to prevent sending messages to unconnected server
		}
		else if (splitCommand[0] == "ls") {
			char vectorSize[1024];
			int iVectorSize;
			int vectSizeRecv = recv(server, vectorSize, sizeof(vectorSize), 0);
			if (vectSizeRecv > 0) {
				std::stringstream ss;
				ss << vectorSize;
				ss >> iVectorSize;
				if (ss.fail()) {
					std::cout << "Could not convert vectorSize to int. Returning 1." << std::endl;
					terminate_thread = true;
				}
				int nLength;
				for (int i = 0; i < iVectorSize; ++i) {
					nLength = recv(server, buffer2, sizeof(buffer), 0);
					if (nLength > 0) {
						std::string name(buffer2);
						int folder = name.find_last_of(".");
						if (name == "." || name == "..") {
						}
						if (folder == -1) {
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 9);
							std::cout << name << std::endl;
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
						}
						else if (name.substr(name.find_last_of(".") + 1) == "cpp" || name.substr(name.find_last_of(".") + 1) == "h" || name.substr(name.find_last_of(".") + 1) == "txt") {
							std::cout << name << std::endl;
						}
						else if (name.substr(name.find_last_of(".") + 1) == "exe") {
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
							std::cout << name << std::endl;
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
						}
						else if (folder > (name.length() - 4)) {
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 5);
							std::cout << name << std::endl;
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
						}
						else {
							if (name != "." || name != "..") {
								std::cout << name << std::endl;
							}
						}
						memset(buffer2, 0, sizeof(buffer2));
					}
				}
				std::cout << std::endl;
			}
			memset(vectorSize, 0, sizeof(vectorSize));
		}
		else if (splitCommand[0] == "pwd" || splitCommand[0] == "cd") {
			int nLength;
			nLength = recv(server, buffer2, sizeof(buffer), 0);
			if (nLength > 0) {
				if (strcmp(buffer, "succesful") != 0) {
					std::cout << "Server says: " << buffer2 << std::endl;
					memset(buffer2, 0, sizeof(buffer2));
				}
			}
		}
		else if (splitCommand[0] == "download" || splitCommand[0] == "dl") {
			std::cout << "Attempting to download " << splitCommand[1] << " ..." << std::endl;
			char recvbuffer[1024];
			std::ofstream fout;
			fout.open("Downloads/" + splitCommand[1]);
			//std::cout << "Opened " << splitCommand[1] << " (copy)" << std::endl;
			while (true) {
				recv(server, recvbuffer, sizeof(recvbuffer), 0);
				if (debug == true) {
					std::cout << "Received: " << recvbuffer << std::endl;
				}
				if (strcmp(recvbuffer, "complete") == 0) {
					std::cout << "Completed download." << std::endl;
					break;
				}
				else {
					fout << recvbuffer;
				}
			}
			fout.close();
			memset(recvbuffer, 0, sizeof(recvbuffer));
		}
		else if (splitCommand[0] == "ping") {
			auto start = std::chrono::high_resolution_clock::now();
			send(server, pingBuffer, sizeof(pingBuffer), 0);
			recv(server, pongBuffer, sizeof(pongBuffer), 0);
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> elapsed = end - start;
			std::cout << "Ping: " << elapsed.count() << " ms" << std::endl;
			std::ostringstream oss;
			oss << elapsed.count();
			strcpy_s(buffer2, oss.str().c_str());
			send(server, buffer2, sizeof(buffer2), 0);
			memset(buffer2, 0, sizeof(buffer2));
			std::cout << "Ping completed" << std::endl;
		}
	}
bottom:
	memset(buffer, 0, sizeof(buffer));
	closesocket(server); // Disconnecting socket
	WSACleanup(); // Cleaning up WSA (WinSockAddr)
}

void recvHandler() {
	char recvBuf[1024];
	while (!terminate_thread && !disconnect) {
		recv(server, recvBuf, sizeof(recvBuf), 0);

		// Algorithm that puts string into a vector<string> array to have command and argument(s) seperated
		std::string input = std::string(recvBuf);
		boost::replace_all(input, " ", "\t");
		std::vector<std::string> splitCommand;
		boost::split(splitCommand, input, boost::is_any_of("\t"));

		// Sending buffer
		send(server, buffer, sizeof(buffer), 0);

		if ((strcmp(buffer, "disconnect") == 0) || (strcmp(buffer, "dc") == 0)) { // Comparing char to std::string with strcmp(const char* str1, const char* str2), if the same returns 0
			// Closing socket
			closesocket(server); // Disconnecting socket
			WSACleanup(); // Cleaning up WSA (WinSockAddr)
			std::cout << "Socket closed! No longer listening on " << serverip << ":" << port << std::endl;
			disconnect = true; // Stops the infinite loop to prevent sending messages to unconnected server
		}
		else if (splitCommand[0] == "ls") {
			char vectorSize[1024];
			int iVectorSize;
			int vectSizeRecv = recv(server, vectorSize, sizeof(vectorSize), 0);
			if (vectSizeRecv > 0) {
				std::stringstream ss;
				ss << vectorSize;
				ss >> iVectorSize;
				if (ss.fail()) {
					std::cout << "Could not convert vectorSize to int. Returning 1." << std::endl;
					terminate_thread = true;
				}
				int nLength;
				for (int i = 0; i < iVectorSize; ++i) {
					nLength = recv(server, buffer2, sizeof(buffer), 0);
					if (nLength > 0) {
						std::string name(buffer2);
						int folder = name.find_last_of(".");
						if (name == "." || name == "..") {
						}
						if (folder == -1) {
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 9);
							std::cout << name << "   " << std::flush;
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
						}
						else if (name.substr(name.find_last_of(".") + 1) == "cpp" || name.substr(name.find_last_of(".") + 1) == "h" || name.substr(name.find_last_of(".") + 1) == "txt") {
							std::cout << name << "   " << std::flush;
						}
						else if (name.substr(name.find_last_of(".") + 1) == "exe") {
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
							std::cout << name << "   " << std::flush;
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
						}
						else if (folder > (name.length() - 4)) {
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 5);
							std::cout << name << "   " << std::flush;
							SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
						}
						else {
							if (name != "." || name != "..") {
								std::cout << name << "   " << std::flush;
							}
						}
						memset(buffer2, 0, sizeof(buffer2));
					}
				}
				std::cout << std::endl;
			}
			memset(vectorSize, 0, sizeof(vectorSize));
		}
		else if (splitCommand[0] == "pwd" || splitCommand[0] == "cd") {
			int nLength;
			nLength = recv(server, buffer2, sizeof(buffer), 0);
			if (nLength > 0) {
				if (strcmp(buffer, "succesful") != 0) {
					std::cout << "Server says: " << buffer2 << std::endl;
					memset(buffer2, 0, sizeof(buffer2));
				}
			}
		}
		else if (splitCommand[0] == "download" || splitCommand[0] == "dl") {
			std::cout << "Attempting to download " << splitCommand[1] << " ..." << std::endl;
			char recvbuffer[1024];
			std::ofstream fout;
			fout.open("Downloads/" + splitCommand[1]);
			//std::cout << "Opened " << splitCommand[1] << " (copy)" << std::endl;
			while (true) {
				recv(server, recvbuffer, sizeof(recvbuffer), 0);
				if (debug == true) {
					std::cout << "Received: " << recvbuffer << std::endl;
				}
				if (strcmp(recvbuffer, "complete") == 0) {
					std::cout << "Completed download." << std::endl;
					break;
				}
				else {
					fout << recvbuffer;
				}
			}
			fout.close();
			memset(recvbuffer, 0, sizeof(recvbuffer));
		}
		else if (splitCommand[0] == "ping") {
			auto start = std::chrono::high_resolution_clock::now();
			send(server, pingBuffer, sizeof(pingBuffer), 0);
			int res = recv(server, pongBuffer, sizeof(pongBuffer), 0);
			if (res == 0) {
				std::cout << "Connection closed" << std::endl;
			}
			else if (res > 0) {
				std::cout << "Received" << std::endl;
			}
			else {
				std::cout << "Recv failed" << std::endl;
			}
			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> elapsed = end - start;
			std::cout << "Ping: " << elapsed.count() << " ms" << std::endl;
			std::ostringstream oss;
			oss << elapsed.count();
			strcpy_s(buffer2, oss.str().c_str());
			send(server, buffer2, sizeof(buffer2), 0);
			memset(buffer2, 0, sizeof(buffer2));
			std::cout << "Ping completed" << std::endl;
		}
		memset(recvBuf, 0, sizeof(recvBuf));
	}
}

/* 
 * Author	: Md Atiqur Rahman
 * Date 	: December, 2015
 * Compile	: 
 * 		clang++ -std=c++14 -stdlib=libc++ -o sniffer.o -lpcap sniffer.cpp
 *	then run,
 *		sudo ./sniffer.o

 * Desc:
 *		Build port to process name hash table using the output of lsof
 *		Then capture packets and increase counts for each port
 */

#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <unordered_map>

// uncomment this to see captured packet info, will slow down processing of packets
//#define ENABLE_CONSOLE_MESSAGES 1

struct port_info {
	std::string name;
	int iBytes;
	int oBytes;
};

const char *plocal_ip = "130.245.188.149";
std::unordered_map<std::string, port_info> global_process_hash_table;

bool sting_ends_with(std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else
    	return false;
}

void build_hash_table(const char* pStr) {
	std::stringstream stringStream(pStr);
	std::string line;
	std::string local_ip(plocal_ip);
	local_ip += ":";

	// only insert ports for localhost
	while(std::getline(stringStream, line)) {
	    std::size_t prev = 0, pos=0;
		// find position for first pattern after which source ip and port starts
	    if ((prev = line.find("TCP ", prev)) == std::string::npos && (prev = line.find("UDP ", prev)) == std::string::npos)
			continue;
		prev +=  strlen("TCP ");
		// find position for second pattern before which source port ends
		if ((pos = line.find("->", prev)) == std::string::npos &&  (pos = line.find(" ", prev)) == std::string::npos)
			continue;
		// if localhost or local ip string is found, trim them
		if (line.find("localhost:", prev) == prev)
			prev += strlen("localhost:");
		else if (line.find(local_ip, prev) == prev)
			prev += local_ip.length();
		else
			continue;
		if (pos <= prev)
			continue;
		// else we ignore other ip
		std::string token_key = line.substr(prev, pos-prev);
		bool hasDestIP = false;
		std::string second_key = "";
		if (pos+1 <  line.length() &&  line[pos+1] == '>')
			hasDestIP = true;
		if (! hasDestIP)
			goto no_second_key;
		prev = pos+2;
		
		// set pos and get dest port similarly for mapping
		// to get the second port, we go the same way verify ip
		if ((pos = line.find(" ", prev)) == std::string::npos)
			goto no_second_key;
		// std::cout<<"str from prev " << line.substr(prev)<<std::endl;
		if (line.find("localhost:", prev) == prev)
			prev += strlen("localhost:");
		else if (line.find(local_ip, prev) == prev)
			prev += local_ip.length();
		else
			goto no_second_key;
		
		second_key = line.substr(prev, pos-prev);
		
no_second_key:	
		// get process name
		if ((pos = line.find(" ", 0)) == std::string::npos)
			continue;
		std::string token_val = line.substr(0, pos);
		// find if string ends with \x20
		if (sting_ends_with(token_val, "\\x20"))
			token_val = token_val.substr(0,token_val.length()-4);
		// does not end with but contains
		else if ((pos = token_val.find("\\x20", 0)) != std::string::npos) {
			token_val = token_val.erase(pos+1,3);
			token_val[pos] = ' ';
		}
		/* std::cout<<" " << token_key << " ->  " << token_val << std::endl; 
		std::pair<std::unordered_map<std::string, std::string>::iterator, bool> */
		struct port_info tmp_obj = {token_val, 0, 0};
		auto res = global_process_hash_table.emplace(token_key, tmp_obj);
		if (res.second)
			std::cout<< "Key " << token_key << " successfully inserted." << std::endl;
		else
			std::cout<< "Key " << token_key << " already exist!" << std::endl;
		if (second_key != "") {
			res = global_process_hash_table.emplace(second_key, tmp_obj);
			if (res.second)
				std::cout<< "Second key " << second_key << " successfully inserted." << std::endl;
			else
				std::cout<< "Second key " << second_key << " already exist!" << std::endl;
		}
		// hasDestIP
    }
	/*
	not expected
    if (prev < line.length())
        //wordVector.push_back(line.substr(prev, std::string::npos));
		std::cout<<"we got token:  " << line.substr(prev, std::string::npos) << std::endl;
	}*/
	
	for (auto kv: global_process_hash_table) {
		std::cout<<" " << kv.first << " ->  " << kv.second.name << std::endl;		
	}
}

int get_process_mapping() {
	long lSize;
	// FILE *pFile = popen("/bin/ls -l /Users/musicapp/", "r");
	FILE *pFile = popen("lsof -i", "r");
	if (!pFile)
		return -1;
	fseek (pFile , 0 , SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);
	
	// char* buffer = new char[lSize+1];
	char* buffer = (char *) malloc(lSize+1);
	size_t result = fread(buffer, 1, lSize, pFile);
	buffer[lSize] = '\0';
	/*printf("%s\n", buffer);
	 line_p = fgets(buffer, sizeof(buffer), pFile);
	printf("%s", line_p); */
	pclose(pFile);
	build_hash_table(buffer);
	puts("============================================");	
	free(buffer);
	return 0;
}

/*
	takes port string and update relevant(sender/receiver) byte count in hashtable	
*/
void update_byte_count(std::string port_str, int count, bool isSender) {
	auto it = global_process_hash_table.find(port_str);
	if (it == global_process_hash_table.end()) {
		struct port_info tmp_obj = {"Unknown_App", 0, 0};
		auto res_it = global_process_hash_table.emplace(port_str, tmp_obj);
		if (! res_it.second) {
			std::cout<<"insertion failed!"<<std::endl;
			return ;
		}
		it = res_it.first;
	}
	//std::cout << "Adding "<< count <<  " bytes"<< std::endl;
	if (isSender)
		it->second.iBytes += count;
	else
		it->second.oBytes += count;	
}

void show_process_table() {
	puts("Process Traffic List\n=======================================================");	
	std::cout<< "Process name" << ":\t" << "incoming bytes" << ", " <<  "outgoing bytes" <<  std::endl;
	for (auto kv: global_process_hash_table) {
		if (kv.second.iBytes +  kv.second.oBytes > 0) {
			if (kv.second.name == "Unknown_App")
				std::cout<< kv.second.name <<" ("<<kv.first<<")"<< ":\t" << kv.second.iBytes << ", " <<  kv.second.oBytes <<  std::endl;
			else
				std::cout<< kv.second.name << ":\t" << kv.second.iBytes << ", " <<  kv.second.oBytes <<  std::endl;
		}
	}	
}

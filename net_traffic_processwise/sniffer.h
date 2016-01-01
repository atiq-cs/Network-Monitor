/* This part is in C++ */
#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <unordered_map>

struct port_info {
	std::string name;
	int iBytes;
	int oBytes;
};

const char *plocal_ip = "130.245.188.149";
std::unordered_map<std::string, port_info> global_process_hash_table;

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
	printf("%s\n", buffer);
	/* line_p = fgets(buffer, sizeof(buffer), pFile);
	printf("%s", line_p); */
	pclose(pFile);
	build_hash_table(buffer);
	puts("============================================");	
	free(buffer);
	return 0;
}

void show_process_table() {
	puts("Process Traffic List\n=======================================================");	
	for (auto kv: global_process_hash_table) {
		if (kv.second.iBytes +  kv.second.oBytes > 0)
			std::cout<< kv.second.name << ": " << kv.second.iBytes << ": " <<  kv.second.oBytes <<  std::endl;		
	}
	
}
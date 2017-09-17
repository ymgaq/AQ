#pragma once

#include <cstdio>
#include <unistd.h>
#include <string>
#include <vector>
#include <signal.h>
#include <sys/wait.h>

#include "search.h"


/**
 *  •¶Žš—ñ‚ðdelim‚Å•ª‰ð‚·‚é
 *  Break character string with delimiter.
 */
template <typename List>
void SplitString(const std::string& str, const std::string& delim, List& split_list)
{
    split_list.clear();

    using string = std::string;
    string::size_type pos = 0;

    while(pos != string::npos ){
        string::size_type p = str.find(delim, pos);

        if(p == string::npos){
        	split_list.push_back(str.substr(pos)); break;
        }
        else split_list.push_back(str.substr(pos, p - pos));

        pos = p + delim.size();
    }
}


class Process{

public:
	bool GetLine(std::string* line){
		line->clear();
		for (char c; (c = std::fgetc(stream_from)) != EOF;) {
		if (c == '\n') return true;
			line->push_back(c);
		}
		return false;
	}

	void SendLine(const char* str) {
		std::fprintf(stream_to, "%s\n", str);
	}

	int Start(const char* file, char* const argv[]);

private:
	std::FILE* stream_to;
	std::FILE* stream_from;

};

class Cluster{

public:
	int worker_cnt;
	std::vector<Process> process_list;

	Cluster(){	worker_cnt = cfg_worker_cnt; process_list.clear();}
	~Cluster(){ SendCommand("quit"); }
	void SendCommand(std::string str){
		for(int i=0;i<worker_cnt;++i){
			process_list[i].SendLine(str.c_str());
		}
	}

	void LaunchWorkers();
	int Consult(Tree& tree, std::string log_path);

};

#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>
#include <thread>
#include <shared_mutex>

//TODO: Add word position tracking?

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;

class inverted_index {
private:
	//          word            list of files
	std::map<std::string, std::vector<std::string>> dict;
	//list of files already processed
	std::vector<std::string> files;
	mutable read_write_lock m_rw_lock;
	std::string clean_string(std::string input);

public:
	inverted_index(std::vector<std::string>& filenames);
	void add_file(std::string& filename);
	void debug_list_files();
	std::vector<std::string> search(std::vector<std::string>& word_query);
	
};
#pragma once
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include <sstream>

//TODO: Add word position tracking?

class inverted_index {
private:
	//          word            list of files
	std::map<std::string, std::vector<std::string>> dict;
	//list of files already processed
	std::vector<std::string> files;

	std::string clean_string(std::string input);
public:
	inverted_index(std::vector<std::string>& filenames);
	void add_file(std::string& filename);
	void debug_list_files();
	std::vector<std::string> search(std::vector<std::string>& word_query);
	
};
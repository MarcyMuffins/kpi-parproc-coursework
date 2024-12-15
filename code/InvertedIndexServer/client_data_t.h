#pragma once
#include <string>
#include <vector>
#include <ctime>

struct client_data_t{
	size_t id;
	std::wstring ip;
	time_t connect_time;
	time_t start_time;
	std::vector<std::wstring> query;
	std::vector<std::wstring> result;
	std::vector<int> requested_files;
	time_t disconnect_time;
};
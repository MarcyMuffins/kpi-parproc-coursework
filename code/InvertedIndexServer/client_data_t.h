#pragma once
#include <string>
#include <vector>
#include <ctime>

struct client_data_t{
	size_t id;
	std::wstring ip;
	std::vector<std::wstring> query;
	std::vector<std::wstring> result;
	time_t connect_time;
	time_t disconnect_time;
};
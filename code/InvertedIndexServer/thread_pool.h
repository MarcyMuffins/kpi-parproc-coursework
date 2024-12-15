#pragma once
#include "task_queue.h"
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <string>

using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

class thread_pool
{
public:
	inline thread_pool() = default;
	inline ~thread_pool() { terminate(); }
public:
	void initialize(const size_t worker_count, bool debug_mode);
	void terminate();
	void terminate_now();
	void debug_terminate();
	void routine();
	bool working() const;
	bool working_unsafe() const;
public:
	template <typename task_t, typename... arguments>
	inline size_t add_task(task_t&& task, arguments&&... parameters);
	char get_status(size_t id);
public:
	thread_pool(const thread_pool& other) = delete;
	thread_pool(thread_pool&& other) = delete;
	thread_pool& operator=(const thread_pool& rhs) = delete;
	thread_pool& operator=(thread_pool&& rhs) = delete;
private:
	struct TaskStatus {
		enum Status {
			Waiting,
			Working,
			Finished
		} status;
	};
	mutable read_write_lock m_rw_lock;
	mutable read_write_lock m_status_lock;
	mutable read_write_lock m_print_lock;
	mutable std::condition_variable_any m_task_waiter;
	std::vector<std::thread> m_workers;
	task_queue<std::function<void()>> m_tasks;
	std::unordered_map<size_t, TaskStatus> m_task_status;
	std::unordered_map<size_t, std::chrono::time_point<std::chrono::system_clock>> m_debug_queue_time;
	double m_wait_time = 0.0;
	int m_avg_queue_len = 0;
	int m_avg_read_cnt = 0;
	size_t m_tasks_processed = 0;
	bool m_initialized = false;
	bool m_terminated = false;
	bool m_debug = false;
};

bool thread_pool::working() const
{
	read_lock _(m_rw_lock);
	return working_unsafe();
}
bool thread_pool::working_unsafe() const
{
	return m_initialized && !m_terminated;
}

void thread_pool::initialize(const size_t worker_count, bool debug_mode = false)
{
	write_lock _(m_rw_lock);
	if (m_initialized || m_terminated)
	{
		return;
	}
	m_debug = debug_mode;
	if (m_debug == true) {
		m_print_lock.lock();
		std::wcout << L"STR: Initializing " << worker_count << L" workers.\n" << std::endl;
		m_print_lock.unlock();
	}
	m_workers.reserve(worker_count);
	m_task_status.clear();
	m_debug_queue_time.clear();
	for (size_t id = 0; id < worker_count; id++)
	{
		m_workers.emplace_back(&thread_pool::routine, this);
	}
	m_initialized = !m_workers.empty();
}

void thread_pool::routine()
{
	while (true)
	{
		bool task_acquired = false;
		size_t task_id = -1;
		size_t queue_len = 0;
		std::function<void()> task;
		{
			write_lock _(m_rw_lock);
			auto wait_condition = [this, &task_acquired, &task_id, &task, &queue_len] {
				task_acquired = m_tasks.pop(task, task_id);
				queue_len = m_tasks.size();
				return m_terminated || task_acquired;
				};
			m_task_waiter.wait(_, wait_condition);
		}
		if (m_terminated && !task_acquired)
		{
			return;
		}
		m_task_status[task_id].status = thread_pool::TaskStatus::Status::Working;
		if (m_debug == true) {
			m_print_lock.lock();
			auto time_now = std::chrono::system_clock::now();
			auto elapsed = duration_cast<nanoseconds>(time_now - m_debug_queue_time[task_id]);
			m_wait_time += elapsed.count() * 1e-6;
			m_avg_read_cnt++;
			m_avg_queue_len += queue_len;
			std::wcout << std::setprecision(3) << L"WRK " << task_id << L": Task began working. Queue wait time " << elapsed.count() * 1e-6 << " miliseconds." << std::endl;
			m_print_lock.unlock();
		}
		task();
		m_task_status[task_id].status = thread_pool::TaskStatus::Status::Finished;
		if (m_debug == true) {
			m_print_lock.lock();
			m_tasks_processed++;
			std::wcout << L"END " << task_id << L": Task finished." << std::endl;
			m_print_lock.unlock();
		}
	}
}
template <typename task_t, typename... arguments>
size_t thread_pool::add_task(task_t&& task, arguments&&... parameters)
{
	{
		read_lock _(m_rw_lock);
		if (!working_unsafe()) {
			return -1;
		}
	}
	auto bind = std::bind(std::forward<task_t>(task),
		std::forward<arguments>(parameters)...);
	size_t id = m_tasks.emplace(bind);
	m_task_status[id].status = thread_pool::TaskStatus::Status::Waiting;
	m_avg_read_cnt++;
	m_avg_queue_len += m_tasks.size();
	m_task_waiter.notify_one();
	if (m_debug == true) {
		m_print_lock.lock();
		std::wcout << L"ADD " << id << ": Task was added to the queue." << std::endl;
		m_debug_queue_time[id] = std::chrono::system_clock::now();
		m_print_lock.unlock();
	}
	return id;
}


char thread_pool::get_status(size_t id)
{
	if (m_task_status.count(id) == 0) {
		//std::wcout << L"No such task exists." << std::endl;
		return 'N';
	}
	auto task_status = m_task_status.at(id);
	if (task_status.status == thread_pool::TaskStatus::Status::Waiting) {
		//std::wcout << L"Task " << id << L" is in the task queue." << std::endl;
		return 'Q';
	}
	else if (task_status.status == thread_pool::TaskStatus::Status::Working) {
		//std::wcout << L"Task " << id << L" is being processed." << std::endl;
		return 'P';
	}
	return 'F';

}

void thread_pool::terminate()
{
	if (m_debug == true) {
		m_print_lock.lock();
		std::wcout << L"TRM: Terminate called." << std::endl;
		m_print_lock.unlock();
	}
	{
		write_lock _(m_rw_lock);
		if (working_unsafe())
		{
			if (m_debug == true) {
				m_print_lock.lock();
				std::wcout << L"TRM: Waiting for tasks to finish." << std::endl;
				m_print_lock.unlock();
			}
			m_terminated = true;
		}
		else
		{
			if (m_debug == true) {
				debug_terminate();
			}
			m_workers.clear();
			//m_task_status.clear();
			m_terminated = false;
			m_initialized = false;
			return;
		}
	}
	m_task_waiter.notify_all();
	for (std::thread& worker : m_workers)
	{
		worker.join();
	}
	if (m_debug == true) {
		debug_terminate();
	}
	m_workers.clear();
	//m_task_status.clear();
	m_terminated = false;
	m_initialized = false;
}

inline void thread_pool::terminate_now()
{
	if (m_debug == true) {
		m_print_lock.lock();
		std::wcout << L"TRM: Urgent termination called." << std::endl;
		std::wcout << L"TRM: Clearing the task queue." << std::endl;
		m_print_lock.unlock();
	}
	{
		write_lock _(m_rw_lock);
		m_tasks.clear();
		for (int i = 0; i < m_tasks.task_count(); i++) {
			if (m_task_status.at(i).status == TaskStatus::Status::Waiting) {
				m_task_status.erase(i);
			}
		}
	}
	terminate();
}

void thread_pool::debug_terminate() {
	m_print_lock.lock();

	std::wcout << L"TRM: No tasks left, terminating.\n" << std::endl;
	std::wcout << L"====DEBUG INFO====" << std::endl;
	std::wcout << L"Tasks added: " << m_tasks.task_count() << std::endl;
	std::wcout << L"Tasks processed: " << m_tasks_processed << std::endl;
	std::wcout << std::setprecision(3) << L"Total queue wait time: " << m_wait_time << L" ms" << std::endl;
	std::wcout << std::setprecision(3) << L"Average queue wait time: " << m_wait_time / m_tasks_processed << L" ms" << std::endl;
	std::wcout << std::setprecision(3) << L"Average queue length: " << (double)m_avg_queue_len / (double)m_avg_read_cnt << L" tasks" << std::endl;

	m_print_lock.unlock();
}
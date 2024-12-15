
#include <memory>
#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>
#include <cmath>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>

// int main(int argc, const char** argv)
// {
// 	if (argc != 2)
// 	{
// 		std::cout << "please provide pid as argument" << std::endl;
// 		return 1;
// 	}
// 	int pid = std::stoi(argv[1]);
// 	perf_stack temp_stack(pid, 10, true);
// 	if (!temp_stack.init())
// 	{
// 		std::cout << "init fail" << std::endl;
// 		return 1;
// 	}
// 	temp_stack.collect_call_stack(400);
// 	auto dump_result = temp_stack.dump_call_round();
// 	for (const auto& one_pair : dump_result)
// 	{
// 		std::cout << one_pair.first << ": " << one_pair.second << std::endl;
// 	}
// 	return 1;
// }



	/* Same Size:      "2015122520103046"*/
// 	char timestamp[] = "InvalidTimestamp";
// int fetch_current_timestamp(char *buf, size_t sz)
// {
// 	struct timeval tv;
// 	struct tm tm;
// 	char dt[32];

// 	if (gettimeofday(&tv, NULL) || !localtime_r(&tv.tv_sec, &tm))
// 		return -1;

// 	if (!strftime(dt, sizeof(dt), "%Y%m%d%H%M%S", &tm))
// 		return -1;

// 	scnprintf(buf, sz, "%s%02u", dt, (unsigned)tv.tv_usec / 10000);

// 	return 0;
// }

std::string format_timepoint()
{
	auto milliseconds_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	auto epoch_begin = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>();
	auto cur_timepoint = epoch_begin + std::chrono::milliseconds(milliseconds_since_epoch);
	auto cur_time_t = std::chrono::system_clock::to_time_t(cur_timepoint);

	struct tm* timeinfo;
	char buffer[80];

	timeinfo = localtime(&cur_time_t);

	strftime(buffer, sizeof(buffer), "%Y_%m_%d_%H_%M_%S", timeinfo);
	return std::string(buffer);
}

std::unique_ptr<std::thread> m_perf_record_thread;
std::atomic_bool m_perf_cmd_finish;
std::string m_record_output_file_name;
std::string output_dir=".";
std::uint32_t pid;
int perf_round = 0;
int m_gap_ms = 10;
std::string perf_record_cmd;

void global_stop_signal_handler(int sig, siginfo_t* sinfo, void* ucontext)
{
	std::string kill_cmd = "ps -ef|grep -E \"perf record\"|grep perf_record_data_" + std::to_string(pid) + "| grep -v grep|" + "awk '{print $2}' | xargs kill -9 $1";
	std::cout<<"global_stop_signal_handler do "<<kill_cmd<<std::endl;
	std::system(kill_cmd.c_str());
	std::exit(1);
}

void start_perf_record(int count)
{
	struct sigaction sa;
	sa.sa_sigaction = global_stop_signal_handler;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT | SIGTERM, &sa, NULL) == -1) {
		std::cout << "fail to set sig handler" << std::endl;
		return;
	}

	m_record_output_file_name = "perf_record_data_" + std::to_string(pid) + "_" + format_timepoint() + ".out";
	remove(m_record_output_file_name.c_str());
	int duration_seconds = int(std::ceil(count * m_gap_ms * 1.0 / 1000));
	perf_record_cmd = std::string("perf record -g --call-graph fp -p ") + std::to_string(pid) + std::string(" -F 100 -o ") + output_dir + "/" + m_record_output_file_name + " --switch-output=" + std::to_string(duration_seconds) + "s --switch-max-files=4 ";
	m_perf_cmd_finish.store(false);
	// 开启另外一个线程去执行perf 命令 等待执行完成
	m_perf_record_thread.reset(new std::thread([]()
		{
			std::cout<< "start "<<perf_round<<" "<<perf_record_cmd<<std::endl;
			std::system(perf_record_cmd.c_str());
			std::cout<< "end "<<perf_round<<" "<<perf_record_cmd<<std::endl;
			m_perf_cmd_finish.store(true);
			
		}));
	m_perf_record_thread->detach();
	
}

// 获取下一个perf文件的输出
std::string get_next_perf_output(const std::string& directory, const std::string& output_prefix)
{
	const char* root_dir_c = directory.c_str();
	DIR* pdir = opendir(root_dir_c);
	struct dirent *entry = readdir(pdir);
	std::string result;
	while(entry != nullptr)
	{
		if(entry->d_type != DT_DIR && entry->d_name)
		{
			std::string cur_file_name = entry->d_name;
			if(cur_file_name.find(output_prefix)== 0)
			{
				result = cur_file_name;
				std::cout<<"find new output file "<<result<<std::endl;
				break;
			}
		}
		entry = readdir(pdir);
	}
	closedir(pdir);
	return result;
}


int main(int argc, const char** argv)
{
	if (argc != 2)
	{
		std::cout << "please provide pid as argument" << std::endl;
		return 1;
	}
	pid = std::stoi(argv[1]);
	
	
	int count = 3000;
	std::string last_output_file;
	std::string output_prefix;
	
	while(true)
	{
		if(perf_round == 0)
		{
			start_perf_record(count);
		}
		perf_round++;
		std::cout<<" new perf script round "<<perf_round<<std::endl;
		// 发起之后 直接detach 同时主线程定期获取cpu使用率数据
		const int temp_sleep_gap = 100;
		int accumulate_sleep_time = 0;
		
		for (int i = 0; i * temp_sleep_gap < count * m_gap_ms; i++)
		{

			std::this_thread::sleep_for(std::chrono::milliseconds(temp_sleep_gap));
			accumulate_sleep_time += temp_sleep_gap;
			
		}
		// 预估perf执行完成之后 尝试去读取完成的标记位
		std::string new_perf_output_file;
		while (new_perf_output_file.empty())
		{
			new_perf_output_file = get_next_perf_output(output_dir, m_record_output_file_name + ".");
			if(!new_perf_output_file.empty())
			{
				break;
			}
			std::cout<<"sleep wait for perf to generate outputfile"<<std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(temp_sleep_gap*10));
		}
		
		std::string script_output_file_name = "perf_script_data_" + std::to_string(pid) + "_" + format_timepoint() + ".out";
		remove(script_output_file_name.c_str());
		std::string perf_script_cmd = std::string("perf script -i ") + output_dir + "/" + new_perf_output_file + " -F -dso,-symoff > " +  script_output_file_name;
		std::cout<<perf_script_cmd<<" begin "<<std::endl;
		std::system(perf_script_cmd.c_str());
		std::cout<<perf_script_cmd<<" end "<<std::endl;
		remove(new_perf_output_file.c_str());
		remove(script_output_file_name.c_str());
	}
	return 1;
}
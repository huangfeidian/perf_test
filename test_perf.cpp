
#include <memory>
#include <thread>
#include <iostream>
#include <chrono>
#include <atomic>
#include <cmath>

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
std::string last_record_output_file_name;
std::uint32_t pid;
int perf_round = 0;
int m_gap_ms = 10;

void start_perf_record(int count)
{
	std::string perf_record_cmd;

	m_record_output_file_name = "perf_record_data_" + std::to_string(pid) + "_" + format_timepoint() + ".out";
	remove(m_record_output_file_name.c_str());
	int duration_seconds = int(std::ceil(count * m_gap_ms * 1.0 / 1000));
	perf_record_cmd = std::string("perf record -g --call-graph fp -F 100 -p ") + std::to_string(pid) + std::string(" -o ") + m_record_output_file_name + " sleep " + std::to_string(duration_seconds);
	m_perf_cmd_finish.store(false);
	// 开启另外一个线程去执行perf 命令 等待执行完成
	m_perf_record_thread.reset(new std::thread([perf_record_cmd]()
		{
			std::cout<< "start "<<perf_round<<" "<<perf_record_cmd<<std::endl;
			std::system(perf_record_cmd.c_str());
			std::cout<< "end "<<perf_round<<" "<<perf_record_cmd<<std::endl;
			m_perf_cmd_finish.store(true);
			
		}));
	m_perf_record_thread->detach();
	
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
	while(true)
	{
		if(perf_round == 0)
		{
			start_perf_record(count);
		}
		perf_round++;
		// 发起之后 直接detach 同时主线程定期获取cpu使用率数据
		const int temp_sleep_gap = 100;
		int accumulate_sleep_time = 0;
		for (int i = 0; i * temp_sleep_gap < count * m_gap_ms; i++)
		{

			std::this_thread::sleep_for(std::chrono::milliseconds(temp_sleep_gap));
			accumulate_sleep_time += temp_sleep_gap;
			if(m_perf_cmd_finish.load())
			{
				break;
			}
		}
		// 预估perf执行完成之后 尝试去读取完成的标记位
		while (!m_perf_cmd_finish.load())
		{
			std::cout<<"sleep wait for perf to generate outputfile"<<std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(temp_sleep_gap*10));
		}
		// 因为再次启动perf时会覆盖 m_record_output_file_name 所以这里先保存一下
		std::string last_record_output_file_name = m_record_output_file_name;
		start_perf_record(count);
		
		std::string script_output_file_name = "perf_script_data_" + std::to_string(pid) + "_" + format_timepoint() + ".out";
		remove(script_output_file_name.c_str());
		std::string perf_script_cmd = std::string("perf script -i ") + last_record_output_file_name + " -F -dso,-symoff > " +  script_output_file_name;
		std::cout<<perf_script_cmd<<" begin "<<std::endl;
		std::system(perf_script_cmd.c_str());
		std::cout<<perf_script_cmd<<" end "<<std::endl;
		remove(last_record_output_file_name.c_str());
		remove(script_output_file_name.c_str());
		
	}
	return 1;
}
docker run -it --ulimit memlock=64:64 --cpus 2 --cap-add CAP_PERFMON spiritsaway/perf_test:latest

nohup ./LinuxNoEditor/puerts_unreal_demo.sh -game -nullrhi -log -nothreading &
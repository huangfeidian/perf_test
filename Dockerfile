FROM ubuntu:20.04
RUN apt-get update && apt-get install -y libunwind-dev zlib1g-dev linux-tools-generic unzip
RUN rm -f /usr/bin/perf && ln -s /usr/lib/linux-tools-5.4.0-202/perf /usr/bin/perf
RUN useradd --uid 5000 --create-home --shell /bin/bash dev

USER dev
WORKDIR "/home/dev"
ADD ./LinuxNoEditor/ ./LinuxNoEditor/
ADD ./test_perf ./
# RUN unzip ./LinuxNoEditor.zip && chmod a+x ./LinuxNoEditor/puerts_unreal_demo.sh 
# RUN ./puerts_unreal_demo.sh -game -nullrhi -log -nothreading
ENTRYPOINT [ "/bin/bash" ]
// IO
#include <iostream>
// std::string
#include <string>
// std::vector
#include <vector>
// std::string 转 int
#include <sstream>
// PATH_MAX 等常量
#include <climits>
// POSIX API
#include <unistd.h>
// wait
#include <sys/wait.h>

#include <stdio.h>

std::vector<std::string> split(std::string s, const std::string &delimiter);

int main() {
  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);

  // 用来存储读入的一行命令
  std::string cmdline;

  //存储按"|"分割的命令
  std::vector<std::string> cmdVector;

  //存储当前执行到哪条指令
  int pos = 0;

  //设置管道
  int fd[3];
  if(pipe(fd+1) != 0)
    exit(-1);
  fd[0] = 0;

  // 打印提示符
  std::cout << "# ";
  // 读入一行。std::getline 结果不包含换行符。
  std::getline(std::cin, cmdline);
  //分割
  cmdVector = split(cmdline, "|");

  while (true) {
    if(pos == cmdVector.size())
    {
      if(cmdVector.size() > 1)//需要建立管道
      {
        close(fd[0]);
        if(pipe(fd+1) != 0)
          exit(-1);
        fd[0] = 0;
      }
      
      pos = 0;
      // 打印提示符
      std::cout << "# ";
      // 读入一行。std::getline 结果不包含换行符。
      std::getline(std::cin, cmdline);
      //分割
      cmdVector = split(cmdline, "|");
    }
    
    //裁一下两端空格
    cmdVector[pos].erase(0,cmdVector[pos].find_first_not_of(" "));
    cmdVector[pos].erase(cmdVector[pos].find_last_not_of(" ") + 1);
    // 按空格分割命令为单词
    std::vector<std::string> args = split(cmdVector[pos], " ");
    // 退出
    if (args[0] == "exit") {
      if (args.size() <= 1) {
        return 0;
      }

      // std::string 转 int
      std::stringstream code_stream(args[1]);
      int code = 0;
      code_stream >> code;

      // 转换失败
      if (!code_stream.eof() || code_stream.fail()) {
        std::cout << "Invalid exit code\n";
        continue;
      }

      return code;
    }

    if (args[0] == "pwd") {
      char buf[256];
      if(getcwd(buf, sizeof(buf)) == NULL)
        std::cout << "pwd Error\n";
      else if(pos != cmdVector.size()-1)//不是最后一条命令
      {
        write(fd[2], buf, sizeof(buf));
      }
      else
        std::cout << buf << '\n';
      if(pos != 0)//不是第一条指令，关闭上一条管道读口
      close(fd[0]); 
      fd[0] = fd[1]; 
      if(pos != cmdVector.size()-1)//不是最后一条指令，关闭这一条管道的写口
        close(fd[2]);
      if(pos < cmdVector.size()-2)//倒数第二条之前的指令还需新建管道
      {
        if(pipe(fd+1) != 0)
        {
          std::cout << "pipe Error" << std::endl;
          exit(-1);
        }
      }
      pos++;
      continue;
    }

    if (args[0] == "cd") {
      if(pos != 0)//处理管道：不是第一条指令
      {
        char buf[256];
        read(fd[0], buf, sizeof(buf));//从pipe中读取
        if(chdir(buf) != 0)
          std::cout << "cd Error\n";
      }
      else
      {
        if(chdir(args[1].c_str()) != 0)
          std::cout << "cd Error\n";
      }
      if(pos != 0)//不是第一条指令，关闭上一条管道读口
        close(fd[0]); 
      fd[0] = fd[1]; 
      if(pos != cmdVector.size()-1)//不是最后一条指令，关闭这一条管道的写口
        close(fd[2]);
      if(pos < cmdVector.size()-2)//倒数第二条之前的指令还需新建管道
      {
        if(pipe(fd+1) != 0)
        {
          std::cout << "pipe Error" << std::endl;
          exit(-1);
        }
      }
      pos++;
      continue;
    }
    // 处理外部命令

    // std::vector<std::string> 转 char **
    char *arg_ptrs[args.size() + 1];
    for (auto i = 0; i < args.size(); i++) {
        arg_ptrs[i] = &args[i][0];
    }
    // exec p 系列的 argv 需要以 nullptr 结尾
    arg_ptrs[args.size()] = nullptr;

    pid_t pid = fork();

    if (pid == 0) {
      // 这里只有子进程才会进入
      // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
      // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
      if(pos != 0)//处理管道：不是第一条指令
      {
        dup2(fd[0], 0);
        close(fd[0]);
      }
      if(pos != cmdVector.size()-1)//处理管道：不是最后一条指令
      {
        dup2(fd[2], 1);
        close(fd[2]);
      }
      
      execvp(args[0].c_str(), arg_ptrs);
      // 所以这里直接报错
      exit(255);
    }

    // 这里只有父进程（原进程）才会进入
    int ret = waitpid(pid, NULL, 0);
    if (ret < 0) {
      std::cout << "wait failed";
    }
    
    if(pos != 0)//不是第一条指令，关闭上一条管道读口
      close(fd[0]); 
    fd[0] = fd[1]; 
    if(pos != cmdVector.size()-1)//不是最后一条指令，关闭这一条管道的写口
      close(fd[2]);
    if(pos < cmdVector.size()-2)//倒数第二条之前的指令还需新建管道
    {
      if(pipe(fd+1) != 0)
      {
        std::cout << "pipe Error" << std::endl;
        exit(-1);
      }
    }
    pos++;
  }
}

// 经典的 cpp string split 实现
// https://stackoverflow.com/a/14266139/11691878
std::vector<std::string> split(std::string s, const std::string &delimiter) {
  std::vector<std::string> res;
  size_t pos = 0;
  std::string token;
  while ((pos = s.find(delimiter)) != std::string::npos) {
    token = s.substr(0, pos);
    res.push_back(token);
    s = s.substr(pos + delimiter.length());
  }
  res.push_back(s);
  return res;
}

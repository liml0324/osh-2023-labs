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
//open
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int rst = 0;

//在子进程和父进程间通信的管道
int sig[2];

void setPipe(int pos, int *fd, int size);//处理管道
std::vector<std::string> split(std::string s, const std::string &delimiter);
void divideCmd(std::string &cmd, std::vector<std::string> &args, std::string &path, int &type);
int strlen(char * str, int size);
void shellHandleSIGINT(int a);

int main() {
  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);

  // 用来存储读入的一行命令
  std::string cmdline;

  //存储按"|"分割的命令
  std::vector<std::string> cmdVector;

  //设置信号处理的结构体
  struct sigaction shellSIGINT, childSIGINT;
  shellSIGINT.sa_flags = 0;
  shellSIGINT.sa_handler = shellHandleSIGINT;
  
  //处理Ctrl+C
  sigaction(SIGINT, &shellSIGINT, &childSIGINT);
  signal(SIGTTOU, SIG_IGN);

  //存储当前执行到哪条指令
  int pos = 0;
  //保存管道的文件描述符
  int fd[3];

  // 打印提示符
  std::cout << "# ";
  // 读入一行。std::getline 结果不包含换行符。
  std::getline(std::cin, cmdline);
  //分割
  cmdVector = split(cmdline, "|");

  if(cmdVector.size() > 1)//需要建立管道
  {
    if(pipe(fd+1) != 0)
      exit(-1);
    fd[0] = 0;
    //std::cout << "Make pipe" << std::endl;
  }

  while (true) {
    //tcsetpgrp(0, getpgrp());
    if(pos == cmdVector.size() || rst)
    {
      if(cmdVector.size() > 1)//把上一次的管道关了
        close(fd[0]);
      pos = 0; rst = 0;
      // 打印提示符
      std::cout << "# ";
      // 读入一行。std::getline 结果不包含换行符。
      std::getline(std::cin, cmdline);
      //std::cout << "getline" << std::endl;
      //分割
      cmdVector = split(cmdline, "|");
      //std::cout << "split" << std::endl;

      if(cmdVector.size() > 1)//需要建立管道
      {
        if(pipe(fd+1) != 0)
          exit(-1);
        fd[0] = 0;
        //std::cout << "Make pipe" << std::endl;
      }
      //std::cout << "pipe" << std::endl;
    }
    
    std::vector<std::string> args;
    std::string path;
    int type;
    divideCmd(cmdVector[pos], args, path, type);
    //std::cout << args[args.size()-1] << '\n' << path << std::endl;

    //跳过
    if(args[0] == "")
    {
      pos++;
      continue;
    }

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
      else if(pos != cmdVector.size()-1)//处理管道：不是最后一条命令
      {
        write(fd[2], buf, sizeof(buf));
      }
      else if(type > 0)
      {
        int dpo;
        if(type == 1)//>
        {
          dpo = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
        }
        else//>>
        {
          dpo = open(path.c_str(), O_APPEND | O_WRONLY);
        }
        if(dpo < 0) 
          std::cout << "open Error\n";
        else
        {
          write(dpo, buf, strlen(buf, sizeof(buf)));
        }
      }
      else
        std::cout << buf << '\n';
      setPipe(pos, fd, cmdVector.size());
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

      setPipe(pos, fd, cmdVector.size());
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
      sigaction(SIGINT, &childSIGINT, NULL);
      signal(SIGTTOU, SIG_DFL);
      //std::cout << "Child:" << getpgrp() << std::endl;
      //std::cout << "Child pid:" << getpid() << std::endl;
      if(type >= 0)//需要重定向
      {
        if(type == 0)//重定向输入
        {
          int dpin = open(path.c_str(),  O_RDONLY);
          if(dpin < 0)  
            std::cout << "Redirection Error\n";
          else
          {
            dup2(dpin, 0);
            close(dpin);
          }
        }
        else if(type == 1)//>
        {
          int dpo = open(path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0777);
          if(dpo < 0)  
            std::cout << "Redirection Error\n";
          else
          {
            dup2(dpo, 1);
            close(dpo);
          }
        }
        else//>>
        {
          int dpo = open(path.c_str(), O_APPEND | O_WRONLY);
          if(dpo < 0)  
            std::cout << "Redirection Error\n";
          else
          {
            dup2(dpo, 1);
            close(dpo);
          }
        }
      }
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
    setpgid(pid, pid);
    tcsetpgrp(0, pid);
    //std::cout << "Father:" << getpgrp() << std::endl;
    int status;
    int ret = waitpid(pid, &status, 0);
    // int ret;
    // wait(&ret);
    //std::cout << "Wait end" << std::endl;
    tcsetpgrp(0, getpgrp());
    if (ret < 0) {
      std::cout << "wait failed";
    }
    else if(WIFSIGNALED(status))//子进程被信号终止，结束语句运行并重新输入
    {
      rst = 1;
      std::cout << std::endl;
      //std::cout << status << std::endl;
    }
    

    setPipe(pos, fd, cmdVector.size());
    //std::cout << "set pipe end" << std::endl;
    //getchar();
    pos++;
  }
}

void setPipe(int pos, int *fd, int size)
{
  if(pos != 0)//不是第一条指令，关闭上一条管道读口
  {
    close(fd[0]); 
    //std::cout << "close0" << std::endl;
  }
    
  fd[0] = fd[1]; 
  if(pos != size-1)//不是最后一条指令，关闭这一条管道的写口
  {
    close(fd[2]);
    //std::cout << "close2" << std::endl;
  }
    
  if(pos < size-2 && !rst)//倒数第二条之前的指令还需新建管道（除非命令被中止执行）
  {
    if(pipe(fd+1) != 0)
    {
      std::cout << "pipe Error" << std::endl;
      exit(-1);
    }
  }
  return;
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

int getLast(std::string cmd, int &type)
{
  auto a = cmd.find_last_of("<>");
  if(a == -1) type = -1;
  else if(cmd[a] == '<')  type = 0;//<
  else if(cmd[a] == '>')  
  {
    if(a && cmd[a-1] == '>')  type = 2;//>>
    else  type = 1;//>
  }
  return a;
}

void divideCmd(std::string &cmd, std::vector<std::string> &args, std::string &path, int &type)
{
  auto firstPos = cmd.find_first_of("<>");
  std::string tempStr;
  if(firstPos >= 0 && firstPos < cmd.length())
  {
    tempStr = cmd.substr(0, firstPos);//截取除掉重定向部分之后的命令
    auto lastPos = getLast(cmd, type);
    path = cmd.substr(lastPos + 1, cmd.length() - lastPos - 1);
  }
  else
  {
    tempStr = cmd;
    type = -1;
    path = "";
  }
  //裁一下两端空格
  path.erase(0,path.find_first_not_of(" "));
  path.erase(path.find_last_not_of(" ") + 1);
  //裁一下两端空格
  tempStr.erase(0,tempStr.find_first_not_of(" "));
  tempStr.erase(tempStr.find_last_not_of(" ") + 1);
  args = split(tempStr, " ");
  return;
}

int strlen(char * str, int size)
{
  int i = 0;
  while(str[i] != 0 && i < size)
    i++;
  return i;
}

void shellHandleSIGINT(int a)
{
  //rst = 1;
  int num = std::cin.rdbuf()->in_avail();
  std::cin.ignore(num);
  std::cout << '\n' << "# ";
  std::cout.flush();
  //std::cout << "SIGINT" << std::endl;
}
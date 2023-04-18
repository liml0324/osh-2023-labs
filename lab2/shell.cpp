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
//#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//stdlib
#include <stdlib.h>

std::vector<std::string> split(std::string s, const std::string &delimiter);
void divideCmd(std::string &cmd, std::vector<std::string> &args, std::string &path, int &type);
void getBackstage(std::string &cmdline, int &backstage);
int strlen(char * str, int size);
void shellHandleSIGINT(int a);
void Wait();

int main() {
  // 不同步 iostream 和 cstdio 的 buffer
  std::ios::sync_with_stdio(false);

  // 用来存储读入的一行命令
  std::string cmdline;

  //存储按"|"分割的命令
  std::vector<std::string> cmdVector;

  //历史命令
  std::vector<std::string> historyCmd;

  //下一次执行历史命令
  int nextHistory = 0;

  //是否在后台执行
  int backstage = 0;

  //设置信号处理的结构体
  struct sigaction shellSIGINT, childSIGINT;
  shellSIGINT.sa_flags = 0;
  shellSIGINT.sa_handler = shellHandleSIGINT;
  
  //处理Ctrl+C
  sigaction(SIGINT, &shellSIGINT, &childSIGINT);
  signal(SIGTTOU, SIG_IGN);

  int (*fd)[2];

  while (true) {
    //perror("!");
    // 打印提示符
    if(nextHistory == 0)//执行新命令
    {
      //获取方向键输入

      char buf[256];
      if(getcwd(buf, sizeof(buf)) == NULL)
        ;
      else
        std::cout << buf;
      std::cout << " $ ";
      std::cout.flush();
      // 读入一行。std::getline 结果不包含换行符。
      std::getline(std::cin, cmdline);
      if(std::cin.eof())
      {
        std::cout << "\nexit" << std::endl;
        exit(0);
      }
      //记录历史命令
      historyCmd.push_back(cmdline);
    }
    else//执行历史命令
    {
      cmdline = historyCmd[nextHistory-1];
      historyCmd.push_back(cmdline);
      std::cout << cmdline << std::endl;
      nextHistory = 0;
    }
    
    //检查是否挂后台
    getBackstage(cmdline, backstage);
    //分割
    cmdVector = split(cmdline, "|");
    pid_t pid = fork();
    if(pid == 0)//子进程
    {
      sigaction(SIGINT, &childSIGINT, NULL);
      signal(SIGTTOU, SIG_DFL);

      if(cmdVector.size() > 1)//需要建立管道
      {
        fd = (int (*)[2])malloc(2 * (cmdVector.size()-1) * sizeof(int));
        for(int i = 0; i < cmdVector.size()-1; i++)
        {
          pipe(fd[i]);
        }
      }
      
      if(backstage == 1)//后台执行
      {
        umask(0);
        int fd0 = 0;
        signal(SIGPIPE, SIG_IGN);
        setsid();
        if((fd0 = open("/dev/null", O_RDWR)) != -1)
        {
          //std::cout << fd0 << std::endl;
          dup2(fd0, STDIN_FILENO);
          dup2(fd0, STDOUT_FILENO);
          dup2(fd0, STDERR_FILENO);
          if(fd0 > STDERR_FILENO)
          {
            close(fd0);
          }
        }
        else
          std::cout << "fd0 error!" << std::endl;
      }
      
      std::vector<std::string> args;
      std::string path;//重定向路径
      int type;//重定向类型
      int pgid = 0;
      pid_t pid_;

      for(int i = 0; i < cmdVector.size(); i++)
      {
        divideCmd(cmdVector[i], args, path, type);
        pid_ = fork();
        if(pid_ == 0)
        {
          if(args[0] == "" || args[0] == "wait" || args[0] == "exit" || args[0] == "cd" || args[0] == "pwd" \
          || args[0] == "history" || args[0].substr(0, 1) == "!")//由父进程处理
          {
            return 0;
          }

          // 处理外部命令

          // std::vector<std::string> 转 char **
          char *arg_ptrs[args.size() + 1];
          for (auto i = 0; i < args.size(); i++) {
              arg_ptrs[i] = &args[i][0];
          }
          // exec p 系列的 argv 需要以 nullptr 结尾
          arg_ptrs[args.size()] = nullptr;

          if(cmdVector.size() > 1)//处理管道
          {
            if(i != 0)//不是第一条命令
            {
              dup2(fd[i-1][0], 0);
              close(fd[i-1][0]);
            }
            if(i != cmdVector.size()-1)//不是最后一条命令
            {
              dup2(fd[i][1], 1);
              close(fd[i][1]);
            }
          }
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

          // execvp 会完全更换子进程接下来的代码，所以正常情况下 execvp 之后这里的代码就没意义了
          // 如果 execvp 之后的代码被运行了，那就是 execvp 出问题了
          execvp(args[0].c_str(), arg_ptrs);
          // 所以这里直接报错
          // std::cout << "Error" << std::endl;
          exit(255);
        }
        else
        {
          if(i > 0)
            close(fd[i-1][0]);
          if(i < cmdVector.size()-1)
            close(fd[i][1]);
        }
      }
      
      while(wait(NULL) >= 0);
      //perror("?");
      if(cmdVector.size() > 1)
        free(fd);
      return 0;
    }
    else//父进程
    {
      if(backstage == 0)//不在后台运行才设置进程组
      {
        setpgid(pid, pid);
        tcsetpgrp(0, pid);
        int status = 0;
        waitpid(pid, &status, 0);//等待执行完毕
        if(WIFSIGNALED(status))
          if(WTERMSIG(status) == SIGINT)
            std::cout << std::endl;
      }
      tcsetpgrp(0, getpgrp());
      std::vector<std::string> args;
      std::string path;
      int type;


      for(int i = 0; i < cmdVector.size(); i++)
      {
        divideCmd(cmdVector[i], args, path, type);
        if(args[0] == "wait")
        {
          Wait();
          continue;
        }
        if(args[0] == "exit")
        {
          if(args.size() <= 1)
            exit(0);
          std::stringstream code_stream(args[1]);
          int code = 0;
          code_stream >> code;

          // 转换失败
          if (!code_stream.eof() || code_stream.fail()) {
            std::cout << "Invalid exit code\n";
            continue;
          }

          exit(code);
        }
        if(args[0] == "cd")
        {
          if(args.size() > 1)
          {
            if(chdir(args[1].c_str()) != 0)
              std::cout << "cd error!" << std::endl;
          }
          else//cd无参数，默认进入家目录
          {
            char *path = getenv("HOME");
            if(chdir(path) != 0)
              std::cout << "cd error!" << std::endl;
          }
          continue;
        }
        if(args[0] == "pwd")
        {
          char buf[256];
          if(getcwd(buf, sizeof(buf)) == NULL)
            std::cout << "pwd Error\n";
          else
            std::cout << buf << std::endl;
          continue;
        }
        if(args[0] == "history")
        {
          if(args.size() > 1)
          {
            std::stringstream code_stream(args[1]);
            int num = 0;
            code_stream >> num;

            // 转换失败
            if (!code_stream.eof() || code_stream.fail()) {
              std::cout << "Invalid history code\n";
              continue;
            }
            if(num > historyCmd.size())
              num = historyCmd.size();
            for(int i = historyCmd.size()-num; i < historyCmd.size(); i++)
              std::cout << i+1 << "\t\t" << historyCmd[i] << std::endl;
          }
          else
          {
            for(int i = 0; i < historyCmd.size(); i++)
              std::cout << i+1 << "\t\t" << historyCmd[i] << std::endl;
          }
        }
        if(args[0].substr(0, 1) == "!")
        {
          historyCmd.pop_back();//!!和!n命令不加入历史记录
          if(args[0] == "!!")//执行上一条命令
          {
            nextHistory = historyCmd.size();
          }
          else//执行标号为num的命令
          {
            std::stringstream code_stream(args[0].substr(1));
            int num = 0;
            code_stream >> num;
            // 转换失败
            if (!code_stream.eof() || code_stream.fail()) {
              std::cout << "Invalid history code\n";
              continue;
            }
            if(num > historyCmd.size())
            {
              std::cout << "No such command in history\n";
              continue;
            }
            nextHistory = num;
          }
        }
      }
    }
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

int getLast(std::string cmd, int &type)//获得最后一个重定向符号的位置，以及重定向类型
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

void divideCmd(std::string &cmd, std::vector<std::string> &args, std::string &path, int &type)//分割一条命令，返回参数，重定向类型和路径（如果有的话）
{
  //裁一下两端空格
  cmd.erase(0,cmd.find_first_not_of(" "));
  cmd.erase(cmd.find_last_not_of(" ") + 1);
  auto firstPos = cmd.find_first_of("<>");
  std::string tempStr;
  if(cmd.size() > 0 && cmd.substr(cmd.size()-1) == "&")
    cmd.erase(cmd.size()-1);
  if(firstPos >= 0 && firstPos < cmd.length())
  {
    tempStr = cmd.substr(0, firstPos);//截取除掉重定向部分之后的命令
    auto lastPos = getLast(cmd, type);//只有最后一个重定向符号有效
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

void getBackstage(std::string &cmdline, int &backstage)
{
  //裁一下两端空格
  cmdline.erase(0,cmdline.find_first_not_of(" "));
  cmdline.erase(cmdline.find_last_not_of(" ") + 1);

  if(cmdline.size() > 0 && cmdline.substr(cmdline.size()-1, 1) == "&")
    backstage = 1;
  else
    backstage = 0;
}

int strlen(char * str, int size)
{
  int i = 0;
  while(str[i] != 0 && i < size)
    i++;
  return i;
}

void shellHandleSIGINT(int a)//shell中处理SIGINT
{
  //rst = 1;
  int num = std::cin.rdbuf()->in_avail();
  std::cin.ignore(num);
  std::cin.clear();
  std::cout << '\n';
  char buf[256];
  if(getcwd(buf, sizeof(buf)) == NULL)
    ;
  else
    std::cout << buf;
  std::cout << " $ ";
  std::cout.flush();
  //std::cout << "SIGINT" << std::endl;
}

void Wait()
{
  int signal;
  while(wait(&signal) >= 0);
}
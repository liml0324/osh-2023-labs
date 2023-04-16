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

std::vector<std::string> split(std::string s, const std::string &delimiter);
void divideCmd(std::string &cmd, std::vector<std::string> &args, std::string &path, int &type);
int strlen(char * str, int size);
void shellHandleSIGINT(int a);
void cd(int pos, std::vector<std::string> &cmdVector, std::string path, int type, int (*fd)[2]);
void pwd(int pos, std::vector<std::string> &cmdVector, std::string path, int type, int (*fd)[2]);

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

  int (*fd)[2];

  while (true) {
    //perror("!");
    if(cmdVector.size() > 1)
      free(fd);
    // 打印提示符
    // fflush(stdout);
    std::cout << "# ";
    std::cout.flush();
    // 读入一行。std::getline 结果不包含换行符。
    std::getline(std::cin, cmdline);
    //分割
    cmdVector = split(cmdline, "|");

    if(cmdVector.size() > 1)//需要建立管道
    {
      fd = (int (*)[2])malloc(2 * (cmdVector.size()-1) * sizeof(int));
      for(int i = 0; i < cmdVector.size()-1; i++)
      {
        pipe(fd[i]);
      }
    }
    
    std::vector<std::string> args;
    std::string path;
    int type;
    int pgid;

    for(int i = 0; i < cmdVector.size(); i++)
    {
      divideCmd(cmdVector[i], args, path, type);
      pid_t pid = fork();
      if(pid == 0)
      {
        //raise(SIGTSTP);
        sigaction(SIGINT, &childSIGINT, NULL);
        signal(SIGTTOU, SIG_DFL);
        // setpgid(getpid(), getpid());

        if(args[0] == "")//空命令
        {
          if(cmdVector.size() > 1)
          {
            if(i != 0)//处理管道：不是第一条指令
            {
              close(fd[i-1][0]);
            }
            if(i != cmdVector.size()-1)//不是最后一条命令
            {
              close(fd[i][0]);
            }
          }
          return 0;
        }
        if(args[0] == "exit")//exit
        {
          return 0;
        }
        if(args[0] == "cd")//cd
        {
          cd(i, cmdVector, path, type, fd);
          return 0;
        }
        if(args[0] == "pwd")//pwd
        {
          pwd(i, cmdVector, path, type, fd);
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
      else//父进程
      {
        //kill(pid, SIGSTOP);
        if(i == 0)
          pgid = pid;
        setpgid(pid, pgid);
        if(i == 0)
          tcsetpgrp(0, pgid);
        //kill(pid, SIGCONT);
        
        if(i == cmdVector.size()-1)
          tcsetpgrp(0, pid);
        
        if(cmdVector.size() > 1)
        {
          if(i != 0)
            close(fd[i-1][0]);
          if(i != cmdVector.size()-1)
            close(fd[i][1]);
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
      }
    }
    
    int ret;
    //ret = waitpid(-pgid, NULL, 0);
    wait(&ret);
    //perror("?");
    if(ret < 0)
      std::cout << "Wait failed" << std::endl;
    tcsetpgrp(0, getpgrp());
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
  auto firstPos = cmd.find_first_of("<>");
  std::string tempStr;
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
  std::cout << '\n' << "# ";
  std::cout.flush();
  //std::cout << "SIGINT" << std::endl;
}

void pwd(int pos, std::vector<std::string> &cmdVector, std::string path, int type, int (*fd)[2])
{
  char buf[256];
  if(getcwd(buf, sizeof(buf)) == NULL)
    std::cout << "pwd Error\n";
  else if(cmdVector.size() > 1)//处理管道
  {
    if(pos != 0)//不是第一条命令
    {
      close(fd[pos-1][0]);//关闭前一个读口
    }
    if(pos != cmdVector.size()-1)//不是最后一条命令
    {
      write(fd[pos][1], buf, strlen(buf, sizeof(buf)));
      close(fd[pos][1]);//关闭后一个读口
    }
    
  }
  else if(type > 0)//重定向
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
    std::cout << buf << std::endl;
}

void cd(int pos, std::vector<std::string> &cmdVector, std::string path, int type, int (*fd)[2])
{
  if(cmdVector.size() > 1)
  {
    if(pos != 0)//处理管道：不是第一条指令
    {
      char buf[256];
      read(fd[pos-1][0], buf, sizeof(buf));//从pipe中读取
      close(fd[pos-1][0]);
      if(chdir(buf) != 0)
        std::cout << "cd Error\n";
    }
    if(pos != cmdVector.size()-1)//不是最后一条命令
    {
      close(fd[pos][0]);
    }
  }
  else
  {
    if(chdir(path.c_str()) != 0)
      std::cout << "cd Error\n";
  }
}
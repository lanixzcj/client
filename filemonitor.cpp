#include "filemonitor.h"


char file_monitor_cache[FILE_CACHE_LEN][MAX_G_STRING_SIZE] = {NULL};
int file_monitor_pos = 0;
int last_fetch = 0;
sem_t file_full;
sem_t file_empty;
sem_t file_mutex;


int inotify_fd; //global int that stores the inotify_init
int MONITOR_TYPE =  IN_CREATE | IN_DELETE | IN_DELETE_SELF | \
                    IN_ACCESS | IN_ATTRIB | IN_MODIFY | IN_OPEN;
char * monitor_dirs[1024];


void write_filemonitor_2cache(const char *source)
{
    sem_wait(&file_empty);
    sem_wait(&file_mutex);
    strcpy(file_monitor_cache[file_monitor_pos++], source);
    int val = sem_getvalue(&file_full ,&val);
    if(val == 0) sem_post(&file_full);
    sem_post(&file_mutex);


}
void read_filemonitor_4Cache(char dest[][MAX_G_STRING_SIZE], int &cur)
{

    sem_wait(&file_full);
    sem_wait(&file_mutex);
    while (last_fetch < file_monitor_pos) {
      printf("before strcpy in read_filemonitor_4Cache: %s\n", file_monitor_cache[last_fetch]);
      strcpy(&dest[cur][0], file_monitor_cache[last_fetch]);
      last_fetch++;
      cur++;
      sem_post(&file_empty);
    }
    file_monitor_pos = last_fetch = 0;
    sem_post(&file_mutex);

}




void get_all_dir(string root_monitor)
{
    DIR * dir;
    struct dirent *entry;
    struct stat statbuf;
    queue<string> dir_monitor_q;

    dir_monitor_q.push(root_monitor);
    string tmp;

    while(!dir_monitor_q.empty())
    {
        tmp = dir_monitor_q.front();
        dir_monitor_q.pop();

        const char *tmp_cstring = tmp.c_str();

        dir = opendir(tmp_cstring);
        if(!dir)
        {
            fprintf (stderr, "Cannot open directory '%s': %s\n",
                     tmp_cstring, strerror (errno));
            exit (EXIT_FAILURE);
        }

        while((entry = readdir(dir)) != NULL)
        {

            if(!strncmp(entry->d_name, ".", 1) || !strncmp(entry->d_name, "..", 2) || !strncmp(entry->d_name, "/", 1))
                continue;

#ifdef _DIRENT_HAVE_D_TYPE
            switch (entry->d_type) {
              case DT_DIR:
                  {
                    char path[PATH_MAX];
                    int path_length = snprintf(path, PATH_MAX, "%s/%s", tmp_cstring, entry->d_name);
                    if(path_length >= PATH_MAX )
                    {
                      fprintf (stderr, "Path length has got too long.\n");
                      exit (EXIT_FAILURE);
                    }

                    int wd = inotify_add_watch(inotify_fd, path, MONITOR_TYPE);
                    monitor_dirs[wd] = (char *)malloc(sizeof(char) * MAX_G_STRING_SIZE);
        				    strcpy(monitor_dirs[wd], path);
                    string sub_dir(path);
                    dir_monitor_q.push(sub_dir);
                    printf("enter the sub_dir %s\n", path);
                  }
            }
#else
            stat(entry->d_name, &statbuf);
            if(S_ISDIR(statbuf.st_mode))
            {
              char path[PATH_MAX];
              int path_length = snprintf(path, PATH_MAX, "%s/%s", tmp_cstring, entry->d_name);
              if(path_length >= PATH_MAX )
              {
                fprintf (stderr, "Path length has got too long.\n");
                exit (EXIT_FAILURE);
              }

              int wd = inotify_add_watch(inotify_fd, path, MONITOR_TYPE);
              monitor_dirs[wd] = (char *)malloc(sizeof(char) * MAX_G_STRING_SIZE);
              strcpy(monitor_dirs[wd], path);
              string sub_dir(path);
              dir_monitor_q.push(sub_dir);
              printf("enter the read sub_dir func%s\n", path);
            }
#endif

          }
        closedir(dir);
      }



  }


void monitor_files(void *arg)
{
  int length, i = 0;
  // int inotify_fd;
  // int wd;
  char buffer[EVENT_BUF_LEN];
  struct passwd * pwd;
  pwd = getpwuid(getuid());
  char root[30];
  bzero(root, sizeof(char) * 30);
  strcpy(root , "/home/");
  strcat(root, pwd->pw_name);
  string root_dir(root);

  const char * MonitorDir = root_dir.c_str();

  inotify_fd = inotify_init();
  if (inotify_fd < 0) {
    perror("inotify_init");
  }

  /// 添加监测目录(目录事先存在)
  /// wd 与 工作目录 对应, 要保存起来才能知道 wd 对应哪个 监控目录
  int wd = inotify_add_watch(inotify_fd, MonitorDir, MONITOR_TYPE);
  monitor_dirs[wd] = (char *)MonitorDir;
  /// add all childs directory
  get_all_dir(root_dir);
  //init timestamp and log char array
  char time_c[15];
  char log[MAX_G_STRING_SIZE];

  // 循环监听
  while(true)
  {
    i = 0;
    length = read(inotify_fd, buffer, EVENT_BUF_LEN);

    if (length < 0) {
      perror("read");
    }

    while (i < length) {
      struct inotify_event *event = (struct inotify_event *) &buffer[i];
      if (event->len) {
                /// 新建 目录/文件
        if (event->mask & IN_CREATE) {
              if (event->mask & IN_ISDIR) {

                sprintf(time_c,"%ld",time(NULL));
                snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-create", time_c, monitor_dirs[event->wd], event->name);
                printf("log: %s\n", log);
                write_filemonitor_2cache(log);
                //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);

    						char path[PATH_MAX];

                snprintf(path, PATH_MAX, "%s/%s", monitor_dirs[event->wd], event->name);

    						int wd = inotify_add_watch(inotify_fd, path, MONITOR_TYPE);
                monitor_dirs[wd] = (char *)malloc(sizeof(char) * MAX_G_STRING_SIZE);
    				    strcpy(monitor_dirs[wd], path);
                printf("adding the path to watch list%s\n", path);
              } else {
                sprintf(time_c,"%ld",time(NULL));
                snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-create", time_c, monitor_dirs[event->wd], event->name);
                printf("log: %s\n", log);
                write_filemonitor_2cache(log);
                //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);
                // printf("New file %s/%s created.\n", monitor_dirs[event->wd], event->name);
              }
                /// 删除 目录/文件
        } else if ((event->mask & IN_DELETE) || (event->mask & IN_DELETE_SELF)) {
              if (event->mask & IN_ISDIR) {
                sprintf(time_c,"%ld",time(NULL));
                snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-delete", time_c, monitor_dirs[event->wd], event->name);
                printf("log: %s\n", log);
                write_filemonitor_2cache(log);
                //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);
                // write_filemonitor_2cache(log);
                // printf("Directory %s/%s deleted.\n",  monitor_dirs[event->wd], event->name);
              } else {
                sprintf(time_c,"%ld",time(NULL));
                snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-delete", time_c, monitor_dirs[event->wd], event->name);
                printf("log: %s\n", log);
                write_filemonitor_2cache(log);
                //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);
                // write_filemonitor_2cache(log);
                // printf("File %s/%s deleted.\n",  monitor_dirs[event->wd], event->name);
              }
                /// 访问 内容修改 属性修改
        // } else if (event->mask & IN_ACCESS) {
        //             printf("File %s/%s was accessed.\n",  monitor_dirs[event->wd], event->name);
        } else if (event->mask & IN_ATTRIB) {
          sprintf(time_c,"%ld",time(NULL));
          snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-permc", time_c, monitor_dirs[event->wd], event->name);
          printf("log: %s\n", log);
          write_filemonitor_2cache(log);
          //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);
          // write_filemonitor_2cache(log);
          // printf("File %s/%s permission was changed.\n",  monitor_dirs[event->wd], event->name);
        } else if (event->mask & IN_MODIFY) {
                if (event->mask & IN_ISDIR) {
                  sprintf(time_c,"%ld",time(NULL));
                  snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-modified", time_c, monitor_dirs[event->wd], event->name);
                  printf("log: %s\n", log);
                  write_filemonitor_2cache(log);
                  //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);
                  // write_filemonitor_2cache(log);
                // printf("Directory %s/%s was modified.\n",  monitor_dirs[event->wd], event->name);
              } else {
                sprintf(time_c,"%ld",time(NULL));
                snprintf(log, MAX_G_STRING_SIZE, "%s-%s/%s-modified", time_c, monitor_dirs[event->wd], event->name);
                printf("log: %s\n", log);
                write_filemonitor_2cache(log);
                //printf("cache string : %s\n", file_monitor_cache[file_monitor_pos-1]);
                // write_filemonitor_2cache(log);
                // printf("File %s/%s was modified.\n",  monitor_dirs[event->wd], event->name);
              }
        }
                /// 打开关闭
        // } else if (event->mask & IN_OPEN) {
        //   if (event->mask & IN_ISDIR) {
        //     printf("Directory %s/%s was opened.\n",  monitor_dirs[event->wd], event->name);
        //   } else {
        //     printf("File %s/%s was opened.\n",  monitor_dirs[event->wd], event->name);
        //   }
        //
        // } else if (event->mask & IN_CLOSE_WRITE) {
        //             printf("File %s/%s was closed with write.\n",  monitor_dirs[event->wd], event->name);
        // } else if (event->mask & IN_CLOSE_NOWRITE) {
        //             printf("File %s/%s was closed without write.\n",  monitor_dirs[event->wd], event->name);
        // }
      }
      i += EVENT_SIZE + event->len;
    }

  }

  int count_dir = 0;
  while (monitor_dirs[count_dir] != NULL) {
    inotify_rm_watch(inotify_fd, count_dir);
    count_dir++;
  }
  close(inotify_fd);
  printf("clear the fd and associated datas\n");
}

void remove_monitor()
{
  int count_dir = 0;
  while (monitor_dirs[count_dir] != NULL) {
    inotify_rm_watch(inotify_fd, count_dir);
    free(monitor_dirs[count_dir]);
    count_dir++;
  }
  close(inotify_fd);
  printf("clear the fd and associated datas\n");
}

// int main(int argc, char const *argv[]) {
//
//   sem_init(&file_full, 0, 0);
//   sem_init(&file_empty, 0, 5);
//   sem_init(&file_mutex, 0, 1);
//
//   monitor_files();
//   sleep(2);
//   remove_monitor();
//   return 0;
// }

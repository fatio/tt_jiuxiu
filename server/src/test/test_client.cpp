/*================================================================
 *   Copyright (C) 2014 All rights reserved.
 *
 *   文件名称：test_client.cpp
 *   创 建 者：Zhang Yuanhao
 *   邮    箱：bluefoxah@gmail.com
 *   创建日期：2014年12月30日
 *   描    述：
 *
 ================================================================*/

#include <vector>
#include <iostream>
#include <map>
#include <wait.h>
#include "ClientConn.h"
#include "netlib.h"
#include "TokenValidator.h"
#include "Thread.h"
#include "IM.BaseDefine.pb.h"
#include "IM.Buddy.pb.h"
#include "Common.h"
#include "Client.h"
#include "ConfigFileReader.h"
#include "daemon.h"
#include "setproctitle.h"
#include "util.h"
#include "EventDispatch.h"
#include "tt_core.h"
#include "tt_channel.h"
using namespace std;

#define MAX_LINE_LEN	1024
string g_cmd_string[10];
int g_cmd_num;
CClient* g_pClient = NULL;
#define tt_nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK)


sig_atomic_t  tt_sigalrm;
sig_atomic_t  tt_terminate;
sig_atomic_t  tt_quit;
sig_atomic_t  tt_sigio;
sig_atomic_t  tt_reap;
sig_atomic_t  tt_debug_quit;
sig_atomic_t  tt_user1;
int    		  tt_exiting;
sig_atomic_t  tt_reconfigure;
sig_atomic_t  tt_reopen;
int 		     tt_process;
int			  tt_process_slot;
int     		  tt_channel;
int     		  tt_last_process;
//static char **tt_os_environ;

int              tt_argc;
char           **tt_argv;
char           **tt_os_argv;

#define 		  TT_PROCESS_MASTER     1
#define       TT_PROCESS_WORKER     2

#define 		  TT_CMD_OPEN_CHANNEL   1
#define 		  TT_CMD_CLOSE_CHANNEL  2
#define       TT_CMD_QUIT           3
#define       TT_CMD_TERMINATE      4
#define       TT_CMD_USER1			   5

#define TT_PROCESS_NORESPAWN     -1
#define TT_PROCESS_JUST_SPAWN    -2
#define TT_PROCESS_RESPAWN       -3
#define TT_PROCESS_JUST_RESPAWN  -4
#define TT_PROCESS_DETACHED      -5


#define TT_SHUTDOWN_SIGNAL      QUIT
#define TT_TERMINATE_SIGNAL     TERM
#define tt_signal_helper(n)     SIG##n
#define tt_signal_value(n)      tt_signal_helper(n)
#define tt_value_helper(n)   	  #n
#define tt_value(n)          	   tt_value_helper(n)
#define TT_MAX_PROCESSES        1024


void tt_signal_handler(int signo);

typedef struct {
    int     signo;
    char   *signame;
    char   *name;
    void  (*handler)(int signo);
} tt_signal_t;

tt_signal_t  signals[] = {
	{ tt_signal_value(TT_TERMINATE_SIGNAL),
			"SIG" tt_value(TT_TERMINATE_SIGNAL),
	      "stop",
		  tt_signal_handler },

	{ tt_signal_value(TT_SHUTDOWN_SIGNAL),
		  "SIG" tt_value(TT_SHUTDOWN_SIGNAL),
		  "quit",
		  tt_signal_handler },

	{ SIGALRM, "SIGALRM", "", tt_signal_handler },

	{ SIGINT, "SIGINT", "", tt_signal_handler },

	{ SIGIO, "SIGIO", "", tt_signal_handler },

	{ SIGCHLD, "SIGCHLD", "", tt_signal_handler },

	{ SIGSYS, "SIGSYS, SIG_IGN", "", SIG_IGN },

	{ SIGPIPE, "SIGPIPE, SIG_IGN", "", SIG_IGN },

	{ SIGUSR1, "SIGUSER1", "",tt_signal_handler },

	{ 0, NULL, "", NULL }
};


int tt_init_signals()
{
    tt_signal_t      *sig;
    struct sigaction   sa;

    for (sig = signals; sig->signo != 0; sig++) {
        memset(&sa, 0,sizeof(struct sigaction));
        sa.sa_handler = sig->handler;
        sigemptyset(&sa.sa_mask);
        if (sigaction(sig->signo, &sa, NULL) == -1) {
            FATAL("sigaction(%s) failed", sig->signame);
            return -1;
        }
    }
    return 0;
}

void split_cmd(char* buf)
{
	int len = strlen(buf);
	string element;

	g_cmd_num = 0;
	for (int i = 0; i < len; i++) {
		if (buf[i] == ' ' || buf[i] == '\t') {
			if (!element.empty()) {
				g_cmd_string[g_cmd_num++] = element;
				element.clear();
			}
		} else {
			element += buf[i];
		}
	}

	// put the last one
	if (!element.empty()) {
		g_cmd_string[g_cmd_num++] = element;
	}
}

void print_help()
{
	printf("Usage:\n");
    printf("login <user_id> <app_id> <domain_id>\n");
    /*
	printf("connect serv_ip serv_port user_name user_pass\n");
    printf("getuserinfo\n");
    printf("send toId msg\n");
    printf("unreadcnt\n");
     */
	printf("close\n");
	printf("quit\n");
}

CClient* doLogin(uint32_t user_id, uint32_t app_id, uint32_t domain_id)
{
	CClient* pClient = NULL;
    try
    {
    	pClient = new CClient(user_id, app_id, domain_id);
    }
    catch(...)
    {
        printf("get error while alloc memory\n");
        PROMPTION;
        return NULL;
    }
   pClient->connect();

   return pClient;
}


void exec_cmd()
{
    if (g_cmd_num == 0) {
        return;
    }
    
    if(g_cmd_string[0] == "login")
    {
        if(g_cmd_num == 4)
        {
            doLogin(atoi(g_cmd_string[1].c_str()), atoi(g_cmd_string[2].c_str()), atoi(g_cmd_string[3].c_str()));
        }
        else
        {
            print_help();
        }
    }
    else if (strcmp(g_cmd_string[0].c_str(), "close") == 0) {
        g_pClient->close();
    }
    else if (strcmp(g_cmd_string[0].c_str(), "quit") == 0) {
		exit(0);

    }
    else if (strcmp(g_cmd_string[0].c_str(), "send") == 0) {
        if (g_cmd_num == 3) {
            if (g_pClient == NULL || g_pClient->isLogined() == false) {
	        printf("You must login before sending message.\n");
            } else {
               g_pClient->sendMsg(atoi(g_cmd_string[1].c_str()), IM::BaseDefine::MSG_TYPE_SINGLE_TEXT, g_cmd_string[2]);
            }
        } else {
            print_help();
        }
    }
    else if(strcmp(g_cmd_string[0].c_str(), "list") == 0)
    {
        printf("+---------------------+\n");
        printf("|        用户名       |\n");
        printf("+---------------------+\n");
#if 0
        CMapNick2User_t mapUser = g_pClient->getNick2UserMap();
        auto it = mapUser.begin();
        for(;it!=mapUser.end();++it)
        {
            uint32_t nLen = 21 - it->first.length();
            printf("|");
            for(uint32_t i=0; i<nLen/2; ++it)
            {
                printf(" ");
            }
            printf("%s", it->first.c_str());
            for(uint32_t i=0; i<nLen/2; ++it)
            {
                printf(" ");
            }
            printf("|\n");
            printf("+---------------------+\n");
        }
#endif
    }
    else {
		print_help();
	}
}


std::map<int,CClient*> g_mapClientConn;
typedef std::map<int,CClient*>::iterator ITER_MAP_CLIENT_OBJ;

class CLoginThread : public CThread
{
public:
	void OnThreadRun()
	{
		int nUserId = 1000;
		int nAppId = 100;
		int nDomainId = 1000;

		int nClientCount = 500;

		CClient* pClientObj = NULL;

		for (int i = 0; i < nClientCount; i ++)
		{
			pClientObj = doLogin(nUserId,nAppId, nDomainId);

			if (NULL != pClientObj)
			{
				g_mapClientConn.insert(std::make_pair(nUserId,pClientObj));
				nUserId++;
				nAppId++;
				nDomainId++;
			}

			//sleep(1);
		}
	}

	void DestoryObjs()
	{
		ITER_MAP_CLIENT_OBJ pIterBeg = g_mapClientConn.begin(),
				pIterEnd = g_mapClientConn.end();

		while(pIterBeg != pIterEnd)
		{
			if ( pIterBeg->second != NULL )
			{
				delete pIterBeg->second;
				pIterBeg->second = NULL;
			}
			pIterBeg++;
		}
	}
private:
};

class CmdThread : public CThread
{
public:
	void OnThreadRun()
	{
		while (true)
		{
			fprintf(stderr, "%s", PROMPT);	// print to error will not buffer the printed message

			if (fgets(m_buf, MAX_LINE_LEN - 1, stdin) == NULL)
			{
				fprintf(stderr, "fgets failed: %d\n", errno);
				continue;
			}

			m_buf[strlen(m_buf) - 1] = '\0';	// remove newline character

			split_cmd(m_buf);

			exec_cmd();
		}
	}
private:
	char	m_buf[MAX_LINE_LEN];
};



typedef struct {
    int        pid;
    int        status;
    int        channel[2];
    //ngx_spawn_proc_pt   proc;
    void               *data;
    char               *name;
    unsigned           respawn:1;
    unsigned           just_spawn:1;
    unsigned           detached:1;
    unsigned   		  exiting:1;
    unsigned   		  exited:1;
} tt_process_t;
tt_process_t    tt_processes[TT_MAX_PROCESSES];

int 	  g_tt_pid;



#if 0
void test_client_loop_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
  // receive msg from socket pair
	DEBUG("PID:%d,PPID:%d,channel_fd:%d,tt_terminate:%d,test_client_loop_callback",
			getpid(), getppid(),*(int*)(callback_data),tt_terminate);
	sleep(2);
    if (tt_terminate) {
        DEBUG("exiting");
        exit(1);
    }
}
#endif //0


static void tt_worker_process_exit()
{
	//tt_processes[tt_process_slot].exiting = 1;
	//tt_close_channel(tt_processes[tt_process_slot].channel);
	exit(0);
}


void tt_channel_handler(tt_event_t *pEV)
{
	if (NULL == pEV)
		return;

	 int    n;
	 tt_channel_t      ch;
	 int nChannelFd = pEV->fd;
    DEBUG("###Enter tt_channel_handler");
    for (;;)
     {
        n = tt_read_channel(pEV->fd, &ch, sizeof(tt_channel_t));
        DEBUG("Read %d Bytes from channel: %i", n, nChannelFd);
        if (n == TT_ERROR) {
            return;
           }

        if (n == TT_AGAIN) {
             return;
           }

        DEBUG("channel command: %d", ch.command);

    	  switch(ch.command) {
		      case TT_CMD_USER1:
					 DEBUG("GetPid=%d,PID=%d,Received user1 command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
					 tt_user1 = 1;
					 break;

			   case TT_CMD_TERMINATE:
				    DEBUG("GetPid=%d,PID=%d,Received terminate command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
				    tt_terminate = 1;
				    break;

			   case TT_CMD_QUIT:
				    DEBUG("GetPid=%d,PID=%d,Received quit command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
				    tt_quit = 1;
				    break;

			   case TT_CMD_OPEN_CHANNEL:
				    DEBUG("GetPid=%d,PID=%d,Received open channel command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
				    tt_processes[ch.slot].pid = ch.pid;
		          tt_processes[ch.slot].channel[0] = ch.fd;
				    break;

		      case TT_CMD_CLOSE_CHANNEL:
		            DEBUG("close channel s:%i pid:%d our:%d fd:%d",
		                           ch.slot, ch.pid, tt_processes[ch.slot].pid,
		                           tt_processes[ch.slot].channel[0]);

		            if (close(tt_processes[ch.slot].channel[0]) == -1) {
		                ERROR("close() channel failed");
		               }
		            tt_processes[ch.slot].channel[0] = -1;
		            break;

			   default:
				   break;
    	 }//End switch
    } //End for
	return;
}

/*
static void tt_channel_handler(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
    int          n;
    tt_channel_t      ch;
    int nChannelFd = *((int*)callback_data);
    DEBUG("###Enter tt_channel_handler");
    for (;;)
     {
        n = tt_read_channel(nChannelFd, &ch, sizeof(tt_channel_t));
        DEBUG("Read %d Bytes from channel: %i", n, nChannelFd);
        if (n == TT_ERROR) {
            return;
           }

        if (n == TT_AGAIN) {
             return;
           }

        DEBUG("channel command: %d", ch.command);

    	  switch(ch.command) {
		      case TT_CMD_USER1:
					 DEBUG("GetPid=%d,PID=%d,Received user1 command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
					 tt_user1 = 1;
					 break;

			   case TT_CMD_TERMINATE:
				    DEBUG("GetPid=%d,PID=%d,Received terminate command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
				    tt_terminate = 1;
				    break;

			   case TT_CMD_QUIT:
				    DEBUG("GetPid=%d,PID=%d,Received quit command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
				    tt_quit = 1;
				    break;

			   case TT_CMD_OPEN_CHANNEL:
				    DEBUG("GetPid=%d,PID=%d,Received open channel command from channel id=%d",getpid(),tt_processes[ch.slot].pid,nChannelFd);
				    tt_processes[ch.slot].pid = ch.pid;
		          tt_processes[ch.slot].channel[0] = ch.fd;
				    break;

		      case TT_CMD_CLOSE_CHANNEL:
		            DEBUG("close channel s:%i pid:%d our:%d fd:%d",
		                           ch.slot, ch.pid, tt_processes[ch.slot].pid,
		                           tt_processes[ch.slot].channel[0]);

		            if (close(tt_processes[ch.slot].channel[0]) == -1) {
		                ERROR("close() channel failed");
		               }
		            tt_processes[ch.slot].channel[0] = -1;
		            break;

			   default:
				   break;
    	 }//End switch
    } //End for
	return;
}
*/


static void tt_pass_open_channel(int nSockFd,tt_channel_t *ch)
{
#if 0
    for (int i = 0; i < tt_last_process; i++) {

        if (i == tt_process_slot
            || tt_processes[i].pid == -1
            || tt_processes[i].channel[0] == -1)
        {
            continue;
        }// End if
    }// End for
#endif //0

    DEBUG("tt_write_channel chid = %d",tt_processes[tt_process_slot].channel[0]);
    tt_write_channel(nSockFd,ch, sizeof(tt_channel_t));
}

static void tt_worker_process_init()
{
	sigset_t    set;
	int        n;

   sigemptyset(&set);

   if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        ERROR("sigprocmask() failed");
    }//End if

    for (n = 0; n < tt_last_process; n++) {

        if (tt_processes[n].pid == -1) {
            continue;
        }//End if

        if (n == tt_process_slot) {
            continue;
        }//End if

        if (tt_processes[n].channel[1] == -1) {
            continue;
        }//End if

        //close read channel, reserve write channel for master process
        if (close(tt_processes[n].channel[1]) == -1) {
            FATAL("close() channel failed"); // close read channel
        }//End if
    }// End for

    // close write channel for current worker process
    if (close(tt_processes[tt_process_slot].channel[0]) == -1) {
        FATAL("close() channel failed");
    }//End if
}

// work process start function
static void tt_worker_process_cycle(int nChannelFd)
{
	 int ret = netlib_init();
    if (ret == NETLIB_ERROR)
			return;

    tt_process = TT_PROCESS_WORKER;
	 tt_setproctitle("worker process");

	 CChannelFdSocket* pChannelFd = new CChannelFdSocket;
	 pChannelFd->Initialise(nChannelFd, SOCKET_READ|SOCKET_EXCEP,NULL,tt_channel_handler);
	 tt_add_channel_event(nChannelFd,SOCKET_READ | SOCKET_EXCEP);

	 tt_worker_process_init();
	 //netlib_register_timer(tt_channel_handler,(void*)&nChannelFd,2000);
#if 0
	 rb_timer_item channel_handler;
	 rbtimer_init(&channel_handler,tt_channel_handler,(void*)&nChannelFd,2000,0,0);
	 netlib_register_timer(&channel_handler);
#endif //0

	 //netlib_register_timer(tt_channel_handler,(void*)&nChannelFd,2000);
	 //netlib_add_loop(test_client_loop_callback, (void*)(&nChannelFd) );
	 //netlib_eventloop();

    for ( ;; ) {
		if (tt_terminate) {
			DEBUG("exiting");
			tt_worker_process_exit();
		}//End if (tt_terminate)

		if (tt_quit) {
			tt_quit = 0;
			ERROR("gracefully shutting down");
			tt_setproctitle("worker process is shutting down");

		   if (!tt_exiting) {
		   	tt_exiting = 1;
		    } //End if (!tt_exiting)
	   } //End if (tt_quit)

		if (tt_user1) {
			DEBUG("Process:%d received SIGUSER1",getpid());
			tt_user1 = 0;
		} //End if(tt_user1)

		CEventDispatch::Instance()->ProcessEventsAndTimers();
	}//End for
}

/*
int tt_work_process(int nChannelFd)
{
	int ret = netlib_init();
	if (ret == NETLIB_ERROR)
		return ret;


	netlib_add_loop(test_client_loop_callback, (void*)(&nChannelFd) );
}
*/

// master process worker
static void tt_process_get_status(void)
{
    int	status;
    char	*process;
    int  pid;
    int  err;
    int  i;
    int	one;

    one = 0;

    for ( ;; ) {
          pid = waitpid(-1, &status, WNOHANG);

          if (pid == 0) {
              return;
          }

          if (pid == -1) {
              err = errno;

              if (err == EINTR) {
                  continue;
              } //end if

              if (err == ECHILD && one) {
                  return;
              }//end if
          } //end if

          one = 1;
          process = "unknown process";
          for (i = 0; i < tt_last_process; i++) {
              if (tt_processes[i].pid == pid) {
                  tt_processes[i].status = status;
                  tt_processes[i].exited = 1;
                  tt_processes[i].exiting = 0;
                  process = tt_processes[i].name;
                  break;
              } // End if
          } //End for

          if (WTERMSIG(status)) {
        	  ERROR( "%s %d exited on signal %d",process, pid, WTERMSIG(status));
          }	else {				// End WTERMSIG
        	  ERROR("%s %d exited with code %d",process, pid, WEXITSTATUS(status));
          }// end else

          if (WEXITSTATUS(status) == 2 && tt_processes[i].respawn) {
                      FATAL("%s %d exited with fatal code %d "
                                    "and cannot be respawned",
                                    process, pid, WEXITSTATUS(status));
                      tt_processes[i].respawn = 0;
         }//End if
    }// End for
}


void tt_signal_handler(int signo)
{
	tt_signal_t    *sig;
	char           *action;
	int        err;

	action = "";

	for (sig = signals; sig->signo != 0; sig++) {
		if (sig->signo == signo) {
			break;
		}
	}

	switch (tt_process) {
	case TT_PROCESS_MASTER:
		switch (signo) {
		 case tt_signal_value(TT_SHUTDOWN_SIGNAL):
				 tt_quit = 1;
		 	 	 action = ", shutting down";
				 break;

		 case tt_signal_value(TT_TERMINATE_SIGNAL):
		 case SIGINT:
			 tt_terminate = 1;
			 action = ", exiting";
		    break;

		 case SIGALRM:
		 	tt_sigalrm = 1;
		   break;

		 case SIGIO:
			 tt_sigio = 1;
			 break;

		 case SIGCHLD:
			 tt_reap = 1;
			 break;

		 case SIGUSR1:
			 tt_user1 = 1;
			 break;
		}//switch (signo)
		break;


	case TT_PROCESS_WORKER:
		switch(signo){
		case tt_signal_value(TT_SHUTDOWN_SIGNAL):
			tt_quit = 1;
			action = ",shutting down";
			break;


		case tt_signal_value(TT_TERMINATE_SIGNAL):
		case SIGINT:
			tt_terminate = 1;
			action = ", exiting";
			break;

		case SIGUSR1:
			tt_user1 = 1;
			break;
		} //switch(signo)
		break;
	} //switch (tt_process)

	DEBUG("signal %d (%s) received%s", signo, sig->signame, action);
	if (signo == SIGCHLD) {
		tt_process_get_status();
	}

	return;
}

static void tt_master_process_exit()
{
	DEBUG("Exiting master process!");
	exit(0);
}

static void tt_signal_worker_processes(int signo)
{
    int      		i;
    int      		err;
    tt_channel_t  ch;
    ssize_t       n;

    memset(&ch, 0, sizeof(tt_channel_t));

    switch (signo) {
		case tt_signal_value(TT_SHUTDOWN_SIGNAL):
			ch.command = TT_CMD_QUIT;
			break;

		case tt_signal_value(TT_TERMINATE_SIGNAL):
			ch.command = TT_CMD_TERMINATE;
			break;

		case SIGUSR1:
			ch.command = TT_CMD_USER1;
			break;

		default:
			ch.command = 0;
    } // end switch

    ch.fd = -1;
     // Start work cycle
    for (i = 0; i < tt_last_process; i++) {
        DEBUG( "child: %d p:%d e:%d t:%d d:%d r:%d j:%d",
                       i,
                       tt_processes[i].pid,
                       tt_processes[i].exiting,
                       tt_processes[i].exited,
                       tt_processes[i].detached,
                       tt_processes[i].respawn,
                       tt_processes[i].just_spawn);

        if (tt_processes[i].detached || tt_processes[i].pid == -1) {
            continue;
        	}

#if 0
      if (tt_processes[i].exiting
            && signo == tt_signal_value(TT_SHUTDOWN_SIGNAL))
        {
            continue;
        }
#endif //0

      if (ch.command) {
    	    ch.slot = i;
          if (tt_write_channel(tt_processes[i].channel[0],&ch,sizeof(tt_channel_t))  == TT_OK ) {
        	     if (ch.command == TT_CMD_QUIT || ch.command == TT_CMD_TERMINATE) {
        	    	 	 tt_processes[i].exiting = 1;
        	          } //Endif
        	            continue;
          	  	 }//Endif
            }  //Endif

         // Other signal send to child process
       DEBUG("kill (%d, %d)", tt_processes[i].pid, signo);
       if (kill(tt_processes[i].pid, signo) == -1) {
            FATAL("kill(%d, %d) failed,%s", tt_processes[i].pid, signo,errno);

            if (errno == ESRCH) {
                tt_processes[i].exited = 1;
                tt_processes[i].exiting = 0;
                tt_reap = 1;
            	} // End if (errno == ESRCH)
            continue;
        } // End if
	}  // End for
}


int tt_spawn_process(void *data,char *name/*process name*/,int respawn)
{
	int s = 0;
	u_long     on;
	int 		  tt_channel_fd;
	int 		  spawn_pid;
	int        pid = 0;


    if (respawn >= 0) {
        s = respawn;
    } else {
    	for (s = 0; s < tt_last_process; s++)
    	{
    		if (tt_processes[s].pid == -1)
    			break;
    	}
    	if (s == TT_MAX_PROCESSES)
    	{
    		DEBUG("Reached the max processes limitiation");
    		return -1;
    	}
    }

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, tt_processes[s].channel) == -1)
	{
			ERROR("socketpair() failed while spawning \"%s\"", name);
			return -1;
	}
   DEBUG("PID:%d,channel %d:%d",getpid(),tt_processes[s].channel[0],tt_processes[s].channel[1]);
	 if (respawn != TT_PROCESS_DETACHED) {

		if (tt_nonblocking(tt_processes[s].channel[0]) == -1) {
			ERROR(" failed while spawning \"%s\"",name);
			tt_close_channel(tt_processes[s].channel);
			return -1;
		}

		if (tt_nonblocking(tt_processes[s].channel[1]) == -1) {
			ERROR(" failed while spawning \"%s\"",name);
			tt_close_channel(tt_processes[s].channel);
			return -1;
		}

		on = 1;
		if (ioctl(tt_processes[s].channel[0], FIOASYNC, &on) == -1) {
			ERROR("ioctl(FIOASYNC) failed while spawning \"%s\"", name);
			tt_close_channel(tt_processes[s].channel);
			return -1;
		}

		if (fcntl(tt_processes[s].channel[0], F_SETOWN, g_tt_pid) == -1) {
			ERROR("fcntl(F_SETOWN) failed while spawning \"%s\"", name);
			tt_close_channel(tt_processes[s].channel);
			return -1;
		}

	   tt_channel_fd = tt_processes[s].channel[1]; //Read channel
	 } else {
		 tt_processes[s].channel[0] = -1;
		 tt_processes[s].channel[1] = -1;
	 }

	tt_process_slot = s;
	if (tt_last_process == s)
		tt_last_process++;

	 pid = fork();

	 switch (pid) {
	    case -1:
	        ERROR("fork() failed while spawning \"%s\"", name);
	        tt_close_channel(tt_processes[s].channel);
	        return -1;

	    case 0:
	    	//spawn_pid = getpid();
	    	//DEBUG("PPID:%d,PID:%d,Start spawm child",getppid(),getpid());
	    	//DEBUG("The Returun value is %d In child process!!My PID is %d : i=%d",pid,getpid(),*(int*)data);
	        //proc(cycle, data);
	    	//tt_work_process(tt_channel_fd); // Start child process
	        tt_worker_process_cycle(tt_channel_fd);
	    	  exit(1);
	      break;

	    default:
	    	//DEBUG("The Returun value is %d In father process!!My PID is %d : i=%d",pid,getpid(),*(int*)data);
	        break;
	    }

	 DEBUG("start %s :%d ",name,pid);
	 tt_processes[s].pid = pid;
	 tt_processes[s].exited = 0;
	 tt_processes[s].exiting = 0;
	 tt_processes[s].name = name;

	switch (respawn) {
		case TT_PROCESS_NORESPAWN:
			tt_processes[s].respawn = 0;
			tt_processes[s].just_spawn = 0;
			tt_processes[s].detached = 0;
			break;

		case TT_PROCESS_JUST_SPAWN:
			tt_processes[s].respawn = 0;
			tt_processes[s].just_spawn = 1;
			tt_processes[s].detached = 0;
			break;

		case TT_PROCESS_RESPAWN:
			tt_processes[s].respawn = 1;
			tt_processes[s].just_spawn = 0;
			tt_processes[s].detached = 0;
			break;

		case TT_PROCESS_JUST_RESPAWN:
			tt_processes[s].respawn = 1;
			tt_processes[s].just_spawn = 1;
			tt_processes[s].detached = 0;
			break;

		case TT_PROCESS_DETACHED:
			tt_processes[s].respawn = 0;
			tt_processes[s].just_spawn = 0;
			tt_processes[s].detached = 1;
			break;
	}

	return tt_process_slot;
}

static int  tt_save_argv(int argc, char *const *argv)
{
#if 1

    tt_os_argv = (char **) argv;
    tt_argc = argc;
    tt_argv = (char **) argv;

#else
    size_t     len;
    int   		i;

    tt_os_argv = (char **) argv;
    tt_argc = argc;

    tt_argv = (char**)malloc((argc + 1) * sizeof(char *));
    if (tt_argv == NULL) {
        return -1;
    }

    for (i = 0; i < argc; i++) {
        len = strlen(argv[i]) + 1;

        tt_argv[i] = (char*)malloc(len);
        if (tt_argv[i] == NULL) {
            return -1;
        }

        (void)tt_cpystrn((u_char *) tt_argv[i], (u_char *) argv[i], len);
    }
    //tt_argv[i] = NULL;
#endif
    //tt_os_environ = environ;
    return 0;
}

static void  tt_start_worker_processes(int nProcesses, int nStartType)
{
	// Start fork process
	int nProcessNum = 1;
	//int nProcessSlot = 0;
	tt_channel_t  ch;
	memset(&ch,0,sizeof(tt_channel_t));
   ch.command = TT_CMD_OPEN_CHANNEL;

	for(int i= 0; i < nProcesses; i++) {
		tt_spawn_process(&i,"worker porcess",nStartType);
		ch.fd = tt_processes[tt_process_slot].channel[0];
		ch.pid = tt_processes[tt_process_slot].pid;
		ch.slot = tt_process_slot;

		tt_pass_open_channel(ch.fd,&ch);
	}//End for
}


// restart  child process
static int tt_reap_children()
{
    int         i, n;
    int        live;
    tt_channel_t     ch;

    memset(&ch,0,sizeof(tt_channel_t));

    ch.command = TT_CMD_CLOSE_CHANNEL;
    ch.fd = -1;

    live = 0;
    for (i = 0; i < tt_last_process; i++) {
        DEBUG("child: %d p:%d e:%d t:%d d:%d r:%d j:%d",
                       i,
                       tt_processes[i].pid,
                       tt_processes[i].exiting,
                       tt_processes[i].exited,
                       tt_processes[i].detached,
                       tt_processes[i].respawn,
                       tt_processes[i].just_spawn);

        if (tt_processes[i].pid == -1) {
            continue;
          }

        if (tt_processes[i].exited) {
            if (!tt_processes[i].detached) {
                tt_close_channel(tt_processes[i].channel);

                tt_processes[i].channel[0] = -1;
                tt_processes[i].channel[1] = -1;

                ch.pid = tt_processes[i].pid;
                ch.slot = i;

                for (n = 0; n < tt_last_process; n++) {
                    if (tt_processes[n].exited
                        || tt_processes[n].pid == -1
                        || tt_processes[n].channel[0] == -1)
                          {
                        continue;
                    }//End if

                    DEBUG("pass close channel s:%i pid:%d to:%d",
                                   ch.slot, ch.pid, tt_processes[n].pid);

                    /* TODO: NGX_AGAIN */
                    tt_write_channel(tt_processes[n].channel[0],
                                      &ch, sizeof(tt_channel_t));
                }//End for
            }//End if (!tt_processes[i].detached)

            if (tt_processes[i].respawn
                && !tt_processes[i].exiting
                && !tt_terminate
                && !tt_quit)
                {
            	DEBUG("Start spawn process,which slot %d.",i);
                if (tt_spawn_process(tt_processes[i].data,tt_processes[i].name, i)==-1)
                	 {
                    ERROR("could not respawn %s",tt_processes[i].name);
                    continue;
                	 }

                ch.command = TT_CMD_OPEN_CHANNEL;
                ch.pid = tt_processes[tt_process_slot].pid;
                ch.slot = tt_process_slot;
                ch.fd = tt_processes[tt_process_slot].channel[0];

                tt_pass_open_channel(ch.fd,&ch);
                live = 1;

                continue;
             }//End if


            if (i == tt_last_process - 1) {
                tt_last_process--;
            } else {
                DEBUG("Set process:%d to -1", tt_processes[i].pid);
                tt_processes[i].pid = -1;
            }//end else
        } else if (tt_processes[i].exiting || !tt_processes[i].detached) {
            live = 1;
        }//End if
    }//End for

    return live;
}

int tt_master_process_cycle(int nWorkerProcess)
{
	sigset_t           set;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGALRM);
	sigaddset(&set, SIGIO);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, tt_signal_value(TT_SHUTDOWN_SIGNAL));
	sigaddset(&set, tt_signal_value(TT_TERMINATE_SIGNAL));

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        FATAL("sigprocmask() failed");
    }
    sigemptyset(&set);

    tt_process = TT_PROCESS_MASTER;
    init_setproctitle();
    tt_setproctitle("master process");
    tt_start_worker_processes(nWorkerProcess,TT_PROCESS_RESPAWN);
    int  live = 1;
	 int ret = netlib_init();
	 if (ret == NETLIB_ERROR)
		 return ret;

    for(;;)
	 {
        DEBUG("sigsuspend");
		  sigsuspend(&set);

	      if (tt_reap) {
				  tt_reap = 0;
				  DEBUG("reap children");
			     live = tt_reap_children();
	        }//End if(tt_reap)

	      if (!live && (tt_terminate || tt_quit)) {
				DEBUG("exiting");
				tt_master_process_exit();
			}//End if

		  if (tt_terminate) { //tt_terminate assigned in function tt_signal_handler()
			  tt_signal_worker_processes(tt_signal_value(TT_TERMINATE_SIGNAL));
			  continue;
			} // End if (tt_terminate)

		  if (tt_quit) {
			  tt_signal_worker_processes(tt_signal_value(TT_SHUTDOWN_SIGNAL));
			  continue;
		  } // End if

		  if (tt_user1) {
			  tt_signal_worker_processes(SIGUSR1);
			  tt_user1 = 0;
			  continue;
		  } //End if
    } // End for
}

CmdThread g_cmd_thread;


int main(int argc, char* argv[])
{
//    play("message.wav");
	//g_cmd_thread.StartThread();
	CConfigFileReader config_file("test_client.conf");
	int nWorkerProcess = atoi(config_file.GetConfigName("ProcessNum"));
	tt_daemon();
	g_tt_pid = getpid();
	tt_init_signals();		// initialise signals


   if (tt_save_argv(argc, argv) != 0) {
        return 1;
    }

	tt_last_process = 0;
	for (int i = 0; i < TT_MAX_PROCESSES; i++)
	{
		tt_processes[i].pid = -1;
		tt_processes[i].status = 0;
	}

	tt_master_process_cycle(nWorkerProcess);

#if 0
	sigset_t           set;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	sigaddset(&set, SIGALRM);
	sigaddset(&set, SIGIO);
	sigaddset(&set, SIGINT);
	sigaddset(&set, tt_signal_value(TT_SHUTDOWN_SIGNAL));
	sigaddset(&set, tt_signal_value(TT_TERMINATE_SIGNAL));

    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        FATAL("sigprocmask() failed");
    }

    sigemptyset(&set);

	//CLoginThread login_thread;
	//login_thread.StartThread();
	//signal(SIGPIPE, SIG_IGN);

	// Start fork process
	int nProcessNum = 1;
	int nProcessSlot = 0;
	for(int i= 0; i < TT_MAX_PROCESSES; i++) {
		nProcessSlot = tt_spawn_process(&i,"worker porcess");
		ch.fd = tt_processes[nProcessSlot].channel[0];
		ch.pid = tt_processes[nProcessSlot].pid;
		ch.slot = nProcessSlot;
	}

#if 0
	int ret = netlib_init();
	if (ret == NETLIB_ERROR)
		return ret;

	netlib_add_loop(test_client_loop_callback, NULL);
    
	netlib_eventloop();
#endif //0

	int ret = netlib_init();
	if (ret == NETLIB_ERROR)
		return ret;

	for(;;)
	{
		DEBUG("sigsuspend");
      sigsuspend(&set);

      if (tt_terminate) { //tt_terminate assigned in function tt_signal_handler()
    	  tt_signal_worker_processes(tt_signal_value(TT_TERMINATE_SIGNAL));
    	  continue;
        } // End if

      if (tt_quit) {
    	  tt_signal_worker_processes(tt_signal_value(TT_SHUTDOWN_SIGNAL));
      } // End if
	} // End for
#endif //0
	//netlib_eventloop();

	return 0;
}

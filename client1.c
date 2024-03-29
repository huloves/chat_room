#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <linux/socket.h>
#include <string.h>
#include <pthread.h>

#include "chat.h"

struct node{  //请求结构体
    //int cmd;
    //char time[30];
    //char from_name[20];
    //char to_name[20];
    MSG request;
    struct node *next;
};

int fd;  //用来通信的文件描述符
char myname[20]; //自己的名字
int chat_clear = 7;  //用来聊天清屏

struct node *head_friend = NULL;
struct node *head_group = NULL;

pthread_mutex_t mutex;
pthread_cond_t cond;

void my_err(char *err, int line){  //错误处理函数
    printf("line: %d", line);
    perror(err);
    exit(1);
}

char *my_time(){
    char *str;
    time_t now;
    time(&now);

    str = ctime(&now);
    str[strlen(str)-1] = '\0';

    return str;
}

//尾插法
void charu_list(struct node **phead, MSG request){
    struct node *current, *new_node;
    new_node = (struct node*)malloc(sizeof(struct node));
    //init
    new_node->request = request;
    new_node->next = NULL;

    if(*phead == NULL){
        *phead = new_node;
        return;
    }

    //找到尾节点
    current = *phead;
    while(current->next != NULL){
        current = current->next;
    }

    current->next = new_node;
    return;
}

//从头删除一个节点
void del_head(struct node *phead){
    struct node *temp;

    temp = phead;
    phead = temp->next;
    
    free(temp);
}

//遍历并显示链表内容
void bianli_list(struct node *phead){
    struct node *current;
    int i = 0;
    current = phead;
    while(current != NULL){
        printf("%d [%s]想加你为好友\n", i, current->request.from_name);
        current = current->next;
        i++;
    }
}

int login_face(int conn_fd);            //登录界面
int user_face(int conn_fd);             //conn_fd  //用户界面
int reg(int conn_fd);                   //注册功能
int login(int conn_fd);                 //登录功能
int forget_password(int conn_fd);       //忘记密码
int my_friend(int conn_fd);             //我的好友功能
void *server_message(void *arg);        //线程处理函数
int choice_group(int conn_fd);          //选择群聊
int deal_request(int conn_fd);          //处理申请
int create_group(int conn_fd);          //创建群聊
int send_file(int conn_fd);             //发送文件
int exit_face(int conn_fd);             //注销
int all_friends(int conn_fd);           //查看所有好友
int online_friends(int conn_fd);        //查看在线好友
int private_chat(int conn_fd);          //私聊功能
int add_friend(int conn_fd);            //添加好友
int delete_friend(int conn_fd);         //删除好友
int friend_request(int conn_fd);        //好友申请处理
int group_request(int conn_fd);         //入群申请处理
int view_group(int conn_fd);            //查看我的群
int add_group(int conn_fd);             //申请加群
int public_chat(int conn_fd);           //群聊
int quit_group(int conn_fd);            //退出群聊
int recv_file(MSG *message);            //接收文件
int manage_group(int conn_fd);          //管理群
int view_group_member(int conn_fd);     //查看群成员
int break_group(int conn_fd);           //解散群
int set_manager(int conn_fd);           //设置管理员

int main(int argc, char * argv[])
{
    if(argc < 3)
    {
        printf("eg: ./a.out [port] [ip]\n");
        exit(1);
    }

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    
    int ret;
    pthread_t pthread1;
    int port = atoi(argv[1]); //字符串变量转换成整形变量
    //创建套接字
    fd = socket(AF_INET, SOCK_STREAM, 0);

    //连接服务器
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    
    serv.sin_family = AF_INET;
    serv.sin_port = htons(port);
    //serv.sin_addr.s_addr = htonl()
    //inet_pton(AF_INET, "127.0.0.1", &serv.sin_addr.s_addr);
    inet_pton(AF_INET, argv[2], &serv.sin_addr.s_addr);
    ret = connect(fd, (struct sockaddr*)&serv, sizeof(serv) );
    if(ret == -1){
        perror("connect");
        printf("line: %d\n", __LINE__-3);
        exit(1);
    }

    //通信
    //显示登录界面
    while(1){
        system("clear");
        char choice[10]; //登录界面的选择
        memset(choice, 0 ,sizeof(choice));
        printf("----------------------\n");
        printf("☆       1.注册      ☆ \n");
        printf("----------------------\n");
        printf("☆       2.登录      ☆ \n");
        printf("----------------------\n");
        printf("☆       3.忘记密码  ☆ \n");
        printf("----------------------\n");
        printf("☆       0.退出      ☆ \n");
        printf("----------------------\n");

        while(1){
            printf("请输入你的选择：");
            scanf("%s", choice);
            while(getchar() != '\n')
                continue;
            
            if(strcmp(choice, "1")==0 || strcmp(choice, "2")==0 || strcmp(choice, "3")==0 || strcmp(choice, "0")==0){
                break;
            }
            else{
                printf("输入错误，请按照提示输入\n");
            }
        }

        if(choice[0] == '0'){
            close(fd);
            exit(1);
        }
        else if(choice[0] == '3'){  //退出
            forget_password(fd);
            getchar();
            continue;
        }
        else if(choice[0] == '1'){  //选择注册
            reg(fd);
            continue;
        }
        else if(choice[0] == '2'){  //选择登录
            ret = login(fd);
            if(ret == 1){  //登陆成功
                //pthread_cond_signal(&cond);
                break;
            }
            else{          //登录失败
                continue;
            }
        }
    }

    //sleep(1);

    //登陆成功，创建用户线程，接受服务器的消息
    pthread_create(&pthread1, NULL, server_message, NULL);
    pthread_detach(pthread1);
    user_face(fd);

    return 0;
}

//线程处理函数
void *server_message(void *arg){
    //pthread_cond_wait(&cond, &mutex);
    //printf("A\n");
    
    while(1){
        //printf("a");
        MSG message;
        //char *recv_message;
        //recv_message = (char*)malloc(sizeof(message)); //每次接受信息的为一个结构体大小
        memset(&message, 0, sizeof(message));
    
        int len = recv(fd, &message, sizeof(message), 0);
        if(len == -1){
            my_err("recv", __LINE__-2);
        }
        else if(len == 0){
            printf("服务器断开连接\n");
            pthread_exit(0);
        }
        //memcpy(&message, recv_message, sizeof(message));
        //printf("Q\n");
        //printf("----message.cmd: %d\n", message.cmd);
        switch(message.cmd){
        case 111:
            printf("你还没有朋友\n");
            break;
        case 11:
            printf("好友: %s\n", message.friend_name);
            break;
        case 121:
            //printf("目前没有好友在线\n");
            printf("目前你还没有好友\n");
            break;
        case 122:
            printf("目前没有好友在线\n");
            break;
        case 12:
            printf("在线好友: %s\n", message.friend_name);
            break;
        case 13:
            //printf("\033[0m\033[s"); //保存当前光标
            //printf("\033[0m\033[4;1H");  //光标跳到第四行，第一格
            //printf("\033[0m\033[%d;1H", chat_clear);
            //chat_clear += 2;
            printf("%s %s说：\n", message.time, message.from_name);
            printf("%s\n", message.news);
            //chat_clear++;
            //pthread_mutex_lock(&mutex);
            //if(chat_clear == 6){
            //    for(int i=0; i<6; i++){
            //        printf("\033[0m\033[K");
            //    }
            //    chat_clear = 0;
            //}
            //pthread_mutex_unlock(&mutex);
            //printf("\033[0m\033[u");
            //printf("\033[0m\033[u");  //恢复光标
            //fflush(stdout);   //清空I/O缓存中的标准输出缓存使光标恢复到原点
            break;
        case 130:
            printf("%s 还不是你的好友\n", message.to_name);
            break;
        case 131:
            printf("[!!!] %s\n", message.news);
            break;
        //case -13:
          //  printf("你的好友不在线\n");
          //  break;
        case 14:                              //-----------------------------------
            printf("\n你有一个好友申请，请到消息处理\n");
            //printf("A\n");
            pthread_mutex_lock(&mutex);
            charu_list(&head_friend, message);
            pthread_mutex_unlock(&mutex);
            //printf("N\n");
            //bianli_list(head_friend);
            break;
        case 140:
            printf("你们已经成为好友\n");
            break;
        case 141:
            printf("[%s] 该用户不存在\n", message.to_name);
            break;
        case 149:
            printf("不能加自己为好友\n");
            break;
        case 15:
            printf("删除成功\n");
            break;
        case 150:
            printf("你没有该好友\n");
            break;
        case 211:
            printf("你还没有加入任何群聊\n");
            break;
        case 21:
            printf("群名: %s\n", message.group.group_name);
            break;
        case 22:
            printf("%s %s说：\n", message.time, message.from_name);
            printf("%s\n", message.news);
            break;
        case 220:
            printf("你还没有该群聊\n");
            break;
        case 23:
            printf("\n你有一个入群申请，请到处理申请\n");
            pthread_mutex_lock(&mutex);
            charu_list(&head_group, message);
            pthread_mutex_unlock(&mutex);
            break;
        case 230:
            printf("[%s] 已经加入该群\n", message.to_group_name);
            break;
        case 231:
            printf("[%s] 该群不存在\n", message.to_group_name);
            break;
        case 24:
            printf("%s 退出了[%s] \n", message.from_name, message.to_group_name);
            break;
        case 240:
            printf("[%s] 该群不存在\n", message.to_group_name);
            break;
        case 241:
            printf("[%s] 你是该群的群主，不能退出，可选择解散群\n", message.to_group_name);
            break;
        case 242:
            printf("[%s] 你未加入过该群\n", message.to_group_name);
            break;
        case 243:
            printf("[%s] 已退出该群\n", message.to_group_name);
            break;
        case 251:
            //printf("%s\n", message.to_name);
            switch(message.friend_num){
            case 1:
                printf("%s [群主]\n", message.to_name);
                break;
            case 2:
                printf("%s [管理员]\n", message.to_name);
                break;
            case 3:
                printf("%s [群成员]\n", message.to_name);
                break;
            }
            break;
        case 2510:
            printf("[%s] 该群不存在\n", message.to_group_name);
            break;
        case 2511:
            printf("[%s] 未加入该群\n", message.to_group_name);
            break;
        case 252:
            printf("[%s] 解散完成\n", message.to_group_name);
            break;
        case 2520:
            printf("[%s] 该群不存在\n", message.to_group_name);
            break;
        case 2521:
            printf("[%s] 你不是该群的群主，不能解散该群\n", message.to_group_name);
            break;
        case 253:
            printf("[%s] 成功将%s设置为管理员\n", message.to_group_name, message.to_name);
            break;
        case 2530:
            printf("[%s] 你不是该群的群主\n", message.to_group_name);
            break;
        case 2531:
            printf("[%s] %s不是该群的群成员\n", message.to_group_name, message.to_name);
            break;
        case 311:
            printf("\n%s 已成为你的好友\n", message.to_name);
            break;
        case 312:
            printf("\n%s 拒绝了你的好友申请\n", message.to_name);
            break;
        case 321:
            printf("\n你已加入该群 [%s]\n", message.to_group_name);
            break;
        case 322:
            printf("\n [%s] 的群主拒绝了你的申请\n", message.to_group_name);
            break;
        case 50:
            printf("该群已存在\n");
            break;
        case 5:
            printf("[%s] 群创建成功\n", message.group.group_name);
            break;
        case 6:
            printf("%s 给你发送了一个文件\n", message.from_name);
            recv_file(&message);
            break;
        case 60:
            printf("该用户不存在\n");
            break;
        case 61:
            printf("该人不是你的好友\n");
            break;
        }
    }
}

int recv_file(MSG *message){
    int file_fd;
    char buf[1024];
    memset(buf, 0, sizeof(buf));

    printf("filename:%s\n", message->file_name);
    sprintf(buf, "%s", message->file_name);
    //sprintf(buf, "/home/huloves/123");
    printf("buf:%s\n", buf);
    //creat(buf, 0777);
    file_fd = open(buf, O_RDWR | O_CREAT, 0777);
    if(file_fd == -1){
        my_err("open", __LINE__-2);
    }
    printf("A\n");
    char *ptr = (char*)message;
    int sum = 0;
    int recv_size;
    while((recv_size = recv(fd, ptr+sum, sizeof(MSG)-sum, 0)) > 0){
        //printf("B\n");
        sum += recv_size;
        if(sum == sizeof(MSG)) {
            sum = 0;
            if(ntohs(message->once_len) < 1024){
                write(file_fd, message->news, ntohs(message->once_len));
                printf("接收完成...\n");
                break;
            }
            printf("正在接收文件......size = %d\n", ntohs(message->once_len));
            write(file_fd, message->news, ntohs(message->once_len));
        }
        else {
            continue;
        }
    }

    close(file_fd);
}

int user_face(int conn_fd) {
    char choice[2];
    while(1){
        system("clear");
        printf("-------------------------\n");
        printf("☆       1.我的好友     ☆ \n");
        printf("-------------------------\n");
        printf("        2.选择群聊       \n");
        printf("-------------------------\n");
        printf("☆       3.处理申请     ☆ \n");
        printf("-------------------------\n");
        printf("        4.聊天记录       \n");
        printf("-------------------------\n");
        printf("☆       5.创建群聊     ☆ \n");
        printf("-------------------------\n");
        printf("        6.发送文件       \n");
        printf("-------------------------\n");
        printf("☆       0.注销退出     ☆ \n");
        printf("-------------------------\n");
        //printf("    请输入你的选择：");

        //scanf("%s", ch);
        //while(getchar() != '\n');
    
        while(1){
            printf("   请输入你的选择：");
            scanf("%s", choice);
            while(getchar() != '\n')
                continue;

            if(strcmp(choice, "1")==0||strcmp(choice, "2")==0||strcmp(choice, "3")==0||
                strcmp(choice, "4")==0||strcmp(choice, "5")==0||strcmp(choice, "6")==0||
                 strcmp(choice, "0")==0){
                break;
            }
            else{
                printf("输入错误，请按照提示输入\n");
            }
        }

        switch(choice[0]){
        case '1':
            my_friend(conn_fd);
            break;
        case '2':
            choice_group(conn_fd);
            break;
        case '3':
            deal_request(conn_fd);
            break;
        case '5':
            create_group(conn_fd);
            break;
        case '6':
            send_file(conn_fd);
            break;
        case '0':
            //exit_face(conn_fd);
            close(conn_fd);
            exit(1);
            break;
        }
    }

    return 0;
}

int reg(int conn_fd) {
    MSG message;
    //message.type = 'r';
    message.cmd = 1;

    system("clear");

    char buf[256];
    printf("*姓名：");
    scanf("%s", buf);
    while(getchar() != '\n');
    strcpy(message.user_infor.name, buf);

    printf("昵称：");
    scanf("%s", buf);
    while(getchar() != '\n');
    strcpy(message.user_infor.nickname, buf);

    printf("性别：");
    scanf("%s", buf);
    while(getchar() != '\n');
    strcpy(message.user_infor.sex, buf);

    printf("密码：");
    scanf("%s", buf);
    while(getchar() != '\n');
    strcpy(message.user_infor.password, buf);

    printf("电话号码：");
    scanf("%s", buf);
    while(getchar() != '\n');
    strcpy(message.user_infor.telephone, buf);

    send(conn_fd, &message, sizeof(message), 0);
    recv(conn_fd, &message, sizeof(message), 0);

    //printf("cmd = %d\n", message.cmd);
    //注册成功返回 1001
    if(message.cmd == 1001) {
        printf("注册成功\n");
        //printf("正在返回登录界面\n");
        printf("按回车键返回菜单\n");
        getchar();
        //sleep(1);
        return 1;
    }
    //注册失败返回 -1
    else if(message.cmd == -1) {
        printf("该用户已被注册\n");
        //printf("正在返回登录界面\n");
        printf("按回车键返回菜单\n");
        getchar();
        return -1;
    }
}

int login(int conn_fd){
    MSG message;
    memset(&message, 0, sizeof(message));
    message.cmd = 2;

    char buf[256];
    printf("用户名：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;

    strcpy(message.user_infor.name, buf);
    strcpy(myname, buf);

    printf("密  码：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;    
    strcpy(message.user_infor.password, buf);
    
    send(conn_fd, &message, sizeof(message), 0);
    recv(conn_fd, &message, sizeof(message), 0);

    //登录成功返回1002
    //printf("---cmd = %d\n", message.cmd);
    if(message.cmd == 1002) {
        printf("登陆成功\n");
        printf("按回车键继续\n");
        getchar();
        //pthread_cond_signal(&cond);
        //printf("M\n");
        return 1;     
    }
    //登录失败返回-2
    else if(message.cmd == -2) {
        printf("登录失败\n");
        printf("按回车键返回菜单\n");
        getchar();
        return -1;
    }
}

int forget_password(int conn_fd){
    MSG message;
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));

    message.cmd = 0;
    
    char buf[200];
    memset(buf, 0, sizeof(buf));
    printf("姓名：");
    scanf("%s", buf);
    strcpy(message.user_infor.name, buf);
    printf("请输入预留的电话号码：");
    scanf("%s", buf);
    strcpy(message.user_infor.telephone, buf);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }
    recv(conn_fd, &message, sizeof(message), 0);

    if(message.cmd == 01){
        printf("该账户未注册\n");
    }
    else if(message.cmd == 02){
        printf("电话号码错误\n");
    }
    else if(message.cmd == 03){
        printf("账号密码：%s\n", message.user_infor.password);
    }

    printf("按回车键返回菜单\n");
    getchar();
}

int my_friend(int conn_fd){
    char choice[10];
    
    while(1){
        memset(choice, 0, sizeof(choice));
        system("clear");
        printf("=========== %s ===========\n", myname);
        printf("-------------------------------\n");
        printf("☆      1.查看所有好友        ☆ \n");
        printf("-------------------------------\n");
        printf("☆      2.查看在线好友        ☆ \n");
        printf("-------------------------------\n");
        printf("☆      3.选择私聊            ☆ \n");
        printf("-------------------------------\n");
        printf("☆      4.添加好友            ☆ \n");
        printf("-------------------------------\n");
        printf("☆      5.删除好友            ☆ \n");
        printf("-------------------------------\n");
        printf("☆      0.返回上级菜单        ☆ \n");
        printf("-------------------------------\n");

        while(1){
            printf("请输入你的选择：");
            scanf("%s", choice);
            while(getchar() != '\n')
                continue;

            if(strcmp(choice, "1")==0||strcmp(choice, "2")==0||strcmp(choice, "3")==0||
               strcmp(choice, "4")==0||strcmp(choice, "5")==0||strcmp(choice, "0")==0){
                break;
            }
            else{
                printf("错误输入，请按照提示输入\n");
            }
        }

        switch(choice[0]){
        case '0':
            return 0;
        case '1':
            all_friends(conn_fd);
            break;
        case '2':
            online_friends(conn_fd);
            break;
        case '3':
            private_chat(conn_fd);
            break;
        case '4':
            add_friend(conn_fd);
            break;
        case '5':
            delete_friend(conn_fd);
            break;
        }
    }
}

int all_friends(int conn_fd){
    MSG message;
    int len = sizeof(message);
    memset(&message, 0, len);
    //message.type = 'a';
    message.cmd = 11;
    
    strcpy(message.user_infor.name, myname);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }
    /*recv(conn_fd, &message, sizeof(message), 0);
    if(message.friend_num == 0){
        printf("你还没有朋友\n");
    }
    else{
        for(int i=0; i<message.friend_num; i++){
            recv(conn_fd, &message, sizeof(message), 0);
            printf("haoyou: %s\n", message.friend_name);
        }
    }*/
    sleep(1);
    printf("按任意键返回菜单\n");
    getchar();
}

int online_friends(int conn_fd){
    MSG message;
    int len = sizeof(message);
    memset(&message, 0, len);
    
    //message.type = 'o';
    message.cmd = 12;
    strcpy(message.user_infor.name, myname);
    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    sleep(1);
    
    printf("按任意键返回菜单\n");
    getchar();
}

int private_chat(int conn_fd){
    MSG message;
    char buf[1024];
    char now_time[30];
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));
    
    message.cmd = 13;
    //memcpy(message.time, my_time(), 30);
    strcpy(message.from_name, myname);
    printf("请输入你好友的名字：");
    scanf("%s", buf);
    strcpy(message.to_name, buf);

    system("clear");

    printf("-----正在与 %s 聊天-----\n", message.to_name);
    printf("   --- quit 退出 ---\n");
    printf("=============================\n");

    while(1){
        memset(now_time, 0, sizeof(now_time));
        memset(buf, 0, sizeof(buf));
        strcpy(now_time, my_time());
        //printf("\033[0m\033[16;0H");
        //printf("%s %s :\n", now_time, message.from_name);
        scanf("%s", buf);
        while(getchar()!='\n')
            continue;

        if(strcmp(buf, "quit") == 0){
            break;
        }

        strcpy(message.time, now_time);
        strcpy(message.news, buf);
        send(conn_fd, &message, sizeof(message), 0);
    }
    //chat_clear = 7;

    printf("按回车键返回菜单\n");
    getchar();
}

int add_friend(int conn_fd){
    MSG message;
    char buf[1024];
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    memset(&buf, 0, sizeof(buf));
    
    message.cmd = 14;
    strcpy(message.from_name, myname);

    printf("请输入添加的好友名字：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_name, buf);
    //printf("%s\n", message.to_name);
    
    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-2);
        exit(1);
    }

    usleep(200);

    printf("按回车键返回菜单\n");
    getchar();
}

int delete_friend(int conn_fd){
    MSG message;
    char buf[200];
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 15;
    strcpy(message.from_name, myname);

    printf("想要删除的好友:");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_name, buf);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }
    
    usleep(200);
    printf("按回车键返回菜单\n");
    getchar();
}

int choice_group(int conn_fd){
    char choice[10];
    memset(choice, 0, sizeof(choice));

    while(1){
        system("clear");
        printf("----------------------------\n");
        printf("☆      1.查看我的群聊     ☆ \n");
        printf("----------------------------\n");
        printf("☆      2.群聊             ☆ \n");
        printf("----------------------------\n");
        printf("☆      3.申请入群         ☆ \n");
        printf("----------------------------\n");
        printf("☆      4.退出群聊         ☆ \n");
        printf("----------------------------\n");
        printf("       5.管理群             \n");
        printf("----------------------------\n");
        printf("☆      0.返回上级菜单     ☆ \n");
        printf("----------------------------\n");

        while(1){
            printf("请输入你的选择：");
            scanf("%s", choice);
            while(getchar() != '\n')
                continue;

            if(strcmp(choice, "1")==0||strcmp(choice, "2")==0||strcmp(choice, "3")==0||
                strcmp(choice, "4")==0||strcmp(choice, "0")==0||strcmp(choice, "5")==0){
                break;
            }
            else{
                printf("请按照提示输入\n");
            }
        }

        switch(choice[0]){
        case '0':
            return 0;
        case '1':
            view_group(conn_fd);
            break;
        case '2':
            public_chat(conn_fd);
            break;
        case '3':
            add_group(conn_fd);
            break;
        case '4':
            quit_group(conn_fd);
            break;
        case '5':
            manage_group(conn_fd);
            break;
        }
    }
}

int view_group(int conn_fd){
    MSG message;
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));

    message.cmd = 21;
    strcpy(message.user_infor.name, myname);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    sleep(1);
    printf("按回车键返回菜单\n");
    getchar();
}

int public_chat(int conn_fd){
    MSG message;
    char buf[1024];
    char now_time[30];
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 22;

    strcpy(message.from_name, myname);
    printf("请输入群聊的名称：");
    scanf("%s", buf);
    strcpy(message.to_group_name, buf);

    system("clear");

    printf("--------正在 %s 中聊天--------\n", message.to_group_name);
    printf("     ---- quit 退出 ----\n");
    printf("================================\n");

    while(1){
        memset(now_time, 0, sizeof(now_time));
        memset(buf, 0, sizeof(buf));
        strcpy(now_time, my_time());
        //printf("%s %s :\n", now_time, message.from_name);
        scanf("%s", buf);
        while(getchar() != '\n')
            continue;

        if(strcmp(buf, "quit") == 0){
            break;
        }

        strcpy(message.time, now_time);
        strcpy(message.news, buf);
        if(send(conn_fd, &message, sizeof(message), 0) != len){
            my_err("send", __LINE__-1);
        }
    }

    printf("按回车键返回菜单\n");
    getchar();
}

int add_group(int conn_fd){
    MSG message;
    char buf[1024];
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 23;
    strcpy(message.from_name, myname);

    printf("请输入你想要加入的群名：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_group_name, buf);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    usleep(100);
    printf("按回车键返回菜单\n");
    getchar();
}

int quit_group(int conn_fd){
    MSG message;
    int len = sizeof(message);
    char buf[200];
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 24;
    strcpy(message.from_name, myname);

    printf("请输入你想退出群名：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_group_name, buf);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    usleep(200);
    printf("按回车键返回菜单\n");
    getchar();
}

int manage_group(int conn_fd){
    char choice[10];
    memset(choice, 0, sizeof(choice));

    while(1){
        system("clear");
        printf("---------------------------\n");
        printf("☆      1.查看群成员      ☆ \n");
        printf("---------------------------\n");
        printf("☆      2.解散群          ☆ \n");
        printf("---------------------------\n");
        printf("☆      3.设置管理员      ☆ \n");
        printf("---------------------------\n");
        printf("       4.移出群成员        \n");
        printf("---------------------------\n");
        printf("☆      0.返回上级菜单    ☆ \n");
        printf("---------------------------\n");

        while(1){
            printf("请输入你的选择：");
            scanf("%s", choice);
            while(getchar() != '\n')
                continue;

            if(strcmp(choice, "1")==0||strcmp(choice, "2")==0||strcmp(choice, "3")==0||
               strcmp(choice, "4")==0||strcmp(choice, "0")==0){
                break;
            }
            else{
                printf("请按照提示输入\n");
            }
        }

        switch(choice[0]){
        case '0':
            return 0;
        case '1':
            view_group_member(conn_fd);
            break;
        case '2':
            break_group(conn_fd);
            break;
        case '3':
            set_manager(conn_fd);
            break;
        }
    }
}

int view_group_member(int conn_fd){
    MSG message;
    int len = sizeof(message);
    char buf[200];
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 251;

    printf("请输入你要查看的群:");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_group_name, buf);

    strcpy(message.from_name, myname);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    sleep(1);
    printf("按回车键返回菜单");
    getchar();
}

int break_group(int conn_fd){
    MSG message;
    int len = sizeof(message);
    char buf[200];
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 252;

    printf("请输入你要解散的群名:");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_group_name, buf);

    strcpy(message.from_name, myname);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    sleep(1);
    printf("按回车键返回菜单\n");
    getchar();
}

int set_manager(int conn_fd){
    MSG message;
    int len = sizeof(message);
    char buf[200];
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 253;

    strcpy(message.from_name, myname);

    printf("请输入你的群:");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_group_name, buf);

    printf("请输入你要让谁成为管理员:");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_name, buf);

    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    usleep(200);
    printf("按回车键返回菜单\n");
    getchar();
}

int deal_request(int conn_fd){
    char choice[10];
    //MSG message;
    //int len = sizeof(message);
    //memset(&message, 0, sizeof(message));
    memset(choice, 0, sizeof(choice));

    while(1){
        system("clear");
        printf("--------------------------\n");
        printf("☆      1.好友申请       ☆ \n");
        printf("--------------------------\n");
        printf("☆      2.入群申请       ☆ \n");
        printf("--------------------------\n");
        printf("       3.文件消息         \n");
        printf("--------------------------\n");
        printf("☆      0.返回上级菜单   ☆ \n");
        printf("--------------------------\n");

        while(1){
            printf("请输入你的选择：");
            scanf("%s", choice);
            while(getchar() != '\n')
                continue;

            if(strcmp(choice, "1")==0||strcmp(choice, "2")==0||strcmp(choice, "3")==0||strcmp(choice, "0")==0){
                break;
            }
            else{
                printf("请按照提示输入\n");
            }
        }

        switch(choice[0]){
        case '0':
            return 0;
        case '1':
            friend_request(conn_fd);
            break;
        case '2':
            group_request(conn_fd);
            break;
        }
    }
}

int friend_request(int conn_fd){
    MSG message;
    int num;
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    //message.cmd = 31;
    

    //如果没有申请，直接返回
    if(head_friend == NULL){
        printf("目前你还没有还有申请\n");
        printf("\n按回车键返回菜单\n");
        getchar();
        return 0;
    }
    
    //显示第一个好友申请
    struct node *current;
    current = head_friend;
    //printf("B\n");
    printf("from_name: %s\n", current->request.from_name);
    message = current->request;
    //printf("A\n");
    printf("%s 想要加你为好友\n", current->request.from_name);

    printf("-------同意输入'y', 不同意输入'n'-------\n");
    char choice[2];
    //scanf("%s", choice);
    while(1){
        printf("请输入你的选择：");
        scanf("%s", choice);
        while(getchar() != '\n')
            continue;
        
        if(strcmp(choice, "y")==0||strcmp(choice, "n")==0){
            break;
        }
        else{
            printf("请按照提示输入\n");
        }
    }

    switch(choice[0]){
    case 'y':  //同意申请
        message.cmd = 311;
        if(send(conn_fd, &message, sizeof(message), 0) != len){
            my_err("send", __LINE__-1);
        }
        printf("%s 和你已经成为好友\n", message.from_name);
        pthread_mutex_lock(&mutex);
        del_head(head_friend);
        pthread_mutex_unlock(&mutex);
        break;
    case 'n':  //拒绝申请
        message.cmd = 312;
        if(send(conn_fd, &message, sizeof(message), 0) != len){
            my_err("send", __LINE__-1);
        }
        printf("你已经拒绝了申请\n");
        pthread_mutex_lock(&mutex);
        del_head(head_friend);
        pthread_mutex_unlock(&mutex);
        break;
    }

    printf("按任意键返回菜单\n");
    getchar();
}

int group_request(int conn_fd){
    MSG message;
    int num;
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    //message.cmd = 31;
    
    //如果没有入群申请，直接返回
    if(head_group == NULL){
        printf("目前没有入群申请\n");
        printf("按回车键返回菜单\n");
        getchar();
        return 0;
    }

    //显示第一个群申请
    struct node *current;
    current = head_group;
    //printf("B\n");
    //printf("from_name: %s\n", current->request.from_name);
    message = current->request;
    //printf("A\n");
    //printf("%s 想要申请入群 [%s]\n", current->request.from_name, current->request.to_group_name);
    printf("%s 申请加入群 [%s]\n", message.from_name, message.to_group_name);

    printf("-------同意输入'y', 不同意输入'n'-------\n");
    char choice[2];
    //scanf("%s", choice);
    while(1){
        printf("请输入你的选择：");
        scanf("%s", choice);
        if(strcmp(choice, "y")==0||strcmp(choice, "n")==0){
            break;
        }
        else{
            printf("请按照提示输入\n");
        }
    }

    switch(choice[0]){
    case 'y':  //同意申请
        message.cmd = 321;
        if(send(conn_fd, &message, sizeof(message), 0) != len){
            my_err("send", __LINE__-1);
        }
        //printf("%s 和你已经成为好友\n", message.from_name);
        printf("%s 已加入群聊 [%s]\n", message.from_name, message.to_group_name);
        pthread_mutex_lock(&mutex);
        del_head(head_group);
        pthread_mutex_unlock(&mutex);
        break;
    case 'n':  //拒绝申请
        message.cmd = 322;
        if(send(conn_fd, &message, sizeof(message), 0) != len){
            my_err("send", __LINE__-1);
        }
        printf("你已经拒绝了申请\n");
        pthread_mutex_lock(&mutex);
        del_head(head_group);
        pthread_mutex_unlock(&mutex);
        break;
    }

    printf("按任意键返回菜单\n");
    getchar();
}

//创建群聊
int create_group(int conn_fd){
    MSG message;
    char buf[20];
    int len = sizeof(MSG);
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 5;

    strcpy(message.group.group_owner, myname); //群主
    printf("请输入群名：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;

    strcpy(message.group.group_name, buf);  //群名
    
    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }
    
    sleep(1);
    printf("按任意键返回菜单\n");
    getchar();
    //getchar();
}

int send_file(int conn_fd){
    MSG message;
    int file_len;
    char buf[1024];
    int len = sizeof(message);
    memset(&message, 0, sizeof(message));
    memset(buf, 0, sizeof(buf));

    message.cmd = 6;
    strcpy(message.from_name, myname);

    printf("请输入文件的绝对路径：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.file_path, buf);

    printf("请输入文件的接收者：：");
    scanf("%s", buf);
    while(getchar() != '\n')
        continue;
    strcpy(message.to_name, buf);

    //打开文件
    printf("%s\n", message.file_path);
    int fd = open(message.file_path, O_RDONLY);
    if(fd == -1){
        printf("请输入正确的路径名称\n");
        printf("按回车键返回菜单\n");
        getchar();
    }
    
    //计算文件大小
    if(lseek(fd, 0, SEEK_END) == -1){
        my_err("lseek", __LINE__-1);
    }    
    file_len = lseek(fd, 0, SEEK_CUR);
    if(file_len == -1){
        my_err("lseek", __LINE__-2);
    }
    if(lseek(fd, 0, SEEK_SET) == -1){
        my_err("lseek", __LINE__-1);
    }
    message.file_len = file_len;

    //通知服务器开始收文件
    if(send(conn_fd, &message, sizeof(message), 0) != len){
        my_err("send", __LINE__-1);
    }

    int size = 0;
    /*while(1){
        if(size = read(fd, message.news, sizeof(message.news)-1)){
            printf("size:%d", size);
            send(conn_fd, message.news, sizeof(message.news)-1, 0);
            printf("正在发送文件...\n");
        }
        else{
            break;
        }
    }*/
    message.send_end = 0;
    while((size = read(fd, message.news, sizeof(message.news))) > 0){
        printf("正在发送文件......size = %d\n", size);
        message.once_len = htons(size);
        send(conn_fd, &message, sizeof(message), 0);
        memset(&message, 0, sizeof(message));
        message.send_end = 0;
        message.cmd = 6;
    }
    printf("发送结束...\n");
    //memset(&message, 0, sizeof(message));
    //message.cmd = 6;
    //message.send_end = 1;
    //send(conn_fd, &message, sizeof(message), 0);  //通知服务器发送结束
    
    //printf("------size:%d\n", size);

    printf("按回车键返回菜单\n");
    getchar();
    //getchar();
    //关闭文件
    close(fd);
}

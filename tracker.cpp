#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include <set>
#include <unordered_map>
#include <unordered_set>

#define BACKLOG 20
// #define MAXDATASIZE 16384
#define MAXDATASIZE 512

using namespace std;

struct peer_details
{
    char *peer_ip;
    int fd;
};

struct user_details {
    string user_name;
    string password;
    int id;
    bool is_logged_in;
    //keep is_logged_in here only rather than id
    // char *ip;
    // char *port;
    string ip;
    string port;
    unordered_set<string> part_of_groups;
};

struct file_details {
    // int file_size;
    unordered_set<string> shared_by_users;
    //maintain sha also here
};

struct group_details {
    string group_id;
    string admin;
    unordered_set<string> list_of_users;
    unordered_set<string> pending_requests;
    unordered_map<string, file_details> file_to_details;
    // unordered_map<string, unordered_map<string, file_details>> file_to_details;
    //map of filename -> (map of username -> file_details)
};

map<string, int> command_to_item;
map<string, user_details> user_name_to_details;
map<string, group_details> group_to_details;
vector<bool> logged_in_users;

int user_id = 0;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int get_in_port(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
    {
        return ((struct sockaddr_in *)sa)->sin_port;
    }
    return ((struct sockaddr_in6 *)sa)->sin6_port;
}

int parse_msg(char *msg) {
    string message = msg;
    istringstream ss(message);
    string command;
    getline(ss, command, ' ');
    auto itr = command_to_item.find(command);
    if(itr == command_to_item.end()) {
        return -1;
    }
    return itr->second;
      
}

vector<string> get_args(char *msg) {
    
    string message = msg;
    istringstream ss(message);
    vector<string> args;
    string word;
    while(getline(ss, word, ' ')) {
        args.push_back(word);
    }
    return args;
}

string get_file_name(string filepath) {
    size_t pos = filepath.find_last_of("/");
    if(pos == string::npos)
        return filepath;
    //check if there is no / in the filepath i.e. pos = npos
    return filepath.substr(pos + 1);
}

string upload_file(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 3) {
        reply = "Incorrect format. Correct format: upload_file <file_path> <group_id>";
        return reply;
    }
    string filepath = args[1];
    string group_id = args[2];
    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "No such group exists";
        return reply;
    }
    unordered_set<string> group_users = it->second.list_of_users;
    auto it2 = group_users.find(user_name);
    if(it2 == group_users.end()) {
        reply = "You are not a part of this group and hence cannot upload a file to that group";
        return reply;
    }
    struct file_details f;
    string file_name = get_file_name(filepath);
    // f.file_name = get_file_name(filepath);
    // it->second.file_to_details[file_name].insert({user_name, f});
    it->second.file_to_details[file_name].shared_by_users.insert(user_name);
    reply = "File uploaded successfully";
    // mp.insert({})
    // it->second.file_to_details.insert({user_name, })
    return reply;
}

string download_file(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 4) {
        reply = "Incorrect format. Correct format: download_file <group_id> <file_name> <destination_path>";
        return reply;
    }
    string group_id = args[1];
    string file_name = args[2];
    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "Group does not exist";
        return reply;
    }
    unordered_set<string> group_members = it->second.list_of_users;
    auto it2 = group_members.find(user_name);
    if(it2 == group_members.end()) {
        reply = "You are not a part of this group";
        return reply;
    }
    unordered_map<string, file_details> mp = it->second.file_to_details;
    if(mp.find(file_name) == mp.end()) {
        reply = "File does not exist in the group";
        return reply;
    }
    unordered_set<string> file_users = mp[file_name].shared_by_users;
    for(auto it3 = file_users.begin(); it3 != file_users.end(); it3++) {
        string file_user = *it3;
        if(user_name_to_details[file_user].is_logged_in) {
            reply += user_name_to_details[file_user].ip;
            reply += ":";
            reply += user_name_to_details[file_user].port;
            reply += ",";
        }
    }
    if(reply == "") {
        reply = "All file seeders/leechers are offline";
        return reply;
    }
    reply = "Peer Details:\n" + reply;
    reply = reply.substr(0, reply.size() - 1);
    return reply;
    
}

string list_files(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 2) {
        reply = "Incorrect format. Correct format: list_files <group_id>";
        return reply;
    }
    string group_id = args[1];
    auto it = group_to_details.find(group_id);
    unordered_set<string> members = it->second.list_of_users;
    if(members.find(user_name) == members.end()) {
        reply = "You are not a member of this group";
        return reply;
    }
    // unordered_map<string, unordered_map<string, file_details>> mp = it->second.file_to_details;
    unordered_map<string, file_details> mp = it->second.file_to_details;
    reply = "There are " + to_string(mp.size()) + " files in this group\n";
    for(auto it2 = mp.begin(); it2 != mp.end(); it2++) {
        reply += it2->first;
        reply += "\n";
    }
    return reply;
}

string stop_share(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 3) {
        reply = "Incorrect format. Correct format: stop_share <group_id> <file_name>";
        return reply;
    }
    string group_id = args[1];
    string file_name = args[2];
    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "Group does not exist";
        return reply;
    }
    // unordered_map<string, unordered_map<string, file_details>> mp = it->second.file_to_details;
    unordered_map<string, file_details> mp = it->second.file_to_details;
    auto it2 = mp.find(file_name);
    if(it2 == mp.end()) {
        reply = "That file does not exist in this group anyway";
        return reply;
    }
    // size_t num_keys_deleted = it2->second.erase(user_name);
    // size_t num_keys_deleted = it->second.file_to_details[file_name].erase(user_name);
    size_t num_keys_deleted = it->second.file_to_details[file_name].shared_by_users.erase(user_name);

    // if there is no user sharing this file anymore, this file's key is deleted
    if(it->second.file_to_details[file_name].shared_by_users.empty()) {
        it->second.file_to_details.erase(file_name);
    }
    if(num_keys_deleted) {
        reply = "Stopped sharing the file";
    }
    else
    {
        reply = "This file was not shared by you anyway";
    }
    return reply;
    

}

string login_user(char *msg, char **u_name) {
    string reply = "";
    cout << *u_name << endl;
    if(strcmp(*u_name, "NA") != 0) {
        cout << "Not NA" << endl;
        reply = "One user has already logged in. Logout first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 3) {
        reply = "Incorrect format. Correct format: login <user_id> <passwd>";
        return reply;
    }
    string user_name = args[1];
    string passwd = args[2];

    auto itr = user_name_to_details.find(user_name);
    if(itr == user_name_to_details.end()) {
        reply = "Username or password is wrong";
        return reply;
    }
    string true_passwd = itr->second.password;
    if(strcmp(true_passwd.c_str(), passwd.c_str()) != 0) {
        reply = "Username or password is wrong";
        return reply;
    }
    printf("User id of this user is: %d\n", itr->second.id);
    user_name_to_details[user_name].is_logged_in = true;
    //lock
    logged_in_users[itr->second.id] = true;
    // for(int b: logged_in_users) {
    //     printf("%d ", b);
    // }
    // printf("\n");

    //lock
    *u_name = &user_name[0];
    cout << *u_name << "here \n";
    reply = "Successfully logged in";
    return reply;
}

string create_user(char *msg) {

    vector<string> args = get_args(msg);
    int num_args = args.size();
    string reply = "";
    if(num_args != 3) {
        reply = "Incorrect format. Correct format: create_user <username> <passwd>";
        return reply;
    }
    
    string user_name = args[1];
    string passwd = args[2];
    printf("user name: %s and passwd: %s\n", user_name.c_str(), passwd.c_str());

    auto itr = user_name_to_details.find(user_name);
    if(itr != user_name_to_details.end()) {
        reply = "Username already exits. Choose some other name";
        return reply;
    }
    struct user_details u;
    u.user_name = user_name;
    u.password = passwd;
    // u.ip = NULL;
    // u.port = NULL;
    
    //lock
    u.id = user_id++;

    user_name_to_details.insert({user_name, u});
    logged_in_users.push_back(0);
    for(int b: logged_in_users) {
        printf("%d ", b);
    }
    printf("\n");
    //lock
    reply = "User registered";
    return reply;
}

string save_ip_port(char *msg, string user_name) {
    string reply = "";
    vector<string> args = get_args(msg);
    string peer_ip = args[1];
    string peer_port = args[2];
    cout << "peer ip " << peer_ip;
    cout << "Peer port: " << peer_port;
    user_name_to_details[user_name].ip = peer_ip;
    user_name_to_details[user_name].port = peer_port;
    return "Logged in";
}

string create_group(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 2) {
        reply = "Incorrect format. Correct format: create_group <group_id>";
        return reply;
    }
    string group_id = args[1];
    auto itr = group_to_details.find(group_id);
    
    if(itr != group_to_details.end()) {
        reply = "This group id already taken. Choose another one";
        return reply;
    }
    
    struct group_details g;
    g.admin = user_name;
    g.group_id = group_id;
    g.list_of_users.insert(user_name);
    // g.pending_requests = set<string>();
    group_to_details.insert({group_id, g});
    reply = "Group " + group_id + " made successfully";

    //added group to user's structure
    auto itr2 = user_name_to_details.find(user_name);
    itr2->second.part_of_groups.insert(group_id);

    return reply;
}

string list_groups(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 1) {
        reply = "Incorrect format. Correct format: list_groups";
        return reply;
    }

    reply = "There are " + to_string(group_to_details.size()) + " groups:\n";

    for(auto it = group_to_details.begin(); it != group_to_details.end(); it++) {
        reply += it->first;
        reply += "\n";
    }
    return reply;
}

string logout(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 1) {
        reply = "Incorrect format. Correct format: logout";
        return reply;
    }
    auto it = user_name_to_details.find(user_name);
    int id = it->second.id;
    user_name_to_details[user_name].is_logged_in = false;
    logged_in_users[id] = false;
    reply = "Logged out successfully";
    return reply;

}

string join_group(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 2) {
        reply = "Incorrect format. Correct format: join_group <group_id>";
        return reply;
    }
    string group_id = args[1];
    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "No such group id exists";
        return reply;
    }

    unordered_set<string> group_members = it->second.list_of_users;
    if(group_members.find(user_name) != group_members.end()) {
        reply = "You are already a part of this group";
        return reply;
    }

    unordered_set<string> pending_requests = it->second.pending_requests;
    auto it2 = pending_requests.find(user_name);
    if(it2 != pending_requests.end()) {
        reply = "You have already requested to join this group earlier. Sit tight. Your request will be taken up soon";
        return reply;
    }

    it->second.pending_requests.insert(user_name);
    reply = "Your join request has been sent";
    return reply;

}

string leave_group(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 2) {
        reply = "Incorrect format. Correct format: leave_group <group_id>";
        return reply;
    }
    string group_id = args[1];
    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "You are not part of that group anyway/That group does not even exist actually";
        return reply;
    }
    unordered_set<string> members = it->second.list_of_users;
    auto it2 = members.find(user_name);
    if(it2 == members.end()) {
        reply = "You are not part of that group anyway\n";
        unordered_set<string> pending_requests = it->second.pending_requests;
        auto it3 = pending_requests.find(user_name);
        if(it3 == pending_requests.end()) {
            reply += "Your join request was also not pending";
        }
        else {
            it->second.pending_requests.erase(user_name);
            reply = "Your join request which was pending has been withdrawn now";
            
        }
    }
    else {
        it->second.list_of_users.erase(user_name);
        reply = "You have left the group " + group_id;
        reply += "\n";
        if(it->second.admin == user_name) {
            members = it->second.list_of_users;
            if(members.empty()) {
                group_to_details.erase(group_id);
                reply += "Deleted the whole group as every member has exited";
            }
            else {
                it->second.admin = *(members.begin());
                reply += "New admin of the group " + group_id + " is " + it->second.admin;
            }
        }
    }
    return reply;
}

string list_requests(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 2) {
        reply = "Incorrect format. Correct format: list_requests <group_id>";
        return reply;
    }
    string group_id = args[1];
    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "You are not an admin of any such group/No such group even exists actually";
        return reply;
    }
    string group_admin = it->second.admin;
    if(group_admin != user_name) {
        reply = "You are not an admin of any such group";
        return reply;
    }
    unordered_set<string> pending_requests = it->second.pending_requests;
    reply = "There are " + to_string(pending_requests.size()) + " pending requests of following users:\n";
    for(auto it2 = pending_requests.begin(); it2 != pending_requests.end(); it2++) {
        reply += *it2;
        reply += "\n";
    }
}

string accept_request(char *msg, string user_name) {
    string reply = "";
    if(user_name == "NA") {
        reply = "Login first";
        return reply;
    }
    vector<string> args = get_args(msg);
    int num_args = args.size();
    if(num_args != 3) {
        reply = "Incorrect format. Correct format: accept_request <group_id> <user_id>";
        return reply;
    }
    
    string group_id = args[1];
    string user_id = args[2];

    auto it = group_to_details.find(group_id);
    if(it == group_to_details.end()) {
        reply = "No such group exists";
        return reply;
    }
    string group_admin = it->second.admin;
    if(group_admin != user_name) {
        reply = "You are not an admin of this group and don't have right to accept requests";
        return reply;
    }
    unordered_set<string> pending_requests = it->second.pending_requests;
    auto it2 = pending_requests.find(user_id);
    if(it2 == pending_requests.end()) {
        reply = "There is no join request from this user in this group";
        return reply;
    }
    it->second.pending_requests.erase(user_id);
    it->second.list_of_users.insert(user_id);
    reply = "User " + user_id + " has been added to group " + group_id;
    return reply;
}

void* serve_peer(void *args) {
    struct peer_details *peer_d = (struct peer_details *)args;
    
    string user_name = "NA";
    char *u_name = &user_name[0];

    char *peer_ip = peer_d->peer_ip;
    int client_fd = peer_d->fd;

    int numbytes;
    char buf[MAXDATASIZE];

    while((numbytes = send(client_fd, "Welcome", strlen("Welcome"), 0)) == -1) {
        perror("could not send");
        sleep(2);
    }

    while(true) {
        bzero(&buf, sizeof buf);
        if((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
            perror("Could not recv");
        }
        buf[numbytes] = 0;
        char msg[numbytes + 1];
        for(int i = 0; i <= numbytes; i++) {
            msg[i] = buf[i];
        }
        // printf("client %d request: '%s'\n", client_fd, msg);
        // if(strcmp(msg, "logout") == 0) {
        //     if((numbytes = send(client_fd, "Bye", strlen("Bye"), 0)) == -1) {
        //         perror("could not send");
        //     }
        //     //should update the data structures accordingly
        //     break;
        // }
        int item = parse_msg(msg);
        printf("client %d request: '%s'\n", client_fd, msg);
        // printf("item: %d\n", item);
        string reply = "Invalid command";
        switch (item)
        {
        case 1:
            reply = create_user(msg);
            break;
        
        case 2:
            reply = login_user(msg, &u_name);
            if(reply == "Successfully logged in") {
                user_name = u_name;
                if((numbytes = send(client_fd, reply.c_str(), strlen(reply.c_str()), 0)) == -1) {
                   perror("could not send");
                }
                bzero(&buf, sizeof buf);
                if((numbytes = recv(client_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                    perror("Could not recv");
                }
                buf[numbytes] = 0;
                char temp_msg[numbytes + 1];
                for(int i = 0; i <= numbytes; i++) {
                    temp_msg[i] = buf[i];
                }
                reply = save_ip_port(temp_msg, user_name);
            }
            // cout << "here printing username : " << user_name << "\n";
            break;

        case 3:
            reply = create_group(msg, user_name);
            break;

        case 4:
            reply = join_group(msg, user_name);
            break;

        case 5:
            reply = leave_group(msg, user_name);
            break;

        case 6:
            reply = list_requests(msg, user_name);
            break;

        case 7:
            reply = accept_request(msg, user_name);
            break;

        case 8:
            reply = list_groups(msg, user_name);    
            break;

        case 9:
            reply = list_files(msg, user_name);
            break;

        case 10:
            reply = upload_file(msg, user_name);
            break;

        case 11:
            reply = download_file(msg, user_name);
            break;

        case 12:
            reply = logout(msg, user_name);
            if(reply == "Logged out successfully") {
                user_name = "NA";
                u_name = &user_name[0];
            }
            break;

        case 14:
            reply = stop_share(msg, user_name);
            break;

        default:
            break;
        }

        // printf("client %d request: '%s'\n", client_fd, msg);
        //need to take action accordingly and perform a task depending on the request
        if((numbytes = send(client_fd, reply.c_str(), strlen(reply.c_str()), 0)) == -1) {
            perror("could not send");
        }
    }

    close(client_fd);
    return NULL;

}

void build_command_to_item_map() {
    command_to_item = {
        {"create_user", 1}, 
        {"login", 2}, 
        {"create_group", 3}, 
        {"join_group", 4}, 
        {"leave_group", 5},
        {"list_requests", 6},
        {"accept_request", 7},
        {"list_groups", 8},
        {"list_files", 9},
        {"upload_file", 10},
        {"download_file", 11},
        {"logout", 12},
        {"show_downloads", 13},
        {"stop_share", 14}
        };
}


int main(int argc, char *argv[])
{
    if(argc != 3) {
        printf("Provide correct format: ./tracker trackerinfo.txt tracker_no\n");
        exit(1);
    }

    build_command_to_item_map();

    struct addrinfo hints, *server_info, *p;
    struct sockaddr_storage client_addr;
    socklen_t sockaddr_st_size = sizeof client_addr;
    char s[INET6_ADDRSTRLEN];


    int result;
    int sock_fd;
    int yes = 1;
    // int client_fd;

    ifstream tracker_file(argv[1]);
    string tracker_details, tracker1_details, tracker2_details;
    int tracker_no = atoi(argv[2]);

    if(tracker_file.is_open()) {
        getline(tracker_file, tracker1_details);
        getline(tracker_file, tracker2_details);
        tracker_file.close();
    }
    else {
        printf("Could not open tracker file. Provide correct path and permissions\n");
        exit(1);
    }

    if(tracker_no == 1)
        tracker_details = tracker1_details;
    else
        tracker_details = tracker2_details;

    char *tracker_ip = strtok(&tracker_details[0], ":");
    char *tracker_port = strtok(NULL, ":");

    printf("The tracker ip is %s and port is %s\n", tracker_ip, tracker_port);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((result = getaddrinfo(tracker_ip, tracker_port, &hints, &server_info)) != 0) {
        printf("getaddreinfo: %s\n", gai_strerror(result));
        exit(1);
    }

    for(p = server_info; p != NULL; p = p->ai_next) {
        if((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
            perror("server: setsockopt");
            continue;
        }

        if(bind(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sock_fd);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(server_info);

    if(p == NULL) {
        perror("server: could not bind");
        exit(1);
    }

    if(listen(sock_fd, BACKLOG) == -1) {
        perror("server: listen");
        exit(1);
    }

    printf("Server is up and running... Waiting for connections\n");

    while(true) {
        int client_fd;
        if((client_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sockaddr_st_size)) == -1) {
            // failed to accept
            perror("server:acept");
            continue;
        }
        
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        // struct sockaddr_in add;
        
        printf("server: got connection from: %s and sockfd : %d\n", s, client_fd);
        struct peer_details peer_d;
        peer_d.peer_ip = s;
        peer_d.fd = client_fd;

        pthread_t client;
        pthread_create(&client, NULL, serve_peer, (void *)&peer_d);
    }


    return 0;
}

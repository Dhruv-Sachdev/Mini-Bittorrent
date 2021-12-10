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
#include <pthread.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <sys/stat.h>
#include <vector>
#include <unordered_set>
#include <cmath>

// #define MAXDATASIZE 16384
#define MAXDATASIZE 512
#define BACKLOG 20
// #define CHUNK_SIZE 8
#define CHUNK_SIZE 256

using namespace std;

struct ip_port
{
    char* ip;
    char* port;
};

struct tracker_peer_details {
    char* tracker_ip;
    char* tracker_port;
    char* peer_ip;
    char* peer_port;
};

struct file_details
{
    long long file_size;
    vector<bool> is_chunk_present;
    string filepath;
    
};

struct file_download_details {
    string group_id;
    string dest_path;
    string file_name;
    vector<pair<string, string>> peer_ip_port;
};

struct chunk_download_details {
    string group_id;
    string dest_path;
    string file_name;
    string ip;
    string port;
};

struct download_details {
    string group_id;
    string dest_path;
    string file_name;
    string ip;
    string port;
    vector<int> chunks_to_download; 
};


unordered_map<string, vector<string>> downloads;
unordered_map<string, unordered_map<string, unordered_map<string, file_details>>> user_name_to_group_to_file_details; 
string user_name;
char home_path[1000];
unordered_map<string, int> command_to_item;


string get_absolute_path(string path) {
    string abs_path;
    if(path[0] != '/') {
        abs_path = home_path;
        abs_path += "/";
        abs_path += path;
        return abs_path;
    }
    return path;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
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

void update_user_name(string u) {
    user_name = u;
}

long long get_file_size(string file_path) { 
    struct stat st;
    stat(file_path.c_str(), &st);
    return st.st_size;
}

void update_file_size(string file_path, struct file_details *f) {
    long long fs = get_file_size(file_path);
    // printf("Got file size %lld \n", fs);
    f->file_size = fs;
    // printf("Set file size\n");
}

int get_num_chunks(long long file_size) {
    // return ceil(1.0*file_size / (CHUNK_SIZE * 1024));
    return ceil(1.0*file_size / (CHUNK_SIZE));

}

void update_file_chunks(long long file_size, struct file_details *f) {
    int num_chunks = get_num_chunks(file_size);
    f->is_chunk_present.resize(num_chunks, true);
    vector<bool> chunk_info = f->is_chunk_present;

    // for(int i = 0; i < chunk_info.size(); i++) {
    //     printf(chunk_info[i] ? "true " : "false ");
    // }
}

void update_file_details(string file_path, struct file_details *f) {
    update_file_size(file_path, f);
    // printf("Updated  file size now\n");
    // printf("filesize %lld\n", f->file_size);
    update_file_chunks(f->file_size, f);
    // printf("Updated  file chunks now\n");

    f->filepath = file_path;
}

string get_file_name(string filepath) {
    size_t pos = filepath.find_last_of("/");
    if(pos == string::npos)
        return filepath;
    //check if there is no / in the filepath i.e. pos = npos
    return filepath.substr(pos + 1);
}

void add_file(string filepath, string group_id) {
    struct file_details f;
    // printf("Updating file details now %lld\n", f.file_size);
    
    update_file_details(filepath, &f);
    // printf("Client side: Updated file details\n");
    string filename = get_file_name(filepath);
    user_name_to_group_to_file_details[user_name][group_id][filename] = f;

    // checking if things are correct
    // f = user_name_to_group_to_file_details[user_name][group_id][filename];
    // printf("%lld\n", f.file_size);
    // printf("%s\n", &f.filepath[0]);
    // vector<bool> chunk_info = f.is_chunk_present;
    // for(int i = 0; i < chunk_info.size(); i++) {
    //     printf(chunk_info[i] ? "true " : "false ");
    // }
    // printf("\n");
}

vector<string> get_args(string msg, char delimiter) {
    vector<string> res;
    string t;
    for(int i = 0; i < msg.size(); i++) {
        if(msg[i] == delimiter) {
            res.push_back(t);
            t = "";
        }
        else {
            t += msg[i];
        }
    }
    if(t != "") {
        res.push_back(t);
    }
    return res;
}

//filename, groupname, username, peper ips/ports

void *dummy(void *args) {
    while(true) {

    }
}

void *get_file_size_from_peer(void *args) {
    struct chunk_download_details cdd = *((struct chunk_download_details *)args);
    string peer_ip = cdd.ip;
    string peer_port = cdd.port;
    // printf("peer port in get_file_size_from_peer: %s\n", &peer_port[0]);
    string file_name = cdd.file_name;
    string group_id = cdd.group_id;

    struct addrinfo hints, *servinfo, *p;
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char send_msg[MAXDATASIZE];
    char s[INET6_ADDRSTRLEN];
    int rv;
    int exit_status_one = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(peer_ip.c_str(), peer_port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        pthread_exit(&exit_status_one);
    }
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = (socket(p->ai_family, p->ai_socktype, p->ai_protocol))) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("client: connect");
            continue;
        }

        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        pthread_exit(&exit_status_one);
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    // printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo);
    string msg = "get_file_size";
    msg += " ";
    msg += file_name;
    msg += " ";
    msg += group_id;
    // printf("sending msg: %s\n", &msg[0]);

    if(send(sockfd, msg.c_str(), strlen(msg.c_str()), 0) == -1) {
        perror("Could not send msg\n");
    }

    if((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("Could not receive data\n");
    }
    buf[numbytes] = '\0';
    // printf("Peer replied: %s\n", buf);
    char recv_msg[numbytes + 1];
    for(int i = 0; i <= numbytes; i++) {
        recv_msg[i] = buf[i];
    }
    // cout << peer_port << " " << recv_msg << "\n";
    close(sockfd);
    long long *file_size = (long long *)malloc(sizeof(long long));
    *file_size = atoll(recv_msg);
    // printf("File size receved from peer: %lld\n", *file_size);
    return file_size;
}

void *download_chunks(void *args) {
    // printf("Starting to download chunk\n");
    struct download_details dd = *((struct download_details *)args);
    string peer_ip = dd.ip;
    string peer_port = dd.port;
    // printf("peer port in download_chunk_info: %s\n", &peer_port[0]);
    string file_name = dd.file_name;
    string group_id = dd.group_id;
    string dest_path = dd.dest_path;
    vector<int> chunks_to_download = dd.chunks_to_download;
    // printf("Chunks to download from peer %s %d\n", &peer_port[0], chunks_to_download.size());
    // for(int i = 0; i < chunks_to_download.size(); i++) {
    //     cout << chunks_to_download[i] << " ";
    // }
    // cout << endl;
    if(chunks_to_download.empty()) {
        pthread_exit(NULL);
    }

    struct addrinfo hints, *servinfo, *p;
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char send_msg[MAXDATASIZE];
    char s[INET6_ADDRSTRLEN];
    int rv;
    int exit_status_one = 1;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;


    if ((rv = getaddrinfo(peer_ip.c_str(), peer_port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        pthread_exit(&exit_status_one);
    }
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = (socket(p->ai_family, p->ai_socktype, p->ai_protocol))) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("client: connect");
            continue;
        }

        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        pthread_exit(&exit_status_one);
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    // printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo);

    for(int i = 0; i < chunks_to_download.size(); i++) {
        int chunk_no = chunks_to_download[i];
        string msg = "download_chunk";
        msg += " ";
        msg += file_name;
        msg += " ";
        msg += group_id;
        msg += " ";
        msg += to_string(chunk_no);
        // printf("sending msg: %s\n", &msg[0]);
        
        if(send(sockfd, msg.c_str(), strlen(msg.c_str()), 0) == -1) {
            perror("Could not send msg\n");
        }

        bzero(&buf, sizeof(buf));
        if((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
            perror("Could not receive data\n");
        }
        buf[numbytes] = 0;
        // printf("Peer replied:\n%s\n", buf);

        // char recv_msg[numbytes + 1];
        // for(int i = 0; i <= numbytes; i++) {
        //     recv_msg[i] = buf[i];
        // }
        // cout << peer_port << " " << recv_msg << "\n";
        // clog << peer_port << " Opening the file to write\n";
        // clog << peer_port << " dest path: " << dest_path << endl;
        FILE* f_write = fopen(dest_path.c_str(), "r+");
        // clog << peer_port << " Opened\n";
        fseek(f_write, chunk_no * CHUNK_SIZE, SEEK_SET);
        // clog << peer_port << " Seeked\n";
        fwrite(buf, sizeof(char), numbytes, f_write);
        // clog << peer_port << " wrote\n";
        //write this chunk to the file with fseek and fwrite
        fclose(f_write);
    }
    send(sockfd, "download_chunk a b -1", strlen("download_chunk a b done"), 0);
    close(sockfd);
    pthread_exit(NULL);
}

void *download_chunk_info(void *args) {
    // printf("Starting download chunk info\n");
    struct chunk_download_details cdd = *((struct chunk_download_details *)args);
    string peer_ip = cdd.ip;
    string peer_port = cdd.port;
    // printf("peer port in download_chunk_info: %s\n", &peer_port[0]);
    string file_name = cdd.file_name;
    string group_id = cdd.group_id;

    struct addrinfo hints, *servinfo, *p;
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char send_msg[MAXDATASIZE];
    char s[INET6_ADDRSTRLEN];
    int rv;
    int exit_status_one = 1;


    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;


    if ((rv = getaddrinfo(peer_ip.c_str(), peer_port.c_str(), &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        pthread_exit(&exit_status_one);
    }
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = (socket(p->ai_family, p->ai_socktype, p->ai_protocol))) == -1)
        {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("client: connect");
            continue;
        }

        break;
    }
    if (p == NULL)
    {
        fprintf(stderr, "client: failed to connect\n");
        pthread_exit(&exit_status_one);
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    // printf("client: connecting to %s\n", s);
    freeaddrinfo(servinfo);
    string msg = "get_chunk_vector";
    msg += " ";
    msg += file_name;
    msg += " ";
    msg += group_id;
    // printf("sending msg: %s\n", &msg[0]);

    if(send(sockfd, msg.c_str(), strlen(msg.c_str()), 0) == -1) {
        perror("Could not send msg\n");
    }

    if((numbytes = recv(sockfd, buf, MAXDATASIZE, 0)) == -1) {
        perror("Could not receive data\n");
    }
    buf[numbytes] = '\0';
    // printf("Peer replied: %s\n", buf);
    char recv_msg[numbytes + 1];
    for(int i = 0; i <= numbytes; i++) {
        recv_msg[i] = buf[i];
    }
    // cout << peer_port << " " << recv_msg << "\n";
    close(sockfd);
    pthread_exit((void *)recv_msg);
}

void *download_helper(void *args) {
    // printf("starting download helper\n");
    struct file_download_details fdd = *((struct file_download_details *)args);
    // printf("Made a struct\n");
    vector<pair<string, string>> peer_ip_port = fdd.peer_ip_port;
    // for(int i = 0; i < peer_ip_port.size(); i++) {
    //     printf("in download helper: %s %s\n", &(peer_ip_port[i].first)[0], &(peer_ip_port[i].second)[0]);
    // }
    // printf("Made a vector of pair\n");
    string group_id = fdd.group_id;
    // printf("Made a group\n");
    string file_name = fdd.file_name;
    // printf("Made a file_name\n");
    string dest_dir = fdd.dest_path;
    string dest_path = dest_dir;
    dest_path += "/";
    dest_path += file_name;
    // clog << "Destination path in download helper: " << dest_path << endl;
    // printf("Made dest_path\n");
    int num_peers = peer_ip_port.size();
    // printf("Downloading from %d peers\n", num_peers);
    pthread_t connect[num_peers];
    char *chunk_info[num_peers];
    
    vector<chunk_download_details> temp(num_peers);

    for(int i = 0; i < num_peers; i++) {
        temp[i].group_id = group_id;
        temp[i].file_name = file_name;
        temp[i].dest_path = dest_path;
        temp[i].ip = peer_ip_port[i].first;
        temp[i].port = peer_ip_port[i].second;
        // printf("For thread %d, sending port num : %s\n", i, &(temp[i].port)[0]);
        pthread_create(&connect[i], NULL, download_chunk_info, &temp[i]);

        // printf("Thread %d created\n", i);
    }
    // long long file_size = get_file_size_from_peer(&temp[0]);

    for(int i = 0; i < num_peers; i++) {
        pthread_join(connect[i], (void **) &chunk_info[i]);
    }
    
    // for(int i = 0; i < num_peers; i++) {
    //     cout << chunk_info[i] << "\n";
    // }
    
    // printf("Received chunk info from all threads alright\n");

    pthread_t fs;
    long long *file_size;
    pthread_create(&fs, NULL, get_file_size_from_peer, &temp[0]);
    pthread_join(fs, (void **)&file_size);
    // printf("File size received after return from thread %lld\n", *file_size);

    FILE *fp = fopen(dest_path.c_str(), "w");
    ftruncate(fileno(fp), *file_size);
    fclose(fp);

    vector<string> chunk_vectors(num_peers);
    vector<vector<int>> chunks_to_download(num_peers);
    // int num_chunks = chunk_vectors[0].size();
    // int num_chunks = strlen(chunk_info[0]);
    int num_chunks = get_num_chunks(*file_size);
    // cout << "Num chunks : " << num_chunks << endl;
    vector<int> indices(num_peers, 0);
    vector<bool> chunk_selected(num_chunks, false);

    for(int i = 0; i < num_peers; i++) {
        chunk_vectors[i] = chunk_info[i];
        // cout << "chunk vector " << chunk_vectors[i] << endl;
    }
    
    // piece selection algorithm and this will be applied only if there are all the chunks in the network
    printf("Entering piece selection algo\n");
    int peer_no = 0;
    for(int i = 0; i < num_chunks;) {
        int index = indices[peer_no];
        while(index < num_chunks) {
            if(chunk_info[peer_no][index] == '0' || chunk_selected[index])
                index++;
            else {
                chunks_to_download[peer_no].push_back(index);
                chunk_selected[index] = 1;
                index++;
                i++;
                break;
            }
        }
        indices[peer_no] = index;
        peer_no = (peer_no + 1) % num_peers;
    }
    //-------------
    printf("Done with piece algo\n");
    // for(int i = 0; i < num_peers; i++) {
    //     for(int j = 0; j < chunks_to_download[i].size(); j++) {
    //         cout << chunks_to_download[i][j] << " ";
    //     }
    //     cout << endl;
    // }

    //can create a file of filesize here
    // Now create some threads to actually download the chunks
    

    //write into this file in diff threads
    pthread_t download_thread[num_peers];
    vector<download_details> dd(num_peers);
    for(int i = 0; i < num_peers; i++) {
        dd[i].group_id = group_id;
        dd[i].file_name = file_name;
        dd[i].dest_path = dest_path;
        dd[i].ip = peer_ip_port[i].first;
        dd[i].port = peer_ip_port[i].second;
        dd[i].chunks_to_download = chunks_to_download[i];
        pthread_create(&download_thread[i], NULL, download_chunks, (void *)&dd[i]);
    }

    for(int i = 0; i < num_peers; i++) {
        pthread_join(download_thread[i], NULL);
    }

    //structure where I can add the filename to the completed downloads

    return NULL;
}

void download_file(char buf[], string sent_msg, int numbytes) {


    vector<string> sent_msg_args = get_args(&sent_msg[0]);
    string group_id = sent_msg_args[1];
    string filename = sent_msg_args[2];
    string dest_path = sent_msg_args[3];
    char rec_msg[numbytes + 1];
    for(int i = 0; i <= numbytes; i++) {
        rec_msg[i] = buf[i];
    }
    string msg = rec_msg;
    int pos = msg.find("\n");
    msg = msg.substr(pos + 1);
    // printf("IP ports %s\n", msg.c_str());
    vector<string> peer_details = get_args(msg, ',');
    // for(int i = 0; i < peer_details.size(); i++) {
    //     printf("%s ", &peer_details[i][0]);
    // }
    vector<pair<string, string>> peer_ip_details;
    for(int i = 0; i < peer_details.size(); i++) {
        vector<string> ip_port = get_args(peer_details[i], ':');
        // printf("ip port: %s %s", &ip_port[0][0], &ip_port[1][0]);
        peer_ip_details.push_back({ip_port[0], ip_port[1]});
    }
    // printf("Reached here\n");
    struct file_download_details fdd;
    fdd.file_name = filename;
    fdd.group_id = group_id;
    fdd.dest_path = dest_path;
    fdd.peer_ip_port = peer_ip_details;
    pthread_t download;
    pthread_create(&download, NULL, download_helper, (void *)&fdd);
    // pthread_join(download, NULL);
}

// void process_response(char buf[], int numbytes, string sent_msg, int sock_fd, struct tracker_peer_details *data) {
//     if(strncmp(buf, "Successfully logged in", numbytes) == 0) {
//         printf("Client side: received successfully logged in\n");
//         vector<string> args = get_args(&sent_msg[0]);
//         update_user_name(args[1]);
//         string msg = "set_ip ";
//         msg += data->peer_ip;
//         msg += " ";
//         msg += data->peer_port;
//         printf("Sending msg: %s\n", msg.c_str());
//         char buffer[MAXDATASIZE];
//         int numbytes;
//         if(send(sock_fd, msg.c_str(), strlen(msg.c_str()), 0) == -1) {
//             printf("Could not send ip and port\n");
//         }
//         if((numbytes = recv(sock_fd, buffer, sizeof(buffer), 0)) == -1) {
//             printf("Could not receive from tracker\n");
//         }
//         buffer[numbytes] = '\0';
//         // printf("server says: %s\n", buffer);

//     }

//     else if(strncmp(buf, "File uploaded successfully", numbytes) == 0) {
//         printf("Client side: received file uploaded successfully\n");
//         vector<string> args = get_args(&sent_msg[0]);
//         printf("Client side: Got args\n");
//         string filepath = get_absolute_path(args[1]);
//         printf("Client side: Got abs path %s \n", &filepath[0]);
//         string group_id = args[2];
//         add_file(filepath, group_id);
//     }

//     else if(strncmp(buf, "Peer Details:", strlen("Peer Details:")) == 0) {
//         download_file(buf, sent_msg, numbytes);
//     }

// }

void *connect_to_peer(void *args) {
    struct ip_port *data;
    // string ip = "127.0.0.1";
    // string port = "9000";
    string ip, port;
    cout << "Give ip to connect to\n";
    getline(cin, ip);
    cout << "Give port to connect to\n";
    getline(cin, port);

    cout << ip << " " << port << "\n";
    // printf("Now connecting to the peer 8000\n");

    struct addrinfo hints, *server_info, *p;
    char buf[MAXDATASIZE];
    string send_msg;
    char s[INET6_ADDRSTRLEN];
    int result;
    int exit_status_one = 1;
    int sock_fd;
    int numbytes;
    
    
    // printf("ip : %s\n", data->ip);
    // printf("port: %s\n", data->port);
    printf("Reached here\n");
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if((result = getaddrinfo(ip.c_str(), port.c_str(), &hints, &server_info)) != 0) {
        printf("getaddreinfo: %s\n", gai_strerror(result));
        pthread_exit(&exit_status_one);
    }

    for(p = server_info; p != NULL; p=p->ai_next) {
        if((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client:socket");
            continue;
        }

        if(connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client:connect");
            continue;
        }

        break;
    }
    if(p == NULL) {
        perror("client: could not connect in anyway");
        pthread_exit(&exit_status_one);
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(server_info);

    // send(sock_fd, "requesting file x", strlen("requesting file x"), 0);
    // if ((numbytes = recv(sock_fd, buf, MAXDATASIZE - 1, 0)) == -1)
    // {
    //     perror("recv");
    //     exit(1);
    // }
    // buf[numbytes] = '\0';
    // printf("peer 8000 says: %s\n", buf);

    while (true)
    {
        if ((numbytes = recv(sock_fd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }
        buf[numbytes] = '\0';
        printf("peer says: %s\n", buf);

        if(strncmp(buf, "Bye", 3) == 0) {
            break;
        }

        // fgets(send_msg, MAXDATASIZE - 1, stdin);
        getline(cin, send_msg);
        
        // if(strcmp(send_msg.c_str(), "connect") == 0) {
        //     printf("detected connect msg\n");
        //     pthread_t peer;
        //     if(pthread_create(&peer, NULL, connect_to_peer, NULL) != 0) {
        //         perror("Could not create thread\n");
        //     }
        
        //     pthread_join(peer, NULL);
        //     printf("This should be printed only after peer connection is over\n");
        // // could check if this is a connect call and make a new thread for connecting to a peer
        // }
        
        // else {
        cout << "Sending msg " << send_msg << "\n";
        if (send(sock_fd, send_msg.c_str(), strlen(send_msg.c_str()), 0) == -1)
        {
            perror("Could not send a msg");
        }
        cout << "Sent" << "\n";
        // }
    }

    printf("closing socket fd: %d\n", sock_fd);
    close(sock_fd);
    
    return NULL;

}

string get_dir_from_path(string filepath) {
    size_t pos = filepath.find_last_of("/");
    if(pos == string::npos)
        return "/home/dhruv";
    //check if there is no / in the filepath i.e. pos = npos
    return filepath.substr(0, pos + 1);
}

bool process_send_msg(string msg) {
    vector<string> args = get_args(&msg[0]);
    if(args[0] == "upload_file") {
        if(args.size() != 3) {
            printf("Incorrect format: Correct format is upload_file​ <file_path> <group_id​>\n");
            return false;
        }
        // printf("file path %s\n", &args[1][0]);
        string filepath = get_absolute_path(args[1]);
        // printf("abs file path %s\n", &filepath[0]);
        struct stat st;
        if(stat(filepath.c_str(), &st) != 0) {
            printf("Provide a valid filepath\n");
            return false;
        }
        return true;
    }
    else if(args[0] == "download_file") {
        if(args.size() != 4) {
            printf("Incorrect format: Correct format is download_file​ <group_id> <file_name> <destination_path>");
            return false;
        }
        string dest_path = get_absolute_path(args[3]);
        string dest_dir = get_dir_from_path(dest_path);
        struct stat st;
        if(stat(dest_dir.c_str(), &st) != 0) {
            printf("Provide a valid dest path\n");
            return false;
        }
        return true;
    }
}

void *make_client(void *args)
{   
    //this thread is to connect to the tracker and communicate
    // struct ip_port *data = (struct ip_port *)args;
    struct tracker_peer_details *data = (struct tracker_peer_details *)args;
    struct addrinfo hints, *server_info, *p;
    char buf[MAXDATASIZE];
    string send_msg;
    char s[INET6_ADDRSTRLEN];
    int result;
    int exit_status_one = 1;
    int sock_fd;
    int numbytes;
    
    // printf("Reached in the client thread\n");
    // printf("ip : %s\n", data->tracker_ip);
    // printf("port: %s\n", data->tracker_port);
    struct file_download_details fdd;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if((result = getaddrinfo(data->tracker_ip, data->tracker_port, &hints, &server_info)) != 0) {
        printf("getaddreinfo: %s\n", gai_strerror(result));
        pthread_exit(&exit_status_one);
    }

    for(p = server_info; p != NULL; p=p->ai_next) {
        if((sock_fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client:socket");
            continue;
        }

        if(connect(sock_fd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("client:connect");
            continue;
        }

        break;
    }
    if(p == NULL) {
        perror("client: could not connect in anyway");
        pthread_exit(&exit_status_one);
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
    printf("client: connecting to %s\n", s);
    freeaddrinfo(server_info);

   while (true)
    {
        // printf("In the loop with talking to server\n");
        if ((numbytes = recv(sock_fd, buf, MAXDATASIZE - 1, 0)) == -1)
        {
            perror("recv");
            exit(1);
        }
        buf[numbytes] = '\0';

        // process_response(buf, numbytes, send_msg, sock_fd, data);

        if(strncmp(buf, "Successfully logged in", numbytes) == 0) {
            // printf("Client side: received successfully logged in\n");
            vector<string> args = get_args(&send_msg[0]);
            update_user_name(args[1]);
            string msg = "set_ip ";
            msg += data->peer_ip;
            msg += " ";
            msg += data->peer_port;
            printf("Sending msg: %s\n", msg.c_str());
            char buffer[MAXDATASIZE];
            int numbytes;
            if(send(sock_fd, msg.c_str(), strlen(msg.c_str()), 0) == -1) {
                printf("Could not send ip and port\n");
            }
            if((numbytes = recv(sock_fd, buffer, sizeof(buffer), 0)) == -1) {
                printf("Could not receive from tracker\n");
            }
            buffer[numbytes] = '\0';
            // printf("server says: %s\n", buffer);

        }

        else if(strncmp(buf, "File uploaded successfully", numbytes) == 0) {
            // printf("Client side: received file uploaded successfully\n");
            vector<string> args = get_args(&send_msg[0]);
            // printf("Client side: Got args\n");
            string filepath = get_absolute_path(args[1]);
            // printf("Client side: Got abs path %s \n", &filepath[0]);
            string group_id = args[2];
            add_file(filepath, group_id);
        }

        else if(strncmp(buf, "Peer Details:", strlen("Peer Details:")) == 0) {
            // should create a thread directly for downloading a file

            // download_file(buf, send_msg, numbytes);

            vector<string> sent_msg_args = get_args(&send_msg[0]);
            string group_id = sent_msg_args[1];
            string filename = sent_msg_args[2];
            string dest_path = get_absolute_path(sent_msg_args[3]);
            char rec_msg[numbytes + 1];
            for(int i = 0; i <= numbytes; i++) {
                rec_msg[i] = buf[i];
            }
            string msg = rec_msg;
            int pos = msg.find("\n");
            msg = msg.substr(pos + 1);
            // printf("IP ports %s\n", msg.c_str());
            vector<string> peer_details = get_args(msg, ',');
            // for(int i = 0; i < peer_details.size(); i++) {
            //     printf("%s ", &peer_details[i][0]);
            // }
            // printf("\n");
            vector<pair<string, string>> peer_ip_details;
            for(int i = 0; i < peer_details.size(); i++) {
                vector<string> ip_port = get_args(peer_details[i], ':');
                // printf("ip port: %s %s", &ip_port[0][0], &ip_port[1][0]);
                peer_ip_details.push_back({ip_port[0], ip_port[1]});
            }
            // printf("\n");
            // printf("Reached here\n");
            // struct file_download_details fdd;
            fdd.file_name = filename;
            fdd.group_id = group_id;
            fdd.dest_path = dest_path;
            fdd.peer_ip_port = peer_ip_details;
            pthread_t download;
            // printf("Creating a thread for download\n");
            pthread_create(&download, NULL, download_helper, (void *)&fdd);
            // pthread_join(download, NULL);
        }


        printf("server says: %s\n", buf);

        if(strncmp(buf, "Bye", 3) == 0) {
            break;
        }

        // fgets(send_msg, MAXDATASIZE - 1, stdin);
        bool is_proper_msg = false;
        while(!is_proper_msg) {
            getline(cin, send_msg);
            is_proper_msg = process_send_msg(send_msg);
        }
        
        if(strcmp(send_msg.c_str(), "connect") == 0) {
            printf("detected connect msg\n");
            pthread_t peer;
            if(pthread_create(&peer, NULL, connect_to_peer, NULL) != 0) {
                perror("Could not create thread\n");
            }
        
            pthread_join(peer, NULL);
            printf("This should be printed only after peer connection is over\n");

            if (send(sock_fd, "I am back now", strlen("I am back now"), 0) == -1)
            {
                perror("Could not send a msg");
            }
        // could check if this is a connect call and make a new thread for connecting to a peer
        }
        
        else {
            if (send(sock_fd, send_msg.c_str(), strlen(send_msg.c_str()), 0) == -1)
            {
                perror("Could not send a msg");
            }
        }
    }
    close(sock_fd);

    return NULL;
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

void *serve_peer(void *args) {
    int peer_fd = *(int *)args;

    int numbytes;
    char buf[MAXDATASIZE];

    bzero(&buf, sizeof buf);
    if((numbytes = recv(peer_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
        perror("Could not recv");
    }
    buf[numbytes] = 0;
    char msg[numbytes + 1];
    for(int i = 0; i <= numbytes; i++) {
        msg[i] = buf[i];
    }
    printf("client %d request: '%s'\n", peer_fd, msg);
    int item = parse_msg(msg);

    if(item == 1) {
        string reply;
        vector<string> args = get_args(msg);
        if(args.size() != 3) {
            reply = "Error: Incorrect format, correct format: get_chunk_vector <filename> <group_id>";
        }
        else {
            string group_id = args[2];
            string file_name = args[1];
            auto it = user_name_to_group_to_file_details[user_name].find(group_id);
            if(it == user_name_to_group_to_file_details[user_name].end()) {
                reply = "Error: I have not shared any file in that group";
            }
            else {
                auto it2 = user_name_to_group_to_file_details[user_name][group_id].find(file_name);
                if(it2 == user_name_to_group_to_file_details[user_name][group_id].end()) {
                    reply = "Error: I have not shared this file in this group";
                }
                else {
                    struct file_details fd = user_name_to_group_to_file_details[user_name][group_id][file_name];
                    vector<bool> chunk_bit_vector = fd.is_chunk_present;
                    for(int i = 0; i < chunk_bit_vector.size(); i++) {
                        if(chunk_bit_vector[i])
                            reply += "1";
                        else
                            reply += "0";
                    }
                }
            }
        }
        if(numbytes = send(peer_fd, reply.c_str(), strlen(reply.c_str()), 0) == -1) {
            perror("Could not send");
        }
    }
    else if(item == 2) {
        while(true) {
            char msg[numbytes + 1];
            for(int i = 0; i <= numbytes; i++) {
                msg[i] = buf[i];
            }
            string reply;
            vector<string> args = get_args(msg);
            if(args.size() != 4) {
                reply = "Error: Incorrect format, correct format: download_chunk <filename> <group_id> <chunk_no>";
            }
            else {
                string group_id = args[2];
                string file_name = args[1];
                int chunk_no = stoi(args[3]);
                if(chunk_no == -1) {
                    break;
                }
                auto it = user_name_to_group_to_file_details[user_name].find(group_id);
                if(it == user_name_to_group_to_file_details[user_name].end()) {
                    reply = "Error: I have not shared any file in that group";
                }
                else {
                    auto it2 = user_name_to_group_to_file_details[user_name][group_id].find(file_name);
                    if(it2 == user_name_to_group_to_file_details[user_name][group_id].end()) {
                        reply = "Error: I have not shared this file in this group";
                    }
                    else {
                        struct file_details fd = user_name_to_group_to_file_details[user_name][group_id][file_name];
                        long long file_size = fd.file_size;
                        string file_path = fd.filepath;
                        // printf("Fetching the chunk %d path: %s\n", chunk_no, &file_path[0]);
                        FILE *f_read = fopen(file_path.c_str(), "r");
                        
                        // char buf[CHUNK_SIZE + 1];
                        memset(buf, 0, sizeof buf);
                        fseek(f_read, chunk_no* CHUNK_SIZE, SEEK_SET);
                        size_t num_bytes_read = fread(buf, sizeof(char), CHUNK_SIZE, f_read);
                        char send_msg[num_bytes_read];
                        
                        for(int i = 0; i < num_bytes_read; i++) {
                            send_msg[i] = buf[i];
                        }
                        // printf("Have to send the msg: %s\n", send_msg);
                        reply = send_msg;
                    }
                }
            }
            if(numbytes = send(peer_fd, reply.c_str(), strlen(reply.c_str()), 0) == -1) {
                perror("Could not send");
            }
            bzero(&buf, sizeof buf);
            if((numbytes = recv(peer_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
                perror("Could not recv");
            }
            buf[numbytes] = 0;

            printf("client %d request: '%s'\n", peer_fd, msg);
            
        }
        
    }
    else if(item == 3){
        string reply;
        vector<string> args = get_args(msg);
        if(args.size() != 3) {
            reply = "Error: Incorrect format, correct format: get_file_size <filename> <group_id>";
        }
        else {
            string group_id = args[2];
            string file_name = args[1];
            auto it = user_name_to_group_to_file_details[user_name].find(group_id);
            if(it == user_name_to_group_to_file_details[user_name].end()) {
                reply = "Error: I have not shared any file in that group";
            }
            else {
                auto it2 = user_name_to_group_to_file_details[user_name][group_id].find(file_name);
                if(it2 == user_name_to_group_to_file_details[user_name][group_id].end()) {
                    reply = "Error: I have not shared this file in this group";
                }
                else {
                    struct file_details fd = user_name_to_group_to_file_details[user_name][group_id][file_name];
                    long long file_size = fd.file_size;
                    reply = to_string(file_size);
                }
            }
        }
        if(numbytes = send(peer_fd, reply.c_str(), strlen(reply.c_str()), 0) == -1) {
            perror("Could not send");
        }
    }
    else {
        //invalid command
    }

    // if((numbytes = send(peer_fd, "Ok", strlen("Ok"), 0)) == -1) {
    //     perror("could not send");
    // }

    // while((numbytes = send(peer_fd, "Welcome", strlen("Welcome"), 0)) == -1) {
    //     perror("could not send");
    //     // sleep(2);
    // }

    // while(true) {
    //     bzero(&buf, sizeof buf);
    //     if((numbytes = recv(peer_fd, buf, MAXDATASIZE - 1, 0)) == -1) {
    //         perror("Could not recv");
    //     }
    //     buf[numbytes] = 0;
    //     char msg[numbytes + 1];
    //     for(int i = 0; i <= numbytes; i++) {
    //         msg[i] = buf[i];
    //     }
    //     // msg[numbytes] = 0;
        
    //     if(strcmp(msg, "logout") == 0) {
    //         if((numbytes = send(peer_fd, "Bye", strlen("Bye"), 0)) == -1) {
    //             perror("could not send");
    //         }
    //         //should update the data structures accordingly
    //         break;
    //     }

    //     printf("client %d request: '%s'\n", peer_fd, msg);
    //     //need to take action accordingly and perform a task depending on the request
    //     if((numbytes = send(peer_fd, "Ok", strlen("Ok"), 0)) == -1) {
    //         perror("could not send");
    //     }
    // }

    close(peer_fd);
    return NULL;

}

void *make_server(void *args)
{
    struct ip_port *data = (struct ip_port *)args;
    
    // printf("Reached in the server thread\n");
    // printf("ip : %s\n", data->ip);
    // printf("port: %s\n", data->port);

    struct addrinfo hints, *server_info, *p;
    struct sockaddr_storage client_addr;
    socklen_t sockaddr_st_size = sizeof client_addr;
    char s[INET6_ADDRSTRLEN];
    int result;
    int sock_fd;
    // int client_fd;

    int yes = 1;


    char *server_ip = data->ip;
    char *server_port = data->port;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((result = getaddrinfo(server_ip, server_port, &hints, &server_info)) != 0) {
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
        int peer_fd;
        if((peer_fd = accept(sock_fd, (struct sockaddr *)&client_addr, &sockaddr_st_size)) == -1) {
            // failed to accept
            perror("server:acept");
            continue;
        }
        
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        struct sockaddr_in add;
        
        printf("server: got connection from: %s and sockfd : %d\n", s, peer_fd);

        pthread_t client;
        pthread_create(&client, NULL, serve_peer, (void *)&peer_fd);
    }

    return NULL;
}

void build_command_to_item_map() {
    command_to_item = {
        {"get_chunk_vector", 1}, 
        {"download_chunk", 2},
        {"get_file_size", 3}
        };
}

int main(int argc, char* argv[])
{
    // std::cout << "Number of arguments is " << argc << std::endl;

    if(argc != 3) {
        printf("Incorrect format, provide peer ip:port, path to tracker_info.txt\n");
        exit(1);
    }
    getcwd(home_path, sizeof(home_path));

    build_command_to_item_map();

    pthread_t client;
    struct ip_port peer_data, tracker_data;
    struct tracker_peer_details tpd;
    
    peer_data.ip = strtok(argv[1], ":");
    peer_data.port = strtok(NULL, ":");

    // std::cout << "Peer ip: " << peer_data.ip << "\n";
    // std::cout << "Peer port: " << peer_data.port << "\n";

    // peer_data.ip = peer_ip;
    // peer_data.port = peer_port;

    ifstream tracker_file(argv[2]);
    string tracker_details;
    if(tracker_file.is_open()) {
        getline(tracker_file, tracker_details);
        tracker_file.close();
    }
    else {
        printf("Could not open tracker file. Provide a valid path and permissions\n");
    }

    tracker_data.ip = strtok(&tracker_details[0], ":");
    tracker_data.port = strtok(NULL, ":");

    tpd.tracker_ip = tracker_data.ip;
    tpd.tracker_port = tracker_data.port;
    tpd.peer_ip = peer_data.ip;
    tpd.peer_port = peer_data.port;

    // std::cout << "Tracker ip: " << tracker_data.ip << "\n";
    // std::cout << "Tracker port: " << tracker_data.port << "\n";

    // tracker_data.ip = tracker_ip;
    // tracker_data.port = tracker_port;

    if (pthread_create(&client, NULL, make_client, (void* )&tpd) != 0)
    {
        perror("Could not make a client thread");
        exit(1);
    }

    pthread_t server;
    if (pthread_create(&server, NULL, make_server, (void* )&peer_data) != 0)
    {
        perror("Could not make a server thread");
        exit(1);
    }


    pthread_join(client, NULL);
    pthread_join(server, NULL);

    return 0;
}

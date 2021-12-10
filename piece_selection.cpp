#include <iostream>
#include <bits/stdc++.h>
#include <string>
#include <unistd.h>

using namespace std;

int main(int argc, char const *argv[])
{
    int num_peers = 2;
    vector<string> chunk_vectors(num_peers);
    vector<int> indices(num_peers, 0);
    int num_chunks = 5;
    vector<vector<int>> chunks_to_download(num_peers);
    vector<bool> chunk_selected(num_chunks, false);
    chunk_vectors[0] = "11111";
    chunk_vectors[1] = "11111";
    // chunk_vectors[2] = "11010";
    // chunk_vectors[3] = "10011";
    // chunk_vectors[4] = "11100";
    //
    
    int peer_no = 0;
    for(int i = 0; i < num_chunks;) {
        int index = indices[peer_no];
        while(index < num_chunks) {
            if(chunk_vectors[peer_no][index] == '0' || chunk_selected[index])
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

    for(int i = 0; i < num_peers; i++) {
        for(int j = 0; j < chunks_to_download[i].size(); j++) {
            cout << chunks_to_download[i][j] << " ";
        }
        cout << endl;
    }
    FILE *fp = fopen("test_file.txt", "w");
    ftruncate(fileno(fp), 108);
    fclose(fp);

    return 0;
}

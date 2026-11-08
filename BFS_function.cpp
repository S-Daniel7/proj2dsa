#include <string>
#include <vector>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <iostream>
#include <climits>
#include <chrono>

using namespace std;


//Bidirectional BFS: finds the shortest chain of shared movies between two actors.
// searches from both actors at once to meet in the middle



struct coStar {
    string actor;
    string movie;
};
// Maps each actor ID to all of their co-stars
typedef unordered_map<string, vector<coStar>> coStarGraph;

struct jump {
    string actor;
    string movie;
};


static string column(const string& line, int index) {
    size_t begin = 0;
    for (int i = 0; i < index; ++i) {
         begin = line.find('\t', begin);
    if (begin == string::npos) {
      return "";
  }
  begin++;
    }


    size_t end = line.find('\t', begin);
    string value = line.substr(begin, end == string::npos ? string::npos : end - begin);


    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
    return value;
}


static bool fopenMessage(ifstream& in, const string& path) {
    in.open(path);
    if (!in) {
        cerr << "Failed to open file: " << path << endl;
        return false;
    }
    return true;
}

static bool readMovies(const string& path, unordered_map<string, string>& movieName) {
    ifstream in;
    if (!fopenMessage(in, path)) {
        return false;
    }

    string line;
    getline(in, line);
    while (getline(in, line)) {
      if (column(line,1) == "movie") {
        movieName[column(line, 0)] = column(line, 2);
      }
    }

    return true;
}

                        // Builds the graph: actors in the same movie get connected
  static bool readCasts(const string& path, const unordered_map<string, string>& movieNames, coStarGraph& graph) {
    ifstream in;
    if (!fopenMessage(in, path)) {
        return false;
    }


    unordered_map<string, unordered_set<string>> casts;
    string line;
    getline(in, line);
    while (getline(in, line)) {
        string movie = column(line, 0);
      if(movieNames.count(movie)==0) {
        continue;
      }
      string job = column(line,3);
      if(job=="actor" || job=="actress") {
        casts[movie].insert(column(line, 2));


      }
    }

    for (const auto& entry : casts) {
        vector<string> people(entry.second.begin(), entry.second.end());
        for (size_t i = 0; i < people.size(); ++i) {
            graph[people[i]];
            for (size_t j = i+1; j < people.size(); ++j) {
                graph[people[i]].push_back({people[j], entry.first});
                graph[people[j]].push_back({people[i], entry.first});
            }
        }
    }
        






    return true;
}

  static bool readActorNames(const string& path, const coStarGraph& graph,
                             unordered_map<string, string>& idToName,
                             unordered_map<string, vector<string>>& nameToIds) {
    ifstream in;
    if (!fopenMessage(in,path)) {
        return false;
    }
    string line;
    getline(in, line);
    while (getline(in,line)){
        string id = column(line, 0);
        if(graph.count(id)==0){
            continue;
        }
        string name = column(line, 1);
        idToName[id] = name;
        nameToIds[name].push_back(id);
    }
    return true;
}



struct nameMapping {
    string from;
    string movie;
};  

struct searcher{
    queue <string> front;
    unordered_map<string, int> depth;
    unordered_map<string, nameMapping> parent;
};

struct meeting {
    int length = INT_MAX;
    string near;
    string far;
    string movie;
    
};
                         // Expands one full level of BFS for one side
static bool expandLayer(const coStarGraph& graph, searcher& side, const searcher& other, meeting& meet){
    bool improved = false;
    size_t layer = side.front.size();

    while (layer --> 0){
        string actor = side.front.front();
        side.front.pop();
        int d = side.depth[actor];

        auto it = graph.find(actor);
        if (it == graph.end()) {
            continue;
        }

        for (const coStar& co : it->second) {
            auto seen = other.depth.find(co.actor);
            if(seen != other.depth.end() && d + 1 + seen->second < meet.length) {
                meet.length = d + 1 + seen->second;
                meet.near = actor;
                meet.far = co.actor;
                meet.movie = co.movie;
                improved = true;
            }

            if (side.depth.count(co.actor)==0){
                side.depth[co.actor] = d + 1;
                side.parent[co.actor] = {actor, co.movie};
                side.front.push(co.actor);

            }
        }



    
    }
    return improved;
}

                        // Finds the shortest path from start to goal, returns false if they aren't connected
bool bidirectionalBFS(const coStarGraph& graph, const string& start, const string& goal, vector<jump>& jumps, size_t& actorsVisited){
    jumps.clear();
    actorsVisited = 0;
    if (!graph.count(start) || !graph.count(goal)) {
        return false;
    }

    if(start == goal){
        return true;
    }

    searcher fromStart, fromGoal;
    fromStart.front.push(start);
    fromStart.depth[start] = 0;
    fromGoal.front.push(goal);
    fromGoal.depth[goal] = 0;

    meeting meet;
    bool startSideFoundit = true;

    while(!fromStart.front.empty() && !fromGoal.front.empty()){
        bool useStart = fromStart.front.size() <= fromGoal.front.size();
        bool better = useStart
            ? expandLayer(graph, fromStart, fromGoal, meet)
            : expandLayer(graph, fromGoal, fromStart, meet);

        if (better) {
            startSideFoundit = useStart;
        }
        if (meet.length != INT_MAX) {
            break;
        }           // the searches met after a full level, so this is the shortest path
    }
      actorsVisited = fromStart.depth.size() + fromGoal.depth.size();

    if (meet.length == INT_MAX) {
        return false;
    }

    string left = startSideFoundit ? meet.near : meet.far;
    string right = startSideFoundit ? meet.far : meet.near;


    for (string cur = left; cur != start; cur = fromStart.parent[cur].from){
        jumps.push_back({cur, fromStart.parent[cur].movie});
    }
    reverse(jumps.begin(), jumps.end());
    jumps.push_back({right, meet.movie});
    for (string cur = right; cur != goal; cur = fromGoal.parent[cur].from){
        jumps.push_back({fromGoal.parent[cur].from, fromGoal.parent[cur].movie});
    }
    return true;
    
     
}
    
static string askForActor(const string& prompt, const unordered_map<string, vector<string>>&nameToIds){
    while(true){
        cout << prompt;
        string name;
        if (!getline(cin, name) || name == "quit") {
            return "";
        }
        auto found = nameToIds.find(name);
        if (found == nameToIds.end()) {
            cout << "Actor not found. Please try again.\n";
            continue;
        }
        
    



    const vector<string>& ids = found->second;
    if(ids.size() == 1) {
        return ids[0];
    }
    cout << ids.size() << " actors are named " << name << ":\n";
    for (size_t i = 0; i < ids.size(); ++i) {
        cout << i + 1 << ". " << ids[i] << "\n";
    }
    cout << "Pick a number: ";
    string pick;
    getline(cin,pick);

    try{ 
        size_t n = stoul(pick);
        if (n>=1 && n<= ids.size()) {
            return ids[n-1];
        }
        cout<<"Invalid choice. \n";

    } catch (...) {
        cout<<"Invalid choice. \n";
    }
    }
}

int main(){
    unordered_map<string, string> moviesNames;
    coStarGraph graph;
    unordered_map<string, string> idToName;
    unordered_map<string, vector<string>> nameToIds;

    cout << "Reading movies...\n";
    if(!readMovies("data/title.basics.tsv", moviesNames)){
        return 1;
    }
    cout << "Reading casts and building graph...\n";
    if(!readCasts("data/title.principals.tsv", moviesNames, graph)){
        return 1;
    }
    cout << "Reading actor names...\n";
    if(!readActorNames("data/name.basics.tsv", graph, idToName, nameToIds)){
        return 1;
    }
    cout<<"Loaded " << graph.size() << " actors.\n"<< "Type \"quit\" to exit.\n";

    auto nameOf = [&](const string& id) {
        auto it = idToName.find(id);
        return it == idToName.end() ? id : it->second;
    };

    while(true){
        cout<<"\n=== Bidirectional BFS ===\n";
        string start = askForActor("First actor: ", nameToIds);
        if (start.empty()) {
            break;
        }
        string goal = askForActor("Second actor: ", nameToIds);
        if (goal.empty()) {
            break;
        }

        vector<jump> jumps;
        size_t visited = 0;
                                // time the search to compare against IDDFS
        auto t0 = chrono::steady_clock::now();
        bool found = bidirectionalBFS(graph, start, goal, jumps, visited);
        auto t1 = chrono::steady_clock::now();
        double ms = chrono::duration<double, milli>(t1 - t0).count();
 
        if (!found) {
            cout << "These actors are not connected.\n";
        } else {
            cout << nameOf(start);
            for (const jump& h : jumps) {
                cout << " --[" << moviesNames.at(h.movie) << "]--> "
                     << nameOf(h.actor);
            }
            cout << "\nConnections: " << jumps.size() << '\n';
        }
        cout << "Actors visited: " << visited
             << " | Time: " << ms << " ms\n";
    }
    return 0;


}
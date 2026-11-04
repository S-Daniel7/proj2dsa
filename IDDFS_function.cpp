#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
using namespace std;

struct Connection {
    string actorID; // Connected actor
    string movieID; // Movie shared with said actor
};

using Graph = unordered_map<string, vector<Connection>>;



bool depthLimitedDFS(
    const Graph& graph,
    const string& current,
    const string& target,
    int remainingDepth,
    unordered_set<string>& onPath,
    vector<Connection>& path
) {
    if (current == target) {
        return true;
    }

    if (remainingDepth == 0) {
        return false;
    }

    auto it = graph.find(current);

    if (it == graph.end()) {
        return false;
    }

    for (const Connection& neighbor : it->second) {
        // Avoid doubling up on an actor that has already been used on this path. this would prevent an infinite loop.
        if (onPath.count(neighbor.actorID)) {
            continue;
        }

        onPath.insert(neighbor.actorID);
        path.push_back(neighbor);

        if (depthLimitedDFS(
                graph,
                neighbor.actorID,
                target,
                remainingDepth - 1,
                onPath,
                path
            )) {
            return true;
            }

        // This route failed, so undo it before trying another.
        path.pop_back();
        onPath.erase(neighbor.actorID);
    }

    return false;
}

bool IDDFS(
    const Graph& graph,
    const string& start,
    const string& target,
    int maxDepth,
    vector<Connection>& path
) {
    path.clear();

    if (maxDepth < 0 ||
        graph.find(start) == graph.end() ||
        graph.find(target) == graph.end()) {
        return false;
        }

    for (int limit = 0; limit <= maxDepth; limit++) {
        unordered_set<string> onPath;
        onPath.insert(start);

        if (depthLimitedDFS(
                graph,
                start,
                target,
                limit,
                onPath,
                path
            )) {
            return true;
            }
    }

    return false;
}


Graph buildGraph(
    const unordered_map<string, vector<string>>& movieCast
) {
    Graph graph;

    for (const auto& entry : movieCast) {
        const string& movieID = entry.first;
        const vector<string>& cast = entry.second;

        for (const string& actor : cast) {
            graph.try_emplace(actor);
        }

        for (size_t i = 0; i < cast.size(); i++) {
            for (size_t j = i + 1; j < cast.size(); j++) {
                // Actors are connected in both directions.
                graph[cast[i]].push_back({cast[j], movieID});
                graph[cast[j]].push_back({cast[i], movieID});
            }
        }
    }

    return graph;
}

vector<string> splitTSV(const string& line) {
    vector<string> fields;
    string field;
    stringstream stream(line);

    while (getline(stream, field, '\t')) {
        fields.push_back(field);
    }

    // Handle Windows line endings.
    if (!fields.empty() &&
        !fields.back().empty() &&
        fields.back().back() == '\r') {
        fields.back().pop_back();
        }

    return fields;
}

unordered_map<string, string> loadMovies(const string& filename) {
    ifstream file(filename);

    if (!file) {
        throw runtime_error("Cannot open: " + filename);
    }

    unordered_map<string, string> movieTitles;
    string line;

    getline(file, line); // Skip column headers.

    while (getline(file, line)) {
        vector<string> fields = splitTSV(line);

        if (fields.size() < 3) {
            continue;
        }

        // fields[0]: tconst (movie ID)
        // fields[1]: titleType
        // fields[2]: primaryTitle
        if (fields[1] == "movie") {
            movieTitles[fields[0]] = fields[2];
        }
    }

    return movieTitles;
}

unordered_map<string, vector<string>> loadMovieCast(
    const string& filename,
    const unordered_map<string, string>& movieTitles
) {
    ifstream file(filename);

    if (!file) {
        throw runtime_error("Cannot open: " + filename);
    }

    unordered_map<string, unordered_set<string>> uniqueCast;
    string line;

    getline(file, line); // Skip column headers.

    while (getline(file, line)) {
        vector<string> fields = splitTSV(line);

        if (fields.size() < 4) {
            continue;
        }

        // fields[0]: tconst (movie ID)
        // fields[2]: nconst (person ID)
        // fields[3]: category
        const string& movieID = fields[0];
        const string& actorID = fields[2];
        const string& category = fields[3];

        if (movieTitles.count(movieID) == 0) {
            continue;
        }

        if (category == "actor" || category == "actress") {
            uniqueCast[movieID].insert(actorID);
        }
    }

    unordered_map<string, vector<string>> movieCast;

    for (const auto& entry : uniqueCast) {
        movieCast[entry.first] = vector<string>(
            entry.second.begin(),
            entry.second.end()
        );
    }

    return movieCast;
}

unordered_map<string, string> loadActorNames(
    const string& filename,
    const Graph& graph
) {
    ifstream file(filename);

    if (!file) {
        throw runtime_error("Cannot open: " + filename);
    }

    unordered_map<string, string> actorNames;
    string line;

    getline(file, line); // Skip column headers.

    while (getline(file, line)) {
        vector<string> fields = splitTSV(line);

        if (fields.size() < 2) {
            continue;
        }

        // fields[0]: nconst (person ID)
        // fields[1]: primaryName
        if (graph.count(fields[0])) {
            actorNames[fields[0]] = fields[1];
        }
    }

    return actorNames;
}

string findActorID(
    const unordered_map<string, string>& actorNames,
    const string& name
) {
    vector<string> matches;

    for (const auto& actor : actorNames) {
        if (actor.second == name) {
            matches.push_back(actor.first);
        }
    }

    if (matches.empty()) {
        return "";
    }

    if (matches.size() == 1) {
        return matches[0];
    }

    cout << "Multiple actors named " << name << ":\n";

    for (size_t i = 0; i < matches.size(); i++) {
        cout << i + 1 << ". " << matches[i] << '\n';
    }

    cout << "Enter their number: ";

    string input;
    getline(cin, input);

    stringstream stream(input);
    size_t choice;
    char extra;

    if (!(stream >> choice) || (stream >> extra) ||
        choice < 1 || choice > matches.size()) {
        return "";
        }

    return matches[choice - 1];
}

int main() {
    try {
        cout << "Loading movies...\n";
        auto movieTitles = loadMovies("data/title.basics.tsv");

        cout << "Loading casts...\n";
        auto movieCast = loadMovieCast(
            "data/title.principals.tsv",
            movieTitles
        );

        cout << "Building graph...\n";
        Graph graph = buildGraph(movieCast);

        cout << "Loading actor names...\n";
        auto actorNames = loadActorNames(
            "data/name.basics.tsv",
            graph
        );

        cout << "Graph contains " << graph.size() << " actors.\n";

        // Convert an actor ID to its display name.
        auto actorLabel = [&](const string& id) -> string {
            auto it = actorNames.find(id);
            return it == actorNames.end() ? id : it->second;
        };

        // Keep asking until the user enters a valid name or stops running the program
        auto promptActor = [&](const string& prompt) -> string {
            while (true) {
                cout << prompt;

                string name;

                if (!getline(cin, name) || name == "quit") {
                    return "";
                }

                string id = findActorID(actorNames, name);

                if (!id.empty()) {
                    return id;
                }

                cout << "Actor not found or selection invalid. Try again.\n"
                     << "Use the exact spelling and capitalization.\n\n";
            }
        };

        cout << "Type \"quit\" at an actor prompt to exit.\n";

        while (true) {
            cout << "\n--- New search ---\n";

            string start = promptActor("Enter starting actor's full name: ");

            if (start.empty()) {
                break;
            }

            string target = promptActor("Enter target actor's full name: ");

            if (target.empty()) {
                break;
            }

            vector<Connection> path;

            cout << "Searching...\n";

            if (IDDFS(graph, start, target, 6, path)) {
                cout << actorLabel(start);

                for (const Connection& step : path) {
                    cout << " --[" << movieTitles.at(step.movieID)
                         << "]--> " << actorLabel(step.actorID);
                }

                cout << "\nConnections: " << path.size() << '\n';
            } else {
                cout << "No connection found within 6 steps.\n";
            }
        }

    } catch (const exception& error) {
        cerr << error.what() << '\n';
        return 1;
    }

    return 0;
}

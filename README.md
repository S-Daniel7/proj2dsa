# Actor Connection Finder

COP3530 Project 2 - Team Krillion

Members: Daniel Tozzi, Sophie Daniel, and Emma Wolman

This C++ project finds the shortest chain of shared movies between two actors.
It compares Bidirectional BFS and Iterative Deepening Depth-First Search (IDDFS).
The interface uses Qt Widgets.

## How it works

1. Enter a starting actor and a target actor.
2. Click **Run comparison**.
3. If several actors have the same name, choose one using the listed movie credits.
4. Compare the paths, number of connections, and search times.

Each actor is a vertex in the graph. An edge joins two actors who share a movie.
A path with two shared movies has two connections. The algorithms may return
different paths with the same shortest length.

BFS searches from both actors. IDDFS tries increasing depth limits, up to six
connections. If IDDFS finds nothing within six steps, a longer path may still
exist. Search times measure only the algorithm calls, excluding data loading
and display updates.

## Data

Download these files from the [IMDb datasets page](https://datasets.imdbws.com/):

- `title.basics.tsv.gz`
- `title.principals.tsv.gz`
- `name.basics.tsv.gz`

Put the three compressed files in your Downloads folder. The GUI extracts them
into `gui/build/data/` on its first use. It uses movie titles, actor/actress
entries in the principal cast data, and actor names. Results depend on the
credits included in this dataset.

The first search takes longer because it prepares the data. A saved graph in
`gui/build/data/actors.cache` speeds up later launches. Searches in the same
window reuse the loaded graphs. Build files, extracted data, and the cache are
ignored by Git.

## Build and run on macOS

Requirements: a C++17 compiler, CMake, and Qt 6. With Homebrew, install the
needed packages:

```sh
brew install qtbase cmake
```

After installation finishes, run these commands from the project folder:

```sh
cmake -S gui -B gui/build -DCMAKE_PREFIX_PATH="$(brew --prefix qtbase)"
cmake --build gui/build
./gui/build/actor_gui
```

After changing the GUI, close the old window, rebuild, and run it again.

## Files

- `BFS_function.cpp`: Bidirectional BFS and its console program.
- `IDDFS_function.cpp`: IDDFS and its console program.
- `gui/frontend.cpp`: the window, data loading, actor selection, and comparison.
- `gui/CMakeLists.txt`: build settings for the GUI.

The GUI calls the existing algorithm functions without changing their files.
Their console entry points are renamed only when included in the GUI build.

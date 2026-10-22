# Mini-Git for Datasets (C core + C++ classes)

A command-line version control tool for CSV datasets. It stores each version of a file as a small
**row-level patch** (computed with Longest Common Subsequence) instead of a full copy, and keeps the
versions in a commit graph so any version can be rebuilt.

The project is split by language on purpose:

| Folder | Language | What lives here |
|--------|----------|-----------------|
| `core/` | **C11** | All data structures and algorithms (DSA) |
| `oop/`  | **C++17** | The object-oriented layer: classes that own and use the C core |
| `tests/` | C and C++ | One test program for each layer |

## Build and run

```
make                 # builds ./minigit
make test            # runs the C tests and the C++ tests
make SAN=1 test      # same, with memory-error detection (AddressSanitizer + UBSan)
```

No `make`? Use the compilers directly (works on Windows with MinGW or in Git Bash):

```
mkdir obj
gcc -std=c11 -Wall -Wextra -Icore -c core/*.c
mv *.o obj/
g++ -std=c++17 -Wall -Wextra -Icore -Ioop oop/*.cpp obj/*.o -o minigit
```

```
./minigit init
./minigit commit students.csv -m "first version"
./minigit checkout <commit_hash> restored.csv
```

## Part 1 - Data structures and algorithms (C, `core/`)

| File | Concept | Cost |
|------|---------|------|
| `mg_strvec.c` | **Dynamic array** of rows, capacity doubles when full | push O(1) amortised |
| `mg_diff.c` (`mg_diff_lines`) | **Dynamic programming**: LCS table of size (m+1) x (n+1) | O(m*n) time and memory |
| `mg_diff.c` (backtracking) | **Backtracking** through the table to get INSERT / DELETE / KEEP edits | O(m+n) |
| `mg_diff.c` (`mg_apply_patch`) | **Patch replay** with a moving index into the base file | O(patch size) |
| `mg_hashmap.c` | **Hash table**: FNV-1a hash, separate chaining, doubles at load factor 0.75 | O(1) average |
| `mg_graph.c` | **DAG** of commits (each commit lists its parents), stored in the hash table | lookup O(1) |
| `mg_graph.c` (`mg_graph_rebuild`) | **Graph traversal**: walk from a commit to the root along first parents, then replay the patches root to commit | O(chain length + patches) |
| `mg_alloc.c` | One place for all `malloc`/`free`, so tests can simulate "out of memory" | - |

The C code uses no C++ and no library containers. Every function returns an `mg_status` code instead of
crashing, and every error path frees what it allocated.

## Part 2 - Object-oriented design (C++, `oop/`)

| Class | OOP idea |
|-------|----------|
| `Dataset`, `Patch` | **Encapsulation + RAII**: a class owns the C memory and frees it in its destructor. They follow the **rule of five** (copy/move constructors, copy/move assignment, destructor). |
| `DiffEngine`, `FileHandler` | **Abstraction**: simple static interfaces; `DiffEngine` only forwards to the C algorithms. |
| `Repository` | **Ownership and invariants**: owns the C graph, generates unique commit ids, keeps HEAD, turns C error codes into exceptions. Copying is deleted because it owns a graph. |
| `CommitView` | A read-only window onto one commit. |
| `MiniGitError`, `UsageError` | **Exceptions**: one error type for failures from the C core, one for wrong command-line usage. |
| `Command` (abstract) -> `InitCommand`, `CommitCommand`, `CheckoutCommand` | **Inheritance + polymorphism**: `main()` calls `execute()` through a `Command` pointer and the right subclass runs (virtual function). |
| `CommandFactory` | **Factory**: reads `argv` and builds the right command object. |

## How a commit and a checkout work

* `commit file.csv`: read the file, rebuild the parent's content (graph traversal), compute the patch
  (LCS), store it as a new commit in the graph, move HEAD.
* `checkout <hash> out.csv`: look the commit up in the hash table, walk back to the root, replay the
  patches on the way forward, write the file.

## Known limitations (be honest about these in the report and viva)

* **No saving to disk yet.** The commit graph lives in memory, so each program run starts empty.
  `Repository::serializeToDisk()` / `deserializeFromDisk()` are the planned next step.
  Until then a commit hash from an earlier run reports "not found".
* **LCS memory is quadratic.** Measured: 4,000 x 4,000 rows takes 0.08 s; 16,000 x 16,000 takes about
  1.5 s and 980 MB. For very large files a linear-space method (Hirschberg) would be the next step.
* **A KEEP edit is stored for every unchanged row** (without its text). The text is not duplicated,
  but the number of entries still grows with the file length.
* **Only the first parent is followed** when rebuilding. A commit can list several parents (the
  structure supports it), but merging is not implemented.

## Tests

* `tests/test_core.c` (C): dynamic array, hash table (5,000 keys, resize, remove), LCS diff checked
  against an independent LCS implementation, 3,000 random round-trips, commit graph, a deliberate cycle,
  and **simulated out-of-memory at every allocation point** (no leak, no crash).
* `tests/test_oop.cpp` (C++): copy/move behaviour, exceptions, `FileHandler`, `Repository`
  (3,000 commits, all ids unique), and every command through the polymorphic interface.

// test_oop.cpp - tests for the C++ layer (classes wrapping the C core).
#include "check.h"
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include "Commands.hpp"
#include "Dataset.hpp"
#include "DiffEngine.hpp"
#include "FileHandler.hpp"
#include "MiniGitError.hpp"
#include "Patch.hpp"
#include "Repository.hpp"

#define CHECK_THROWS(expr, ExType) do { bool caught_ = false; \
    try { expr; } catch (const ExType&) { caught_ = true; } catch (...) {} CHECK(caught_); } while (0)

static std::string readAll(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::ostringstream s; s << f.rdbuf();
    return s.str();
}
static void writeAll(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary); f << text;
}

// ---------------------------------------------------------------- Dataset / Patch

static void test_dataset_rule_of_five() {
    Dataset a{"x", "y", "z"};
    CHECK(a.size() == 3 && a.at(1) == "y" && !a.empty());

    Dataset copy(a);                                   // copy constructor: independent deep copy
    copy.push_back("extra");
    CHECK(a.size() == 3 && copy.size() == 4 && a != copy);

    Dataset assigned;
    assigned = a;                                      // copy assignment
    CHECK(assigned == a);
    assigned = assigned;                               // self-assignment is harmless
    CHECK(assigned == a);

    Dataset moved(std::move(copy));                    // move constructor: source is left empty but valid
    CHECK(moved.size() == 4 && copy.size() == 0 && copy.empty());
    copy.push_back("still usable");
    CHECK(copy.size() == 1);

    Dataset target{"old"};
    target = std::move(moved);                         // move assignment
    CHECK(target.size() == 4 && moved.size() == 0);

    CHECK(std::is_nothrow_move_constructible<Dataset>::value);
    CHECK(std::is_nothrow_move_assignable<Dataset>::value);
}

static void test_dataset_errors() {
    Dataset d{"a"};
    CHECK_THROWS(d.at(1), std::out_of_range);
    CHECK_THROWS(d.push_back(std::string("bad\0row", 7)), std::invalid_argument);
    CHECK(d.size() == 1);                              // a rejected row changes nothing
    Dataset fromVector(std::vector<std::string>{"p", "q"});
    CHECK(fromVector.toVector() == (std::vector<std::string>{"p", "q"}));
    d.push_back("");                                   // an empty row is a valid row
    CHECK(d.size() == 2 && d.at(1) == "");
}

static void test_patch_class() {
    Patch p;
    p.push_back(EditType::KEEP);
    p.push_back(EditType::INSERT, "new row");
    p.push_back(EditType::DELETE, "old row");
    CHECK(p.size() == 3 && p.typeAt(1) == EditType::INSERT && p.contentAt(1) == "new row");
    CHECK(p.contentAt(0) == "" && p.count(EditType::KEEP) == 1);
    CHECK_THROWS(p.typeAt(3), std::out_of_range);
    CHECK_THROWS(p.contentAt(9), std::out_of_range);

    Patch copy(p);
    copy.push_back(EditType::KEEP);
    CHECK(p.size() == 3 && copy.size() == 4);
    Patch moved(std::move(copy));
    CHECK(moved.size() == 4 && copy.size() == 0);
}

// ---------------------------------------------------------------- DiffEngine

static void test_diff_engine() {
    const Dataset v1{"id,name", "1,ann", "2,bob", "3,cy"};
    const Dataset v2{"id,name", "1,ann", "3,cy", "4,dee"};
    const Patch p = DiffEngine::calculateLCS(v1, v2);
    CHECK(DiffEngine::applyPatch(v1, p) == v2);
    CHECK(p.count(EditType::KEEP) == 3 && p.count(EditType::DELETE) == 1 && p.count(EditType::INSERT) == 1);

    CHECK_THROWS(DiffEngine::applyPatch(Dataset{"only one row"}, p), MiniGitError);   // patch does not fit
    try { DiffEngine::applyPatch(Dataset{"only one row"}, p); CHECK(false); }
    catch (const MiniGitError& e) { CHECK(e.status() == MG_ERR_BAD_PATCH); }

    CHECK(DiffEngine::applyPatch(Dataset(), DiffEngine::calculateLCS(Dataset(), v1)) == v1);   // empty -> v1
    CHECK(DiffEngine::applyPatch(v1, DiffEngine::calculateLCS(v1, Dataset())).empty());        // v1 -> empty
}

// ---------------------------------------------------------------- FileHandler

static void test_file_handler() {
    const std::string path = "tmp_oop_test.csv";
    const Dataset rows{"h1,h2", "a,b", "", "c,d"};
    CHECK(FileHandler::writeFile(path, rows) && FileHandler::fileExists(path));
    CHECK(readAll(path) == "h1,h2\na,b\n\nc,d\n");
    CHECK(FileHandler::readFile(path) == rows);

    writeAll(path, "x,y\r\nz,w\r\n");                  // Windows line endings are stripped
    CHECK(FileHandler::readFile(path) == (Dataset{"x,y", "z,w"}));
    writeAll(path, "no newline at end");
    CHECK(FileHandler::readFile(path) == (Dataset{"no newline at end"}));
    std::remove(path.c_str());

    CHECK(!FileHandler::fileExists("definitely_missing_file.csv"));
    CHECK(!FileHandler::writeFile("no_such_dir/out.csv", rows));
}

// ---------------------------------------------------------------- Repository

static void test_repository_chain() {
    Repository repo;
    const Dataset v1{"h", "1", "2"}, v2{"h", "1", "2", "3"}, v3{"h", "2", "3", "4"};

    CHECK(repo.getHead().empty() && repo.commitCount() == 0);
    const std::string c1 = repo.createCommit(DiffEngine::calculateLCS(Dataset(), v1), "", "first");
    const std::string c2 = repo.createCommit(DiffEngine::calculateLCS(v1, v2), c1, "second");
    const std::string c3 = repo.createCommit(DiffEngine::calculateLCS(v2, v3), c2, "third");

    CHECK(c1.size() == 8 && c1.find_first_not_of("0123456789abcdef") == std::string::npos);
    CHECK(repo.getHead() == c3 && repo.commitCount() == 3);
    CHECK(repo.checkout(c1) == v1 && repo.checkout(c2) == v2 && repo.checkout(c3) == v3);   // DAG walk

    const CommitView view = repo.getCommit(c2);
    CHECK(view.hashId() == c2 && view.message() == "second");
    CHECK(view.parentIds() == std::vector<std::string>{c1});
    CHECK(repo.getCommit(c1).parentIds().empty());
    CHECK(view.timestamp().size() == 19);              // "YYYY-MM-DD HH:MM:SS"
    CHECK(view.patchSize() == DiffEngine::calculateLCS(v1, v2).size());

    CHECK_THROWS(repo.getCommit("deadbeef"), MiniGitError);
    CHECK_THROWS(repo.checkout("deadbeef"), MiniGitError);
    try { repo.getCommit("deadbeef"); CHECK(false); }
    catch (const MiniGitError& e) { CHECK(e.status() == MG_ERR_NOT_FOUND); }

    CHECK_THROWS(repo.createCommit(Patch(), "no-such-parent", "x"), MiniGitError);   // unknown parent rejected
    CHECK(repo.commitCount() == 3 && repo.getHead() == c3);                          // ...and nothing changed

    repo.init();                                       // init forgets everything
    CHECK(repo.commitCount() == 0 && repo.getHead().empty() && !repo.hasCommit(c1));
}

static void test_repository_unique_ids() {
    Repository repo;
    std::set<std::string> ids;
    std::string parent;
    for (int i = 0; i < 3000; ++i) {
        parent = repo.createCommit(Patch(), parent, "c" + std::to_string(i));
        ids.insert(parent);
    }
    CHECK(ids.size() == 3000 && repo.commitCount() == 3000);     // no id was ever reused
}

// ---------------------------------------------------------------- Commands (inheritance + polymorphism)

static int run(Repository& repo, std::vector<std::string> args, std::string& out, std::string& err) {
    std::vector<const char*> argv{"minigit"};
    for (const auto& a : args) argv.push_back(a.c_str());
    std::ostringstream o, e;
    int code;
    try {
        std::unique_ptr<Command> cmd = CommandFactory::create(static_cast<int>(argv.size()), argv.data());
        code = cmd->execute(repo, o, e);               // virtual call: the right subclass runs
    } catch (const UsageError& ex) { e << ex.what() << "\n"; code = 1; }
      catch (const std::exception& ex) { e << "Error: " << ex.what() << "\n"; code = 1; }
    out = o.str(); err = e.str();
    return code;
}

static void test_commands() {
    Repository repo;
    std::string out, err;

    CHECK(run(repo, {"init"}, out, err) == 0 && out == "Initialized empty dataset repository.\n");

    writeAll("tmp_v1.csv", "id,name\n1,ann\n2,bob\n");
    CHECK(run(repo, {"commit", "tmp_v1.csv", "-m", "first version"}, out, err) == 0);
    CHECK(out.find("Successfully committed! Hash: ") == 0 && err.empty());
    const std::string h1 = repo.getHead();
    CHECK(repo.getCommit(h1).message() == "first version");

    writeAll("tmp_v2.csv", "id,name\n1,ann\n3,cy\n");
    CHECK(run(repo, {"commit", "tmp_v2.csv", "-m", "second version"}, out, err) == 0);
    const std::string h2 = repo.getHead();
    CHECK(h1 != h2 && repo.getCommit(h2).parentIds() == std::vector<std::string>{h1});
    CHECK(out.find("Saved 4 changes") != std::string::npos);   // KEEP header, KEEP ann, DELETE bob, INSERT cy

    CHECK(run(repo, {"checkout", h1, "tmp_out1.csv"}, out, err) == 0);
    CHECK(readAll("tmp_out1.csv") == "id,name\n1,ann\n2,bob\n");
    CHECK(run(repo, {"checkout", h2, "tmp_out2.csv"}, out, err) == 0);
    CHECK(readAll("tmp_out2.csv") == "id,name\n1,ann\n3,cy\n");
    CHECK(out.find("Time-travel rollback successful!") == 0);

    // default message, and error paths (exit code 1, a clean message, no crash)
    CHECK(run(repo, {"commit", "tmp_v2.csv", "-m"}, out, err) == 0 && repo.getCommit(repo.getHead()).message() == "Auto-commit");
    CHECK(run(repo, {"commit", "tmp_v2.csv"}, out, err) == 1 && err.find("Missing arguments") != std::string::npos);   // same rule as the original CLI
    CHECK(run(repo, {"commit", "missing.csv", "-m", "x"}, out, err) == 1 && err.find("File does not exist") != std::string::npos);
    CHECK(run(repo, {"checkout", "deadbeef", "tmp_never.csv"}, out, err) == 1 && err.find("not found") != std::string::npos);
    CHECK(!FileHandler::fileExists("tmp_never.csv"));
    CHECK(run(repo, {"commit"}, out, err) == 1 && err.find("Missing arguments") != std::string::npos);
    CHECK(run(repo, {"checkout", "abc"}, out, err) == 1 && err.find("Missing arguments") != std::string::npos);
    CHECK(run(repo, {"frobnicate"}, out, err) == 1 && err.find("Unknown command: frobnicate") != std::string::npos);
    CHECK(run(repo, {}, out, err) == 1 && err.find("No command provided") != std::string::npos);

    for (const char* f : {"tmp_v1.csv", "tmp_v2.csv", "tmp_out1.csv", "tmp_out2.csv"}) std::remove(f);
}

static void test_usage_error_flags() {
    const char* none[] = {"minigit"};
    try { CommandFactory::create(1, none); CHECK(false); }
    catch (const UsageError& e) { CHECK(e.showHelp()); }
    const char* partial[] = {"minigit", "commit", "f.csv"};
    try { CommandFactory::create(3, partial); CHECK(false); }
    catch (const UsageError& e) { CHECK(!e.showHelp()); }
    CHECK(CommandFactory::helpText().find("minigit checkout") != std::string::npos);
}

int main() {
    std::printf("C++ (OOP layer) tests\n");
    RUN(test_dataset_rule_of_five);
    RUN(test_dataset_errors);
    RUN(test_patch_class);
    RUN(test_diff_engine);
    RUN(test_file_handler);
    RUN(test_repository_chain);
    RUN(test_repository_unique_ids);
    RUN(test_commands);
    RUN(test_usage_error_flags);
    return SUMMARY();
}

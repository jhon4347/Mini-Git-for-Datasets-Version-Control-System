#include "Commands.hpp"
#include <ostream>
#include "DiffEngine.hpp"
#include "FileHandler.hpp"

int InitCommand::execute(Repository& repo, std::ostream& out, std::ostream&) const {
    repo.init();
    out << "Initialized empty dataset repository.\n";
    return 0;
}

int CommitCommand::execute(Repository& repo, std::ostream& out, std::ostream& err) const {
    if (!FileHandler::fileExists(filepath_)) {
        err << "Error: File does not exist -> " << filepath_ << "\n";
        return 1;
    }

    const Dataset newData = FileHandler::readFile(filepath_);

    // Diff against the parent's content: walk the DAG from HEAD back to the root and replay (C core).
    const std::string parentHash = repo.getHead();
    const Dataset baseData = parentHash.empty() ? Dataset() : repo.checkout(parentHash);

    const Patch deltas = DiffEngine::calculateLCS(baseData, newData);
    const std::string newHash = repo.createCommit(deltas, parentHash, message_);

    out << "Successfully committed! Hash: " << newHash << "\n";
    out << "Saved " << deltas.size() << " changes as deltas instead of full duplicate file.\n";
    return 0;
}

int CheckoutCommand::execute(Repository& repo, std::ostream& out, std::ostream& err) const {
    (void)err;
    const Dataset rebuilt = repo.checkout(hash_);          // throws MiniGitError if the hash is unknown
    if (!FileHandler::writeFile(outputPath_, rebuilt)) return 1;
    out << "Time-travel rollback successful! File rebuilt at: " << outputPath_ << "\n";
    return 0;
}

std::string CommandFactory::helpText() {
    return "Mini-Git for Datasets - Command Line Interface\n"
           "Usage:\n"
           "  minigit init\n"
           "  minigit commit <filepath> -m \"Commit message\"\n"
           "  minigit checkout <commit_hash> <output_filepath>\n";
}

std::unique_ptr<Command> CommandFactory::create(int argc, const char* const argv[]) {
    if (argc < 2) throw UsageError("Error: No command provided.", true);

    const std::string name = argv[1];
    if (name == "init") return std::make_unique<InitCommand>();

    if (name == "commit") {
        if (argc < 4) throw UsageError("Error: Missing arguments. Usage: minigit commit <file.csv> -m \"message\"", false);
        const std::string message = (argc >= 5 && std::string(argv[3]) == "-m") ? argv[4] : "Auto-commit";
        return std::make_unique<CommitCommand>(argv[2], message);
    }

    if (name == "checkout") {
        if (argc < 4) throw UsageError("Error: Missing arguments. Usage: minigit checkout <hash> <output.csv>", false);
        return std::make_unique<CheckoutCommand>(argv[2], argv[3]);
    }

    throw UsageError("Unknown command: " + name, true);
}

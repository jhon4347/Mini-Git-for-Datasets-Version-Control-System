// main.cpp - entry point. Builds a Command from the arguments and runs it (polymorphism).
#include <iostream>
#include <memory>
#include "Commands.hpp"
#include "MiniGitError.hpp"
#include "Repository.hpp"

int main(int argc, char* argv[]) {
    try {
        std::unique_ptr<Command> command = CommandFactory::create(argc, argv);

        Repository repo;
        // Phase 3: call repo.deserializeFromDisk() here to load the graph from a .minigit folder.

        const int exitCode = command->execute(repo, std::cout, std::cerr);

        // Phase 3: call repo.serializeToDisk() here to save the graph before exiting.
        return exitCode;
    } catch (const UsageError& e) {
        std::cerr << e.what() << "\n";
        if (e.showHelp()) std::cout << CommandFactory::helpText();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

// Commands.hpp - the command-line commands as a small class hierarchy.
//
// OOP concepts: abstraction (Command is an abstract interface), inheritance (each command derives
// from it), polymorphism (main() runs any command through a Command pointer: a virtual call),
// encapsulation (each command keeps its own arguments), and a factory that builds the right object.
#pragma once
#include <iosfwd>
#include <memory>
#include <stdexcept>
#include <string>
#include "Repository.hpp"

// Thrown for wrong command-line usage. showHelp tells main() whether to print the help text too.
class UsageError : public std::invalid_argument {
public:
    UsageError(const std::string& message, bool showHelp)
        : std::invalid_argument(message), showHelp_(showHelp) {}
    bool showHelp() const noexcept { return showHelp_; }

private:
    bool showHelp_;
};

class Command {                                           // abstract base class
public:
    virtual ~Command() = default;
    // Runs the command. Returns the process exit code (0 = success).
    virtual int execute(Repository& repo, std::ostream& out, std::ostream& err) const = 0;
};

class InitCommand : public Command {
public:
    int execute(Repository& repo, std::ostream& out, std::ostream& err) const override;
};

class CommitCommand : public Command {
public:
    CommitCommand(std::string filepath, std::string message)
        : filepath_(std::move(filepath)), message_(std::move(message)) {}
    int execute(Repository& repo, std::ostream& out, std::ostream& err) const override;

private:
    std::string filepath_;
    std::string message_;
};

class CheckoutCommand : public Command {
public:
    CheckoutCommand(std::string hash, std::string outputPath)
        : hash_(std::move(hash)), outputPath_(std::move(outputPath)) {}
    int execute(Repository& repo, std::ostream& out, std::ostream& err) const override;

private:
    std::string hash_;
    std::string outputPath_;
};

class CommandFactory {
public:
    // Reads argv and builds the matching command. Throws UsageError on bad input.
    static std::unique_ptr<Command> create(int argc, const char* const argv[]);
    static std::string helpText();
};

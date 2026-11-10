#include "../../GameDiagnostics.h"

#include <iostream>
#include <vector>

namespace
{
    std::vector<std::string> messages;

    void Check(bool condition, const char* message)
    {
        if (!condition) { throw std::runtime_error(message); }
    }

    void Capture(const char* message) { messages.emplace_back(message); }

    int RecoverValue(int& evaluations, int& fallbacks)
    {
        GAME_ERROR_AND_RETURN_VALUE(
            std::out_of_range((++evaluations, "Recoverable value test")), ++fallbacks);
    }

    void RecoverVoid(bool& continued)
    {
        GAME_ERROR_AND_RETURN(std::invalid_argument("Recoverable void test"));
        continued = true;
    }
}

int main()
{
    try
    {
        using namespace Desperabis::Diagnostics;
        SetSink(Capture);
        Check(ReportOnce("deduplication", "first") && !ReportOnce("deduplication", "first") &&
            ReportOnce("deduplication", "second") && ReportOnce("different context", "first") &&
            messages.size() == 3, "Diagnostics must retain distinct failures and suppress exact repeats.");
        int evaluations = 0, fallbacks = 0;
        bool valueThrew = false, voidThrew = false, continued = false;
        int result = 0;
        try { result = RecoverValue(evaluations, fallbacks); }
        catch (const std::out_of_range&) { valueThrew = true; }
        try { RecoverVoid(continued); }
        catch (const std::invalid_argument&) { voidThrew = true; }
#ifndef NDEBUG
        Check(valueThrew && voidThrew && evaluations == 1 && fallbacks == 0 && result == 0 &&
            !continued && messages.size() == 3,
            "Debug must preserve exception types without evaluating Release return values or continuing.");
#else
        Check(!valueThrew && !voidThrew && evaluations == 1 && fallbacks == 1 && result == 1 &&
            !continued && messages.size() == 5,
            "Release must evaluate diagnostics once, log and return explicitly without executing invalid code.");
        RecoverValue(evaluations, fallbacks);
        RecoverVoid(continued);
        Check(evaluations == 2 && fallbacks == 2 && !continued && messages.size() == 5,
            "Release recovery must continue working while repeated diagnostics remain deduplicated.");
#endif
        bool rejectedSink = false;
        try { SetSink(nullptr); }
        catch (const std::invalid_argument&) { rejectedSink = true; }
        Check(rejectedSink, "Invalid diagnostic infrastructure must be rejected in every configuration.");
        for (int index = 0; index < 300; ++index) { ReportOnce("bounded diagnostics", std::to_string(index)); }
        Check(messages.size() == 257 && messages.back().find("limit reached") != std::string::npos,
            "Diagnostic storage and output must be bounded and report suppression once.");
        std::cout << "PASS: Debug/Release recovery, typed errors, explicit returns, single evaluation and bounded diagnostics.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

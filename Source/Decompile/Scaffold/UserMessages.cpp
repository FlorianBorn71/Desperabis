#include "GameContext.h"
#include "GameMath.h"

#include <algorithm>
#include <stdexcept>

namespace Desperabis
{
    void GameContext::PromptUserMessage(std::string_view text, std::int16_t duration)
    {
        auto length = std::min<std::size_t>(text.size(), 150);
        // Preserve the original byte limit without cutting a UTF-8 character in half.
        while (length > 0 && length < text.size() &&
            (static_cast<unsigned char>(text[length]) & 0xc0u) == 0x80u)
        {
            --length;
        }
        userMessage = text.substr(0, length);
        userMessageTicks = text.empty() ? 0 :
            duration < 0 ? WrapSignedWord(-static_cast<std::int32_t>(duration)) : duration;
    }

    void GameContext::UpdateUserMessage(std::int16_t ticks)
    {
        if (ticks < 0) { throw std::invalid_argument("Message updates require nonnegative ticks."); }
        if (userMessageTicks > 0) { userMessageTicks = WrapSignedWord(userMessageTicks - ticks); }
    }
}

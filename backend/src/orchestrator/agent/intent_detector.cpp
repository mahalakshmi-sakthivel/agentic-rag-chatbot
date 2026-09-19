#include "intent_detector.h"
#include <algorithm>
#include <cctype>

namespace orchestrator {

IntentType IntentDetector::analyze(const std::string& query) const {
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    if (lower_query.empty()) {
        return IntentType::CLARIFICATION_REQUIRED;
    }

    if (lower_query.find("calculate") != std::string::npos ||
        lower_query.find("how much is") != std::string::npos ||
        (lower_query.find("times") != std::string::npos && lower_query.find("revenue") == std::string::npos)) {
        return IntentType::CALCULATION;
    }

    if (lower_query.find("highest") != std::string::npos ||
        lower_query.find("maximum") != std::string::npos ||
        lower_query.find("sort") != std::string::npos) {
        return IntentType::STRUCTURED_DATA_QUERY;
    }
    
    if (lower_query.find("compare") != std::string::npos) {
        return IntentType::COMPARISON;
    }
    
    if (lower_query.find("growth from") != std::string::npos ||
        (lower_query.find("and") != std::string::npos && lower_query.find("difference") != std::string::npos)) {
        return IntentType::MULTI_STEP;
    }

    if (lower_query == "what was the revenue?") {
        return IntentType::CLARIFICATION_REQUIRED;
    }

    if (lower_query.find("hello") != std::string::npos ||
        lower_query.find("hi") != std::string::npos) {
        return IntentType::NO_RETRIEVAL_REQUIRED;
    }

    return IntentType::DOCUMENT_LOOKUP;
}

} // namespace orchestrator

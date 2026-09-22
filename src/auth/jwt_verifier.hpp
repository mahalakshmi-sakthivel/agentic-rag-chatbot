#pragma once

#include <drogon/HttpRequest.h>
#include "../common/identity.h"
#include "token_service.hpp"
#include "session_manager.hpp"
#include <memory>

namespace auth {

class JwtVerifier {
public:
    JwtVerifier(std::shared_ptr<TokenService> token_service, std::shared_ptr<SessionManager> session_manager);
    
    // Conforms to common::TokenVerifierFn signature
    common::IdentityContext verify(const drogon::HttpRequestPtr& req) const;

private:
    std::shared_ptr<TokenService> token_service_;
    std::shared_ptr<SessionManager> session_manager_;
};

} // namespace auth

#include "auth_controller.hpp"
#include "../api/controllers/response_helpers.h"
#include <iostream>
#include <nlohmann/json.hpp>

namespace auth {

std::shared_ptr<AuthService> AuthController::auth_service_;
std::shared_ptr<SessionManager> AuthController::session_manager_;

void AuthController::init(std::shared_ptr<AuthService> auth_svc,
                          std::shared_ptr<SessionManager> session_mgr) {
  auth_service_ = std::move(auth_svc);
  session_manager_ = std::move(session_mgr);
}

void AuthController::login(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  try {
    auto req_body = nlohmann::json::parse(req->getBody());

    LoginRequest login_req;
    login_req.email = req_body.value("email", "");
    login_req.password = req_body.value("password", "");
    login_req.tenant_id = req_body.value("tenant_id", "");

    auto result = auth_service_->login(login_req);

    if (!result.success) {
      callback(api::errorResponse(common::ErrorCode::UNAUTHENTICATED,
                                  result.message));
      return;
    }

    // Login returns BOTH access token and refresh token.
    nlohmann::json data = {{"access_token", result.access_token},
                           {"refresh_token", result.refresh_token},
                           {"token_type", result.token_type},
                           {"expires_in", result.expires_in}};

    callback(api::successResponse(data));

  } catch (const std::exception &e) {
    callback(api::errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Invalid JSON payload"));
  }
}

void AuthController::refresh(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  try {
    auto req_body = nlohmann::json::parse(req->getBody());

    // Refresh endpoint must receive the REFRESH TOKEN,
    // not the normal access token.
    if (!req_body.contains("refresh_token") ||
        !req_body["refresh_token"].is_string()) {

      callback(api::errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                  "Missing refresh_token"));

      return;
    }

    const std::string refresh_token =
        req_body["refresh_token"].get<std::string>();

    if (refresh_token.empty()) {
      callback(api::errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                  "Missing refresh_token"));

      return;
    }

    // AuthService validates the refresh token and generates
    // a new access token.
    auto result = auth_service_->refresh_token(refresh_token);

    if (!result.success) {
      callback(api::errorResponse(common::ErrorCode::UNAUTHENTICATED,
                                  result.message));

      return;
    }

    nlohmann::json data = {{"access_token", result.access_token},
                           {"token_type", result.token_type},
                           {"expires_in", result.expires_in}};

    callback(api::successResponse(data));

  } catch (const std::exception &e) {
    callback(api::errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Invalid JSON payload"));
  }
}

void AuthController::logout(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {

  auto identity = common::authenticate(req);

  if (!identity.authenticated) {
    callback(api::errorResponse(common::ErrorCode::UNAUTHENTICATED,
                                "Missing or invalid token"));
    return;
  }

  bool success = auth_service_->logout(identity);

  if (success) {
    callback(api::successResponse(nlohmann::json::object()));
  } else {
    callback(
        api::errorResponse(common::ErrorCode::INTERNAL_ERROR, "Logout failed"));
  }
}

} // namespace auth
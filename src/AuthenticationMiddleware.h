#ifndef AUTHENTICATION_MIDDLEWARE_H // Запобіжник від подвійного включення
#define AUTHENTICATION_MIDDLEWARE_H

#include <ESPAsyncWebServer.h>

class AuthenticationMiddleware : public AsyncMiddleware {
public:
  void setUsername(const char *username);
  void setPassword(const char *password);
  void setPasswordHash(const char *hash);

  void setRealm(const char *realm) {
    _realm = realm;
  }
  void setAuthFailureMessage(const char *message) {
    _authFailMsg = message;
  }

  // set the authentication method to use
  // default is AUTH_NONE: no authentication required
  // AUTH_BASIC: basic authentication
  // AUTH_DIGEST: digest authentication
  // AUTH_BEARER: bearer token authentication
  // AUTH_OTHER: other authentication method
  // AUTH_DENIED: always return 401 Unauthorized
  // if a method is set but no username or password is set, authentication will be ignored
  void setAuthType(AsyncAuthType authMethod) {
    _authMethod = authMethod;
  }

  // Initialize authentication with all parameters
  void begin(
    AsyncAuthType authType,
    const char* realm,
    const char* username,
    const char* password,
    const char* failMessage
  );

  // Initialize authentication using values from secrets.h
  void begin();

  // precompute and store the hash value based on the username, password, realm.
  // can be used for DIGEST and BASIC to avoid recomputing the hash for each request.
  // returns true if the hash was successfully generated and replaced
  bool generateHash();

  // returns true if the username and password (or hash) are set
  bool hasCredentials() const {
    return _hasCreds;
  }


  bool allowed(AsyncWebServerRequest *request) const;

  void run(AsyncWebServerRequest *request, ArMiddlewareNext next);

private:
  String _username;
  String _credentials;
  bool _hash = false;

  String _realm = asyncsrv::T_LOGIN_REQ;
  AsyncAuthType _authMethod = AsyncAuthType::AUTH_NONE;
  String _authFailMsg;
  bool _hasCreds = false;
};

#endif // AUTHENTICATION_MIDDLEWARE_H
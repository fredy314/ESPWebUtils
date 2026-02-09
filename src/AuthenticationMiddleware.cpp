#include "AuthenticationMiddleware.h"
#include <WebAuthentication.h>
#include "secrets.h"

void AuthenticationMiddleware::begin(
  AsyncAuthType authType,
  const char* realm,
  const char* username,
  const char* password,
  const char* failMessage
) {
  setAuthType(authType);
  setRealm(realm);
  setUsername(username);
  setPassword(password);
  setAuthFailureMessage(failMessage);
  generateHash();
}

void AuthenticationMiddleware::begin() {
  begin(
    AsyncAuthType::AUTH_DIGEST,
    AUTH_REALM,      // Використовуємо макроси напряму
    AUTH_USERNAME,
    AUTH_PASSWORD,
    AUTH_FAIL_MESSAGE
  );
}

void AuthenticationMiddleware::setUsername(const char *username) {
  _username = username;
  _hasCreds = _username.length() && _credentials.length();
}

void AuthenticationMiddleware::setPassword(const char *password) {
  _credentials = password;
  _hash = false;
  _hasCreds = _username.length() && _credentials.length();
}

void AuthenticationMiddleware::setPasswordHash(const char *hash) {
  _credentials = hash;
  _hash = _credentials.length();
  _hasCreds = _username.length() && _credentials.length();
}

bool AuthenticationMiddleware::generateHash() {
  // ensure we have all the necessary data
  if (!_hasCreds) {
    return false;
  }

  // if we already have a hash, do nothing
  if (_hash) {
    return false;
  }

  switch (_authMethod) {
    case AsyncAuthType::AUTH_DIGEST:
      _credentials = generateDigestHash(_username.c_str(), _credentials.c_str(), _realm.c_str());
      if (_credentials.length()) {
        _hash = true;
        return true;
      } else {
        return false;
      }

    case AsyncAuthType::AUTH_BASIC:
      _credentials = generateBasicHash(_username.c_str(), _credentials.c_str());
      if (_credentials.length()) {
        _hash = true;
        return true;
      } else {
        return false;
      }

    default: return false;
  }
}

bool AuthenticationMiddleware::allowed(AsyncWebServerRequest *request) const {
  // --- CUSTOM LOGIC: Only protect updates ---
  String url = request->url();
  if (!url.startsWith("/update")) {
      return true; // Allow everything else
  }
  // ------------------------------------------

  if (_authMethod == AsyncAuthType::AUTH_NONE) {
    return true;
  }

  if (_authMethod == AsyncAuthType::AUTH_DENIED) {
    return false;
  }

  if (!_hasCreds) {
    return true;
  }

  return request->authenticate(_username.c_str(), _credentials.c_str(), _realm.c_str(), _hash);
}

void AuthenticationMiddleware::run(AsyncWebServerRequest *request, ArMiddlewareNext next) {
  return allowed(request) ? next() : request->requestAuthentication(_authMethod, _realm.c_str(), _authFailMsg.c_str());
}

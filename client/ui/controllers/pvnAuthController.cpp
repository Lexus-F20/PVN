#include "pvnAuthController.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSettings>
#include <QSysInfo>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QDateTime>

#ifdef Q_OS_ANDROID
    #include "platforms/android/android_controller.h"
#endif

namespace {
// Worker base URL — change here if backend moves
constexpr auto kBackendBase = "https://pvn-backend.alexromanov765.workers.dev";

// Refresh the session in the background when the JWT has less than this long
// left before it expires. Backend issues 30-day tokens; refreshing at 7-days-left
// gives the client a large safety window without hammering the endpoint on
// every launch of a fresh session.
constexpr qint64 kRefreshMarginSeconds = 7 * 24 * 60 * 60;

// One of the three Google OAuth client IDs registered for the PVN project.
// Desktop client (used on Windows/macOS/Linux).
constexpr auto kDesktopClientId =
    "360393448931-g3b1mh8m7pqts3l525kfuib90n6k2bua.apps.googleusercontent.com";

// Android reuses the iOS OAuth client ID — the Android client type in Google
// Console is reserved for the native Google Sign-In SDK and rejects browser
// flows. The iOS client ID supports browser-based PKCE with a reversed-domain
// redirect URI, which works on Android via a custom-scheme intent filter.
constexpr auto kIosClientId =
    "360393448931-4c6t5jtu97qoqk2g77cdoj3ofop4h4lf.apps.googleusercontent.com";
constexpr auto kIosRedirectUri =
    "com.googleusercontent.apps.360393448931-4c6t5jtu97qoqk2g77cdoj3ofop4h4lf:/oauth2redirect";

QString activeClientId() {
#ifdef Q_OS_ANDROID
    return QString::fromLatin1(kIosClientId);
#elif defined(Q_OS_IOS)
    return QString::fromLatin1(kIosClientId);
#else
    return QString::fromLatin1(kDesktopClientId);
#endif
}
}

PvnAuthController::PvnAuthController(ImportController *importController,
                                     ConnectionController *connectionController,
                                     QObject *parent)
    : QObject(parent),
      m_importController(importController),
      m_connectionController(connectionController)
{
    connect(&m_callbackServer, &QTcpServer::newConnection,
            this, &PvnAuthController::onCallbackConnection);

#ifdef Q_OS_ANDROID
    // Catch the OAuth code that the Android Activity receives via custom-scheme
    // intent and forwards through QtAndroidController.onGoogleSignInCode JNI.
    connect(AndroidController::instance(), &AndroidController::googleSignInCode,
            this, [this](const QString &code) {
        if (m_state != State::Opening) return;
        handleCallback(code, m_oauthState);
    });
#endif

    loadPersistedSession();
    maybeRefreshSession();
}

void PvnAuthController::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void PvnAuthController::fail(const QString &msg)
{
    m_errorMessage = msg;
    setState(State::Failed);
    emit signInFailed(msg);
    if (m_callbackServer.isListening()) m_callbackServer.close();
}

QString PvnAuthController::randomBase64Url(int bytes)
{
    QByteArray buf(bytes, 0);
    auto *rng = QRandomGenerator::securelySeeded().system();
    for (int i = 0; i < bytes; ++i) {
        buf[i] = static_cast<char>(rng->bounded(256));
    }
    return QString::fromLatin1(buf.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString PvnAuthController::sha256Base64Url(const QString &input)
{
    const QByteArray hash =
        QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

qint64 PvnAuthController::jwtExpiryFromToken(const QString &jwt)
{
    if (jwt.isEmpty()) return 0;
    const auto parts = jwt.split('.');
    if (parts.size() != 3) return 0;
    QByteArray payload = parts[1].toUtf8();
    // base64url → base64 with padding
    payload.replace('-', '+').replace('_', '/');
    while (payload.size() % 4) payload.append('=');
    const auto doc = QJsonDocument::fromJson(QByteArray::fromBase64(payload));
    if (!doc.isObject()) return 0;
    return static_cast<qint64>(doc.object().value("exp").toDouble());
}

QString PvnAuthController::deviceId() const
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    QString id = s.value(QStringLiteral("pvn/deviceId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue(QStringLiteral("pvn/deviceId"), id);
    }
    return id;
}

bool PvnAuthController::startLocalCallbackServer()
{
    if (m_callbackServer.isListening()) m_callbackServer.close();
    if (!m_callbackServer.listen(QHostAddress::LocalHost, 0)) {
        return false;
    }
    m_callbackPort = m_callbackServer.serverPort();
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(m_callbackPort);
    return true;
}

void PvnAuthController::signInWithGoogle()
{
    m_errorMessage.clear();

#ifdef Q_OS_ANDROID
    // Android — no local server, browser will redirect to our custom URL scheme
    // and the Activity will forward the code via QtAndroidController.
    m_redirectUri = QString::fromLatin1(kIosRedirectUri);
#else
    if (!startLocalCallbackServer()) {
        fail(tr("Failed to start local callback server"));
        return;
    }
#endif

    m_pkceVerifier = randomBase64Url(32);
    const QString challenge = sha256Base64Url(m_pkceVerifier);
    m_oauthState = randomBase64Url(16);

    QUrl authUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery q;
    q.addQueryItem("response_type", "code");
    q.addQueryItem("client_id", activeClientId());
    q.addQueryItem("redirect_uri", m_redirectUri);
    q.addQueryItem("scope", "openid email profile");
    q.addQueryItem("state", m_oauthState);
    q.addQueryItem("code_challenge", challenge);
    q.addQueryItem("code_challenge_method", "S256");
    q.addQueryItem("access_type", "online");
    q.addQueryItem("prompt", "select_account");
    authUrl.setQuery(q);

    setState(State::Opening);
    if (!QDesktopServices::openUrl(authUrl)) {
        fail(tr("Could not open browser"));
    }
}

void PvnAuthController::onCallbackConnection()
{
    QTcpSocket *sock = m_callbackServer.nextPendingConnection();
    if (!sock) return;
    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        const QByteArray req = sock->readAll();
        // Parse the GET line: "GET /callback?code=...&state=... HTTP/1.1"
        const int eol = req.indexOf("\r\n");
        const QByteArray line = req.left(eol > 0 ? eol : req.size());
        const QList<QByteArray> parts = line.split(' ');
        QString code, state;
        if (parts.size() >= 2) {
            const QUrl u("http://localhost" + QString::fromLatin1(parts.at(1)));
            const QUrlQuery q(u);
            code  = q.queryItemValue("code");
            state = q.queryItemValue("state");
        }

        // Always reply with a friendly HTML page
        const QByteArray body =
            "<!doctype html><html><body style='font-family:sans-serif;background:#0A0E1A;color:#E6ECFF;text-align:center;padding-top:80px'>"
            "<h2>PVN sign-in complete</h2>"
            "<p>You can close this tab and return to the app.</p>"
            "</body></html>";
        QByteArray resp =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
            "Connection: close\r\n\r\n" + body;
        sock->write(resp);
        sock->flush();
        sock->disconnectFromHost();

        handleCallback(code, state);
    });
    connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
}

void PvnAuthController::handleCallback(const QString &code, const QString &returnedState)
{
    if (m_callbackServer.isListening()) m_callbackServer.close();
    if (returnedState != m_oauthState) {
        fail(tr("OAuth state mismatch"));
        return;
    }
    if (code.isEmpty()) {
        fail(tr("Empty authorization code"));
        return;
    }
    exchangeCodeForIdToken(code);
}

void PvnAuthController::exchangeCodeForIdToken(const QString &code)
{
    setState(State::ExchangingCode);

    QNetworkRequest req(QUrl("https://oauth2.googleapis.com/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    QUrlQuery body;
    body.addQueryItem("grant_type",    "authorization_code");
    body.addQueryItem("client_id",     activeClientId());
    body.addQueryItem("redirect_uri",  m_redirectUri);
    body.addQueryItem("code",          code);
    body.addQueryItem("code_verifier", m_pkceVerifier);

    auto *reply = m_net.post(req, body.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Token exchange failed: %1").arg(reply->errorString()));
            return;
        }
        const auto data = reply->readAll();
        const auto doc  = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            fail(tr("Token exchange returned invalid JSON"));
            return;
        }
        const QString idToken = doc.object().value("id_token").toString();
        if (idToken.isEmpty()) {
            fail(tr("No id_token in Google response"));
            return;
        }
        callBackendAuth(idToken);
    });
}

void PvnAuthController::callBackendAuth(const QString &idToken)
{
    QJsonObject body;
    body["idToken"]    = idToken;
    body["deviceId"]   = deviceId();
    body["deviceName"] = QSysInfo::machineHostName().left(40);
    postBackendAuth(QStringLiteral("/auth/google"), body);
}

void PvnAuthController::signInWithEmail(const QString &email, const QString &password)
{
    m_errorMessage.clear();
    QJsonObject body;
    body["email"]      = email.trimmed().toLower();
    body["password"]   = password;
    body["deviceId"]   = deviceId();
    body["deviceName"] = QSysInfo::machineHostName().left(40);
    postBackendAuth(QStringLiteral("/auth/login"), body);
}

void PvnAuthController::registerWithEmail(const QString &email, const QString &password, const QString &name)
{
    m_errorMessage.clear();
    QJsonObject body;
    body["email"]      = email.trimmed().toLower();
    body["password"]   = password;
    body["name"]       = name;
    body["deviceId"]   = deviceId();
    body["deviceName"] = QSysInfo::machineHostName().left(40);
    postBackendAuth(QStringLiteral("/auth/register"), body);
}

void PvnAuthController::postBackendAuth(const QString &endpoint, const QJsonObject &body)
{
    setState(State::CallingBackend);

    QNetworkRequest req(QUrl(QString::fromLatin1(kBackendBase) + endpoint));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    auto *reply = m_net.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleAuthReply(reply);
    });
}

void PvnAuthController::handleAuthReply(QNetworkReply *reply)
{
    reply->deleteLater();
    const auto data = reply->readAll();
    const auto doc  = QJsonDocument::fromJson(data);
    const auto obj  = doc.isObject() ? doc.object() : QJsonObject();

    if (reply->error() != QNetworkReply::NoError) {
        const QString serverMsg = obj.value("error").toString();
        fail(serverMsg.isEmpty()
                 ? tr("Backend auth failed: %1").arg(reply->errorString())
                 : serverMsg);
        return;
    }
    if (obj.isEmpty()) {
        fail(tr("Backend returned invalid JSON"));
        return;
    }
    m_jwt       = obj.value("token").toString();
    m_userEmail = obj.value("user").toObject().value("email").toString();
    m_userName  = obj.value("user").toObject().value("name").toString();
    const QString cfg = obj.value("config").toString();
    if (m_jwt.isEmpty() || cfg.isEmpty()) {
        fail(tr("Backend response missing token or config"));
        return;
    }
    persistSession();
    emit userChanged();
    importReturnedConfig(cfg);
}

void PvnAuthController::importReturnedConfig(const QString &configText)
{
    setState(State::Importing);
    auto result = m_importController->extractConfigFromData(configText, QStringLiteral("pvn.conf"));
    if (result.errorCode != ErrorCode::NoError || result.config.isEmpty()) {
        fail(tr("Failed to extract config from backend response"));
        return;
    }
    m_importController->importConfig(result.config);
    setState(State::Authenticated);
    emit signInCompleted();
}

void PvnAuthController::signOut()
{
    // Tear down the tunnel first — the server-side peer is about to be
    // revoked, so keepalives would fail anyway.
    if (m_connectionController && m_connectionController->isConnected()) {
        m_connectionController->closeConnection();
    }

    // Fire-and-forget revoke the peer on the backend. Uses the current JWT;
    // if the network is down we still clear locally so the user isn't stuck.
    postBackendLogout();

    m_jwt.clear();
    m_userEmail.clear();
    m_userName.clear();
    clearPersistedSession();
    setState(State::Idle);
    emit userChanged();
}

void PvnAuthController::postBackendLogout()
{
    if (m_jwt.isEmpty()) return;

    QNetworkRequest req(QUrl(QString::fromLatin1(kBackendBase) + QStringLiteral("/me/logout")));
    req.setRawHeader("Authorization", ("Bearer " + m_jwt).toUtf8());

    auto *reply = m_net.post(req, QByteArray());
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
}

void PvnAuthController::maybeRefreshSession()
{
    if (m_jwt.isEmpty()) return;

    const qint64 exp = jwtExpiryFromToken(m_jwt);
    const qint64 now = QDateTime::currentSecsSinceEpoch();

    // If we can't parse exp, assume the token is stale — force a refresh so we
    // either roll a new token or get a clean 401 and log the user out.
    if (exp != 0 && exp - now > kRefreshMarginSeconds) {
        return;
    }

    QNetworkRequest req(QUrl(QString::fromLatin1(kBackendBase) + QStringLiteral("/me/refresh")));
    req.setRawHeader("Authorization", ("Bearer " + m_jwt).toUtf8());

    auto *reply = m_net.post(req, QByteArray());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() == QNetworkReply::AuthenticationRequiredError
            || reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 401) {
            // Server disowned the session — clear locally, force re-login. Do
            // NOT call closeConnection here: the user hasn't asked to sign out,
            // and we don't want to yank the tunnel out from under a UI that
            // may be showing "connected". They'll be prompted to sign in again
            // on next launch or when they touch settings.
            m_jwt.clear();
            m_userEmail.clear();
            m_userName.clear();
            clearPersistedSession();
            setState(State::Idle);
            emit userChanged();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) return; // network hiccup, try again next launch

        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) return;
        const QString token = doc.object().value("token").toString();
        if (token.isEmpty()) return;
        m_jwt = token;
        persistSession();
    });
}

void PvnAuthController::loadPersistedSession()
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    m_jwt       = s.value(QStringLiteral("pvn/jwt")).toString();
    m_userEmail = s.value(QStringLiteral("pvn/email")).toString();
    m_userName  = s.value(QStringLiteral("pvn/name")).toString();
    if (!m_jwt.isEmpty()) {
        m_state = State::Authenticated;
    }
}

void PvnAuthController::persistSession()
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    s.setValue(QStringLiteral("pvn/jwt"),   m_jwt);
    s.setValue(QStringLiteral("pvn/email"), m_userEmail);
    s.setValue(QStringLiteral("pvn/name"),  m_userName);
}

void PvnAuthController::clearPersistedSession()
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    s.remove(QStringLiteral("pvn/jwt"));
    s.remove(QStringLiteral("pvn/email"));
    s.remove(QStringLiteral("pvn/name"));
}

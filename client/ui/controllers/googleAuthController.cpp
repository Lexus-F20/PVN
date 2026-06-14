#include "googleAuthController.h"

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

namespace {
// Worker base URL — change here if backend moves
constexpr auto kBackendBase = "https://pvn-backend.alexromanov765.workers.dev";

// One of the three Google OAuth client IDs registered for the PVN project.
// Desktop client (used on Windows/macOS/Linux).
constexpr auto kDesktopClientId =
    "360393448931-g3b1mh8m7pqts3l525kfuib90n6k2bua.apps.googleusercontent.com";
}

GoogleAuthController::GoogleAuthController(ImportController *importController,
                                           ConnectionController *connectionController,
                                           QObject *parent)
    : QObject(parent),
      m_importController(importController),
      m_connectionController(connectionController)
{
    connect(&m_callbackServer, &QTcpServer::newConnection,
            this, &GoogleAuthController::onCallbackConnection);

    loadPersistedSession();
}

void GoogleAuthController::setState(State s)
{
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void GoogleAuthController::fail(const QString &msg)
{
    m_errorMessage = msg;
    setState(State::Failed);
    emit signInFailed(msg);
    if (m_callbackServer.isListening()) m_callbackServer.close();
}

QString GoogleAuthController::randomBase64Url(int bytes)
{
    QByteArray buf(bytes, 0);
    auto *rng = QRandomGenerator::securelySeeded().system();
    for (int i = 0; i < bytes; ++i) {
        buf[i] = static_cast<char>(rng->bounded(256));
    }
    return QString::fromLatin1(buf.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString GoogleAuthController::sha256Base64Url(const QString &input)
{
    const QByteArray hash =
        QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(hash.toBase64(
        QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}

QString GoogleAuthController::deviceId() const
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    QString id = s.value(QStringLiteral("pvn/deviceId")).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.setValue(QStringLiteral("pvn/deviceId"), id);
    }
    return id;
}

bool GoogleAuthController::startLocalCallbackServer()
{
    if (m_callbackServer.isListening()) m_callbackServer.close();
    if (!m_callbackServer.listen(QHostAddress::LocalHost, 0)) {
        return false;
    }
    m_callbackPort = m_callbackServer.serverPort();
    m_redirectUri = QStringLiteral("http://127.0.0.1:%1/callback").arg(m_callbackPort);
    return true;
}

void GoogleAuthController::signInWithGoogle()
{
    m_errorMessage.clear();
    if (!startLocalCallbackServer()) {
        fail(tr("Failed to start local callback server"));
        return;
    }

    m_pkceVerifier = randomBase64Url(32);
    const QString challenge = sha256Base64Url(m_pkceVerifier);
    m_oauthState = randomBase64Url(16);

    QUrl authUrl(QStringLiteral("https://accounts.google.com/o/oauth2/v2/auth"));
    QUrlQuery q;
    q.addQueryItem("response_type", "code");
    q.addQueryItem("client_id", kDesktopClientId);
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

void GoogleAuthController::onCallbackConnection()
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

void GoogleAuthController::handleCallback(const QString &code, const QString &returnedState)
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

void GoogleAuthController::exchangeCodeForIdToken(const QString &code)
{
    setState(State::ExchangingCode);

    QNetworkRequest req(QUrl("https://oauth2.googleapis.com/token"));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  "application/x-www-form-urlencoded");
    QUrlQuery body;
    body.addQueryItem("grant_type",    "authorization_code");
    body.addQueryItem("client_id",     kDesktopClientId);
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

void GoogleAuthController::callBackendAuth(const QString &idToken)
{
    setState(State::CallingBackend);

    QNetworkRequest req(QUrl(QString::fromLatin1(kBackendBase) + "/auth/google"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject reqBody;
    reqBody["idToken"]    = idToken;
    reqBody["deviceId"]   = deviceId();
    reqBody["deviceName"] = QSysInfo::machineHostName().left(40);

    auto *reply = m_net.post(req, QJsonDocument(reqBody).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Backend auth failed: %1").arg(reply->errorString()));
            return;
        }
        const auto data = reply->readAll();
        const auto doc  = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            fail(tr("Backend returned invalid JSON"));
            return;
        }
        const auto obj = doc.object();
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
    });
}

void GoogleAuthController::importReturnedConfig(const QString &configText)
{
    setState(State::Importing);
    auto result = m_importController->extractConfigFromData(configText, QStringLiteral("pvn.conf"));
    if (result.importResult != ImportController::ImportResult::Success) {
        fail(tr("Failed to extract config from backend response"));
        return;
    }
    m_importController->importConfig(result.config);
    setState(State::Authenticated);
    emit signInCompleted();
}

void GoogleAuthController::signOut()
{
    m_jwt.clear();
    m_userEmail.clear();
    m_userName.clear();
    clearPersistedSession();
    setState(State::Idle);
    emit userChanged();
}

void GoogleAuthController::loadPersistedSession()
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    m_jwt       = s.value(QStringLiteral("pvn/jwt")).toString();
    m_userEmail = s.value(QStringLiteral("pvn/email")).toString();
    m_userName  = s.value(QStringLiteral("pvn/name")).toString();
    if (!m_jwt.isEmpty()) {
        m_state = State::Authenticated;
    }
}

void GoogleAuthController::persistSession()
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    s.setValue(QStringLiteral("pvn/jwt"),   m_jwt);
    s.setValue(QStringLiteral("pvn/email"), m_userEmail);
    s.setValue(QStringLiteral("pvn/name"),  m_userName);
}

void GoogleAuthController::clearPersistedSession()
{
    QSettings s(QStringLiteral("PVN"), QStringLiteral("PVN"));
    s.remove(QStringLiteral("pvn/jwt"));
    s.remove(QStringLiteral("pvn/email"));
    s.remove(QStringLiteral("pvn/name"));
}

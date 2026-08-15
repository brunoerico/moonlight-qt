#pragma once

#include <QObject>
#include <QVariant>

class ComputerManager;
class NvComputer;
class Session;
class StreamingPreferences;

namespace LanhouseConnect
{

class Event;
class LauncherPrivate;

// Combines CliPair::Launcher and CliStartStream::Launcher into one
// non-interactive flow for LanHouse's own "click play inside the app" path
// (ORCHESTRATOR.md, Fase 2): pairs automatically if needed - reporting the
// PIN to lanhouse-web instead of showing it, the same way the WebRTC bridge
// already does (Fase 3, dev/lanhouse-stream/client/src/api/host.rs) - then
// starts the stream. The actual pairing handshake (ComputerManager::pairHost)
// and stream setup are unchanged from the two existing flows; only how the
// PIN is communicated is different.
class Launcher : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE_D(m_DPtr, Launcher)

public:
    explicit Launcher(QString computer, QString app, QString ticket, QString lanhouseHostId,
                      StreamingPreferences* preferences,
                      QObject *parent = nullptr);
    ~Launcher();
    Q_INVOKABLE void execute(ComputerManager *manager);
    Q_INVOKABLE void quitRunningApp();
    Q_INVOKABLE bool isExecuted() const;

signals:
    void searchingComputer();
    void pairing();
    void searchingApp();
    void sessionCreated(QString appName, Session *session);
    void failed(QString text);
    void appQuitRequired(QString appName);

private slots:
    void onComputerFound(NvComputer *computer);
    void onComputerUpdated(NvComputer *computer);
    void onPairingCompleted(NvComputer *computer, QString error);
    void onTimeout();
    void onQuitAppCompleted(QVariant error);

private:
    QScopedPointer<LauncherPrivate> m_DPtr;
};

}

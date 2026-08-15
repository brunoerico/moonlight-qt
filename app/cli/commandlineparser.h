#pragma once

#include "settings/streamingpreferences.h"

#include <QMap>
#include <QString>

class GlobalCommandLineParser
{
public:
    enum ParseResult {
        NormalStartRequested,
        StreamRequested,
        QuitRequested,
        PairRequested,
        ListRequested,
        LanhouseConnectRequested,
    };

    GlobalCommandLineParser();
    virtual ~GlobalCommandLineParser();

    ParseResult parse(const QStringList &args);

};

class QuitCommandLineParser
{
public:
    QuitCommandLineParser();
    virtual ~QuitCommandLineParser();

    void parse(const QStringList &args);

    QString getHost() const;

private:
    QString m_Host;
};

class PairCommandLineParser
{
public:
    PairCommandLineParser();
    virtual ~PairCommandLineParser();

    void parse(const QStringList &args);

    QString getHost() const;
    QString getPredefinedPin() const;

private:
    QString m_Host;
    QString m_PredefinedPin;
};

class StreamCommandLineParser
{
public:
    StreamCommandLineParser();
    virtual ~StreamCommandLineParser();

    void parse(const QStringList &args, StreamingPreferences *preferences);

    QString getHost() const;
    QString getAppName() const;

private:
    QString m_Host;
    QString m_AppName;
    QMap<QString, StreamingPreferences::WindowMode> m_WindowModeMap;
    QMap<QString, StreamingPreferences::AudioConfig> m_AudioConfigMap;
    QMap<QString, StreamingPreferences::VideoCodecConfig> m_VideoCodecMap;
    QMap<QString, StreamingPreferences::VideoDecoderSelection> m_VideoDecoderMap;
    QMap<QString, StreamingPreferences::CaptureSysKeysMode> m_CaptureSysKeysModeMap;
};

class ListCommandLineParser
{
public:
    ListCommandLineParser();
    virtual ~ListCommandLineParser();

    void parse(const QStringList &args);

    QString getHost() const;
    bool isPrintCSV() const;
    bool isVerbose() const;

private:
    QString m_Host;
    bool m_PrintCSV;
    bool m_Verbose;
};

// Combines pair + stream in one non-interactive action, for LanHouse's own
// "click play inside the app" flow (see lanhouse-web's ORCHESTRATOR.md,
// Fase 2) - reports the pairing PIN to lanhouse-web instead of displaying
// it, since there's no human here to read it off the screen and type it
// into the host's admin panel.
class LanhouseConnectCommandLineParser
{
public:
    LanhouseConnectCommandLineParser();
    virtual ~LanhouseConnectCommandLineParser();

    void parse(const QStringList &args);

    QString getHost() const;
    QString getAppName() const;
    QString getTicket() const;
    QString getLanhouseHostId() const;
    QString getLiveSessionId() const;

private:
    QString m_Host;
    QString m_AppName;
    QString m_Ticket;
    QString m_LanhouseHostId;
    QString m_LiveSessionId;
};

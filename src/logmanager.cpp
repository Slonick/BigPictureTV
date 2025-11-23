#include "logmanager.h"

#include <QFile>
#include <QDateTime>
#include <QTextStream>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <iostream>
#include <QMutex>

// Static member initialization
QString LogManager::m_logFilePath;
bool LogManager::m_initialized = false;
static QMutex g_logMutex;

bool LogManager::initialize(const QString& logFilePath)
{
    QMutexLocker locker(&g_logMutex);

    // Determine log file path
    if (logFilePath.isEmpty()) {
        // Try AppData/Local first (preferred for deployed apps)
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir appDataDir(appDataPath);
        if (!appDataDir.exists()) {
            appDataDir.mkpath(".");
        }
        m_logFilePath = appDataPath + "/logs.txt";

        // If AppData fails, try application directory
        QFile testFile(m_logFilePath);
        if (!testFile.open(QIODevice::Append | QIODevice::Text)) {
            // Fallback to application directory
            m_logFilePath = QCoreApplication::applicationDirPath() + "/logs.txt";
        } else {
            testFile.close();
        }
    } else {
        m_logFilePath = logFilePath;
    }

    // Try to create/open the log file to verify we can write to it
    QFile logFile(m_logFilePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        std::cerr << "Failed to open log file: " << m_logFilePath.toStdString() << std::endl;
        return false;
    }
    logFile.close();

    m_initialized = true;
    // Release lock before logging to avoid deadlock
    locker.unlock();

    info("========================================");
    info("LogManager initialized");
    info("========================================");
    return true;
}

void LogManager::shutdown()
{
    QMutexLocker locker(&g_logMutex);
    if (m_initialized) {
        info("LogManager shutting down");
        m_initialized = false;
    }
}

void LogManager::debug(const QString& message)
{
    writeLog("DEBUG", message);
}

void LogManager::info(const QString& message)
{
    writeLog("INFO", message);
}

void LogManager::warning(const QString& message)
{
    writeLog("WARNING", message);
}

void LogManager::error(const QString& message)
{
    writeLog("ERROR", message);
}

QDebug LogManager::log()
{
    // This returns a debug stream that can be used like: LogManager::log() << "message" << value
    return qDebug();
}

void LogManager::writeLog(const QString& level, const QString& message)
{
    QMutexLocker locker(&g_logMutex);

    if (!m_initialized || m_logFilePath.isEmpty()) {
        return;
    }

    // Create formatted log entry
    QString timestamp = getTimestamp();
    QString logEntry = QString("[%1] [%2] %3").arg(timestamp, level, message);

    // Output to console
    std::cout << logEntry.toStdString() << std::endl;
    std::cout.flush();

    // Check and rotate log if needed before writing
    checkAndRotateLog();

    // Write to file
    QFile logFile(m_logFilePath);
    if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
        // Silently fail - don't try to log the error to avoid recursion
        return;
    }

    QTextStream stream(&logFile);
    stream << logEntry << "\n";
    stream.flush();
    logFile.close();
}

void LogManager::checkAndRotateLog()
{
    QFile logFile(m_logFilePath);

    if (!logFile.exists()) {
        return;
    }

    // Check file size
    if (logFile.size() >= MAX_LOG_SIZE) {
        QString previousLogPath = m_logFilePath.replace("logs.txt", "logs_previous.txt");

        // Delete existing logs_previous.txt if it exists
        QFile previousLogFile(previousLogPath);
        if (previousLogFile.exists()) {
            previousLogFile.remove();
        }

        // Rename current logs.txt to logs_previous.txt
        if (logFile.rename(m_logFilePath, previousLogPath)) {
            // Reset m_logFilePath to original location
            m_logFilePath = m_logFilePath.replace("logs_previous.txt", "logs.txt");
        }
    }
}

QString LogManager::getTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}

#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QString>
#include <QDebug>
#include <memory>

/**
 * @class LogManager
 * @brief Centralized logging manager for console and file output
 *
 * The LogManager class handles all application logging with the following features:
 * - Outputs logs to both console (stdout) and file (logs.txt)
 * - Automatic log file rotation when size exceeds MAX_LOG_SIZE
 * - Creates logs_previous.txt as backup when rotating
 * - Thread-safe logging operations
 */
class LogManager {
public:
    /**
     * @brief Initialize the log manager
     * @param logFilePath Path where logs.txt will be stored
     * @return true if initialization successful, false otherwise
     */
    static bool initialize(const QString& logFilePath = "");

    /**
     * @brief Shutdown the log manager
     */
    static void shutdown();

    /**
     * @brief Log a message at debug level
     * @param message The message to log
     */
    static void debug(const QString& message);

    /**
     * @brief Log a message at info level
     * @param message The message to log
     */
    static void info(const QString& message);

    /**
     * @brief Log a message at warning level
     * @param message The message to log
     */
    static void warning(const QString& message);

    /**
     * @brief Log a message at error level
     * @param message The message to log
     */
    static void error(const QString& message);

    /**
     * @brief Convenience function - similar to qDebug() style usage
     * @return QDebug stream for convenient chaining
     */
    static QDebug log();

private:
    LogManager() = delete;
    ~LogManager() = default;

    static constexpr qint64 MAX_LOG_SIZE = 1024 * 1024; // 1 MB

    /**
     * @brief Write log entry to file and console
     * @param level Log level as string
     * @param message The message to log
     */
    static void writeLog(const QString& level, const QString& message);

    /**
     * @brief Check if log file needs rotation and perform if necessary
     */
    static void checkAndRotateLog();

    /**
     * @brief Get timestamp for log entries
     * @return Current timestamp in format: YYYY-MM-DD HH:mm:ss
     */
    static QString getTimestamp();

    static QString m_logFilePath;
    static bool m_initialized;
};

#endif // LOGMANAGER_H

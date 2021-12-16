// 

#ifndef CPOTESTCOMMON_PROGRUNNER_H_
#define CPOTESTCOMMON_PROGRUNNER_H_

#include <QObject>
#include <QString>
#include <QSet>

class QProcess;
class QProcessEnvironment;
class QRegularExpression;

/** Wrapper to run a local PostgreSQL daemon.
 */
class ProgRunner : public QObject{
    Q_OBJECT
public:
    /** Initialize the state, but do not run any process.
     *
     */
    ProgRunner(QObject *parent = nullptr);

    /// Ensure worker has exited
    ~ProgRunner();

    /** Define the program to-be-run.
     *
     * @param prog The program name/path.
     * @param args The arguments to run with.
     */
    void setProgram(const QString &prog, const QStringList &args);
    /** Define the run environment.
     *
     * @param env The subprocess environment.
     */
    void setEnvironment(const QProcessEnvironment &env);

    /** Set the exit code which will result in successful stop()/join() calls.
     *
     * @param code The desired exit code.
     */
    void setGoodExitCode(const QSet<int> &codes);

    /** Start a subprocess program.
     *
     * @return True if the program started successfully.
     */
    virtual bool start();

    /** Terminate a running service.
     *
     * @return True if the process exited normally.
     */
    virtual bool stop(int timeoutMs=30000);

    /** Wait in Qt event loop for the process to finish.
     *
     * @return True if the process finished with exit code zero.
     */
    virtual bool join(int timeoutMs=30000);

    /** Pop the next line off of the stdout/stderr queue.
     *
     * @return The raw line of data. If null or empty, it indicates no data
     * is yet available.
     * @sa linesAvailable()
     */
    QByteArray nextLine();

    /** Wait in an event loop for a specific line on the process output.
     *
     * @param expr The expression to match.
     * @param timeoutMs Time to wait (in milliseconds).
     * @return True if the line was seen.
     */
    bool waitForLineRe(const QRegularExpression &expr, int timeoutMs=10000);

private slots:
    /** Handle output from the process.
     * @post Updates #_outQueue with lines.
     */
    void stdoutReadable();
    /** Handle output from the process.
     * @post Updates #_errQueue with lines.
     */
    void stderrReadable();

signals:
    /** Emitted when a new line is available for reading.
     * @sa nextLine();
     */
    void linesAvailable();

private:
    /// Valid exit codes
    QSet<int> _goodExit;
    /// The child process
    QProcess *_proc;
    /// Output data/line buffer
    QList<QByteArray> _readQueue;
};


#endif /* CPOTESTCOMMON_PROGRUNNER_H_ */

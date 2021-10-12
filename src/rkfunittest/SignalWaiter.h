//  

#ifndef SIGNAL_WAITER_H
#define SIGNAL_WAITER_H

#include <QObject>

class QEventLoop;
class QTimer;

/** This is an internal helper class to synchronize on session-side activity.
 * These objects wait in a non-GUI event loop for particular signals.
 */
class SignalWaiter : public QObject{
    Q_OBJECT
public:
    /** Construct an unconnected waiter.
     *
     * @param parent The parent for this QObject.
     */
    SignalWaiter(QObject *parent = nullptr);

    /** Waits in an event loop for particular signals.
     * If any signal was received between object construction or an earlier
     * call to wait(), then wait exits immediately with true.
     *
     * @param timeoutMs The wait time in milliseconds.
     * If non-positive, the wait is indefinite.
     * @return True if the signal was received.
     */
    bool wait(int timeoutMs);

public slots:

    /// Called when the expected signal is emitted
    void received();

private slots:
    /// Called after the timeout
    void timeout();

private:
    /// The event loop to wait within
    QEventLoop *_loop;
    /// Timeout timer
    QTimer *_timer;
    /// Flag to indicate signal received before wait() called
    bool _received;
};

#endif /* SIGNAL_WAITER_H */

//  

#include <QTimer>
#include <QDebug>
#include <QEventLoop>
#include "SignalWaiter.h"
#include "ratcommon/PostSet.h"

SignalWaiter::SignalWaiter(QObject *parent)
  : QObject(parent), _received(false){
    _loop = new QEventLoop(this);
    _timer = new QTimer(this);
    _timer->setSingleShot(true);
    connect(_timer, &QTimer::timeout, this, &SignalWaiter::timeout);
}

bool SignalWaiter::wait(int timeoutMs){
    // Always reset this state when returning
    PostSet<bool> ps(_received, false);

    // If the signal was already received
    if(_received){
        return true;
    }

    if(timeoutMs > 0){
        _timer->start(timeoutMs);
    }

    // Wait for the signal, and reset internal status either way
    return (_loop->exec(QEventLoop::ExcludeUserInputEvents) == 0);
}

void SignalWaiter::received(){
    _received = true;
    _timer->stop(); // avoid delayed timeouts
    _loop->exit(0);
}

void SignalWaiter::timeout(){
    _loop->exit(1);
}

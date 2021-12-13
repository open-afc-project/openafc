// Copyright (C) 2017 RKF Engineering Solutions, LLC

#ifndef SRC_RATCOMMON_EXCEPTIONSAFECOREAPP_H_
#define SRC_RATCOMMON_EXCEPTIONSAFECOREAPP_H_

#include "ratcommon_export.h"
#include <QCoreApplication>

/** Provide a QCoreApplication override which intercepts exceptions in
 * event handlers and logs error messages.
 */
class RATCOMMON_EXPORT ExceptionSafeCoreApp : public QCoreApplication{
    Q_OBJECT
public:
    /** No extra behavior in constructor.
     *
     * @param argc The number of command arguments.
     * @param argv The command arguments.
     */
    ExceptionSafeCoreApp(int &argc, char **argv);

    /// Interface for QCoreApplication
    virtual bool notify(QObject *obj, QEvent *event);

    /** Log an error message.
     *
     * @param obj The object associated with the error.
     * @param mObj The meta-object associated with @c obj.
     * @param event The event associated with the error.
     * @param msg The error message.
     */
    void logError(const QObject *obj, const QMetaObject *mObj, const QEvent *event, const QString &msg) const;

    /** Overload to provide status messages.
     * @return The exit code of the QCoreApplication.
     */
    static int exec();
};

#endif /* SRC_RATCOMMON_EXCEPTIONSAFECOREAPP_H_ */

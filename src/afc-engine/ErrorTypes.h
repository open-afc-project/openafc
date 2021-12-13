/*
 * All text, the source code, and the algorithms fixed in this document or otherwise
 * delivered to TerreStar Networks, Inc. are Copyright 2008 RKF Engineering Solutions, LLC ("RKF").
 * All Rights Reserved. No use of the same may be made without prior, written license from RKF.
 *
 * RKF hereby grants Terrestar Networks, Inc, the nonexclusive right and license to use this
 * protected property only for the purpose of upgrading and maintaining that certain software
 * installed on machines delivered to TerreStar pursuant to agreement between Terrestar and
 * RKF Engineering Solutions, LLC relating to RKF professional services in the establishment
 * of TerreStar's Global Resource Management ("GRM") project, .
 *
 * For further guidance on the use of RKF's protected properties related to the GRM project,
 * please contact RKF Engineering, LLC.
 */

#ifndef ERROR_TYPES_H
#define ERROR_TYPES_H

#include <stdexcept>
#include <QString>

/** A convenience class to construct std::runtime_error from QStrings.
 */
class RuntimeError : public std::runtime_error{
public:
    /** Create a new error with title and message.
     * @param title Is used as the title of the error dialog.
     * @param msg I used as the body of the message dialog.
     * @param parent Optionally define the parent for the dialog.
     */
    RuntimeError(const QString &msg)
      : runtime_error(msg.toStdString()){}

    /// Required for std::exception child
    virtual ~RuntimeError() throw(){}
};

/** Represent an error which should be displayed as a dialog.
 * This is not derived from std::exception so that it is guaranteed not to
 * be trapped in a normal catch block.
 */
class FatalError{
public:
    /** Create a new error with title and message.
     */
    FatalError(const QString &titleVal, const QString &msg)
      : _title(titleVal), _msg(msg){}

    /// Same behavior as std::exception
    virtual ~FatalError() throw(){}

    /** Get the title string for the error.
     */
    const QString & title() const throw(){
        return _title;
    }

    /** Duck-type replacement for std::exception::what().
     */
    const QString & what() const throw(){
        return _msg;
    }

protected:
    /// Intended title of the dialog
    QString _title;
    /// Intended message in the dialog
    QString _msg;
};

#endif /* ERROR_TYPES_H */

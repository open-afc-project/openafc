//

#ifndef SQL_ERROR_H
#define SQL_ERROR_H

#include <stdexcept>
#include <QSqlError>

/// Type used for errors in database access
class SqlError : public std::runtime_error
{
    public:
        /** General error condition.
         *
         * @param msg The error message.
         */
        SqlError(const QString &msg);

        /** Error associated with an underlying QSqlError.
         *
         * @param msg The context for the error.
         * @param err The error itself.
         * The parts of the @c err object are extracted into this object message.
         */
        SqlError(const QString &msg, const QSqlError &err);

        /// Required no-throw specification
        ~SqlError() throw()
        {
        }

        /** Get the original QSqlError (if one exists).
         * @return The source error info, or an invalid QSqlError.
         */
        const QSqlError &dbError() const
        {
            return _err;
        }

        /** Get an associated QSqlError error number (if one exists).
         * @return An error number, if applicable, or -1.
         */
        int errNum() const
        {
            return _err.number();
        }

    private:
        /// The SQL error struct
        QSqlError _err;
};

#endif /* SQL_ERROR_H */

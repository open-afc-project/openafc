//  

#ifndef SRC_RATCOMMON_TEXTHELPERS_H_
#define SRC_RATCOMMON_TEXTHELPERS_H_

//#include "ratcommon_export.h"
#include <QString>

class QAbstractSocket;
class QNetworkReply;
class QProcessEnvironment;
class UtcDateTime;
class DateTimeInterval;

/** Namespace for simple text conversion functions.
 * Most convert to/from Qt string representations and throw RuntimeError to
 * signal failures.
 */
namespace TextHelpers{

/** Convert text to an integer value.
 *
 * @param text The text representing a number.
 * @return The number represented.
 * @throw RuntimeError if the text cannot be converted.
 */
//RATCOMMON_EXPORT
int toInt(const QString &text);

/** Convert text to a floating point value.
 *
 * @param text The text representing a number.
 * @return The number represented.
 * @throw RuntimeError if the text cannot be converted.
 */
//RATCOMMON_EXPORT
double toNumber(const QString &text);

/** Convert an integer to hexadecimal digits.
 *
 * @param value The value to convert.
 * @param digits The required number of digits in the result.
 * Unused digits will be padded with zeros.
 * @return
 */
//RATCOMMON_EXPORT
QString toHex(int value, int digits = -1);

/** Convert from hexadecimal string representation.
 *
 * @param text The string with no leading "0x" or any other characters.
 * @return The number represented.
 */
//RATCOMMON_EXPORT
int fromHex(const QString &text);

/** Convert a byte array into a printable hexadecimal string.
 * The result is of the form "12:34:AB" where colons separate the octets,
 * and all alpha characters are uppercase.
 * This is the same form as used by the "openssl x509" command.
 *
 * @param hash The raw bytes of a data set (i.e. a fingerprint).
 * @return The formatted hexadecimal string.
 */
//RATCOMMON_EXPORT
QString toHex(const QByteArray &hash);

/** Construct a string with random hexadecimal digits.
 *
 * @param digits The number of base-16 digits to choose.
 * @return The resulting string of length @c digits.
 */
//RATCOMMON_EXPORT
QString randomHexDigits(int digits);

/** This string is used for QDateTime::toString() to consistently format
 * QDateTime for display.
 * @param sep The date--time separator character.
 * The ISO-standard value here is 'T'.
 * @return The display format string.
 */
//RATCOMMON_EXPORT
QString dateTimeFormat(const QChar &sep = QChar(' '));

/** Surround a string with quotation characters.
 * @param text The text to quote.
 * @return The quoted text.
 */
//RATCOMMON_EXPORT
QString quoted(const QString &text);

/** Surround each element of a string list with quotation characters.
 * @param text The text items to quote.
 * @return The quoted text items.
 */
//RATCOMMON_EXPORT
QStringList quoted(const QStringList &text);

/** Get a name for a socket connection.
 *
 * @param socket The socket to name.
 * @return A combination of the socket's peer name and port.
 */
//RATCOMMON_EXPORT
QString peerName(const QAbstractSocket &socket);

/** Get the human readable name for an HTTP operation.
 *
 * @param reply The reply to take the operation from.
 * @return The name for the operation, in original character encoding.
 */
//RATCOMMON_EXPORT
QByteArray operationName(const QNetworkReply &reply);

/** Retrieve the standard errno information in a thread safe way.
 * @return The string associated with the local errno value.
 * The format is "(N) STR" where N is the original error number.
 */
//RATCOMMON_EXPORT
QString qstrerror();

/** Generate a nicely formatted debug string corresponding with a child
 * process invocation.
 *
 * @param args The full set of arguments to execute.
 * The first @c args string is the executable itself.
 * @param env The full environment of the child process.
 * @return A string suitable for copy-paste into a bash terminal for diagnosis.
 */
//RATCOMMON_EXPORT
QString executableString(const QStringList &args, const QProcessEnvironment &env);

/** Combine a display label and an optional value unit together.
 * This simple function provides consistent use of unit labels.
 *
 * @param label The non-empty label for the value.
 * @param unit A possibly-empty unit for the value.
 * @return The combined representation.
 */
//RATCOMMON_EXPORT
QString combineLabelUnit(const QString &label, const QString &unit);

/** Get text name for non-finite floating point values.
 *
 * @param value The non-finite value to get a name for.
 * @return The human-readable name for the value.
 */
//RATCOMMON_EXPORT
QString nonfiniteText(double value);

}

#endif /* SRC_RATCOMMON_TEXTHELPERS_H_ */

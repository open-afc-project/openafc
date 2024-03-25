//

#include "TextHelpers.h"
#include "afclogging/ErrStream.h"
#include <cerrno>
#include <cmath>
#include <QStringList>
#include <QAbstractSocket>
#include <QNetworkReply>
#include <QHostAddress>
#include <QProcessEnvironment>

int TextHelpers::toInt(const QString &text)
{
	bool valid;
	const double val = text.toInt(&valid);
	if (!valid) {
		throw std::runtime_error(ErrStream()
					 << "Failed to convert '" << text << "' to integer");
	}
	return val;
}

double TextHelpers::toNumber(const QString &text)
{
	bool valid;
	const double val = text.toDouble(&valid);
	if (!valid) {
		throw std::runtime_error(ErrStream()
					 << "Failed to convert '" << text << "' to double");
	}
	return val;
}

QString TextHelpers::toHex(int value, int digits)
{
	return QString("%1").arg(value, digits, 16, QChar('0'));
}

int TextHelpers::fromHex(const QString &text)
{
	bool ok;
	const int val = text.toInt(&ok, 16);
	if (!ok) {
		return -1;
	}
	return val;
}

QString TextHelpers::toHex(const QByteArray &hash)
{
	QByteArray hex = hash.toHex();
	// Insert separators from back to front
	for (int ix = hex.count() - 2; ix > 0; ix -= 2) {
		hex.insert(ix, ':');
	}
	// Guarantee case
	return hex.toUpper();
}

namespace
{
/// Choice of hexadecimal digits
const QString hexDigits("0123456789ABCDEF");
/// Number of hexDigits chars
const int hexDigitCount = 16;
}

QString TextHelpers::randomHexDigits(int digits)
{
	QString str;
	str.resize(digits);
	for (QString::iterator it = str.begin(); it != str.end(); ++it) {
		*it = hexDigits.at(::rand() % hexDigitCount);
	}
	return str;
}

QString TextHelpers::dateTimeFormat(const QChar &sep)
{
	return QString("dd-MMM-yyyy%1HH:mm:ssZ").arg(sep);
}

QString TextHelpers::quoted(const QString &text)
{
	return QString("\"%1\"").arg(text);
}

QStringList TextHelpers::quoted(const QStringList &text)
{
	QStringList result;
	foreach(const QString &item, text)
	{
		result.append(quoted(item));
	}
	return result;
}

QString TextHelpers::peerName(const QAbstractSocket &socket)
{
	QString hostname = socket.peerName();
	if (hostname.isEmpty()) {
		hostname = socket.peerAddress().toString();
	}
	return QString("%1:%2").arg(hostname).arg(socket.peerPort());
}

QByteArray TextHelpers::operationName(const QNetworkReply &reply)
{
	switch (reply.operation()) {
		case QNetworkAccessManager::HeadOperation:
			return "HEAD";
		case QNetworkAccessManager::GetOperation:
			return "GET";
		case QNetworkAccessManager::PutOperation:
			return "PUT";
		case QNetworkAccessManager::PostOperation:
			return "POST";
		case QNetworkAccessManager::DeleteOperation:
			return "DELETE";
		default:
			return reply.request()
				.attribute(QNetworkRequest::CustomVerbAttribute)
				.toByteArray();
	}
}

QString TextHelpers::qstrerror()
{
#if defined(Q_OS_WIN)
	const int errnum = errno;
	const size_t bufLen = 1024;
	char buffer[bufLen];
	::strerror_s(buffer, bufLen, errnum);
	const char *stat = buffer;
#else
	const int errnum = errno;
	const size_t bufLen = 1024;
	char buffer[bufLen];
	::strerror_r(errnum, buffer, bufLen);
	const char *const stat = buffer;
#endif

	const char *msg;
	if (stat == NULL) {
		msg = "Unknown";
	} else {
		msg = stat;
	}
	return QString("(%1) %2").arg(errnum).arg(QString::fromLocal8Bit(msg));
}

namespace
{
QString quoteAndEscape(const QString &in)
{
	QString out(in);
	out.replace("\"", "\\\"");
	return QString("\"%1\"").arg(out);
}
}

QString TextHelpers::executableString(const QStringList &args, const QProcessEnvironment &env)
{
	QStringList envkeys = env.keys();
	qSort(envkeys);
	QStringList envparts;
	foreach(const QString &key, envkeys)
	{
		envparts << QString("%1=%2").arg(key, quoteAndEscape(env.value(key)));
	}

	QStringList cmdparts;
	foreach(const QString &arg, args)
	{
		cmdparts << quoteAndEscape(arg);
	}

	return (envparts + cmdparts).join(" ");
}

QString TextHelpers::combineLabelUnit(const QString &label, const QString &unit)
{
	if (unit.isEmpty()) {
		return label;
	}
	return QString("%1 (%2)").arg(label, unit);
}

QString TextHelpers::nonfiniteText(double value)
{
	// match default QString behavior
	if (std::isnan(value)) {
		return "nan";
	}
	if (std::isinf(value)) {
		if (value > 0) {
			return "+inf";
		} else {
			return "-inf";
		}
	}
	return QString();
}

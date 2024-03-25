//

#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QDir>
#include "SearchPaths.h"
#include "afclogging/Logging.h"

namespace
{
/// Logger for all instances of class
LOGGER_DEFINE_GLOBAL(logger, "SearchPaths")

/** A convenience class to perform multiple path extensions.
 */
class Extender
{
	public:
		/** Create a new extender.
		 *
		 * @param suffix The suffix to append. If non-empty, the suffix itself
		 * will be forced to start with a path separator.
		 */
		Extender(const QString &suffix) : _suffix(suffix)
		{
			if (!_suffix.isEmpty()) {
				_suffix.prepend(QDir::separator());
			}
		}

		/** Append the application-specific suffix to the search paths.
		 */
		QString operator()(const QString &base) const
		{
			return QDir::toNativeSeparators(base + _suffix);
		}

	private:
		/// Common suffix for paths
		QString _suffix;
};
}

bool SearchPaths::init(const QString &pathSuffix)
{
	const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
	const Extender extend(pathSuffix);

	QStringList configPaths;
	QStringList dataPaths;
#if defined(Q_OS_WIN)
	{
		const QString var = env.value("LOCALAPPDATA");
		for (const auto &path : var.split(QDir::listSeparator(), QString::SkipEmptyParts)) {
			const auto extendedPath = extend(path);
			configPaths.append(extendedPath);
			dataPaths.append(extendedPath);
		}
	}
#endif
	for (const auto &path :
	     QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation)) {
		const auto extendedPath = extend(path);
		if (configPaths.isEmpty() || (configPaths.last() != extendedPath)) {
			configPaths.append(extendedPath);
		}
	}
	for (const auto &path :
	     QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
		const auto extendedPath = extend(path);
		if (dataPaths.isEmpty() || (dataPaths.last() != extendedPath)) {
			dataPaths.append(extendedPath);
		}
	}

	LOGGER_DEBUG(logger) << "Using config paths: " << configPaths.join(" ");
	LOGGER_DEBUG(logger) << "Using data paths: " << dataPaths.join(" ");

	QDir::setSearchPaths("config", configPaths);
	QDir::setSearchPaths("data", dataPaths);
	return true;
}

namespace
{
/** Determine if a full path is writable.
 * @param path The path to check.
 * @return True if the path itself exists and is writable, or if the
 * longest existing parent directory is writable.
 */
bool canWrite(const QString &path)
{
	const QFileInfo pathInfo(path);
	if (pathInfo.exists()) {
		return pathInfo.isWritable();
	} else {
		return canWrite(pathInfo.absolutePath());
	}
}
}

QStringList SearchPaths::allPaths(const QString &prefix, const QString &fileName)
{
	QStringList fullPaths;
	foreach(const QString &path, QDir::searchPaths(prefix))
	{
		const QDir testDir(path);
		const QString fullPath(
			QDir::toNativeSeparators(testDir.absoluteFilePath(fileName)));
		fullPaths.append(fullPath);
	}
	return fullPaths;
}

QString SearchPaths::forWriting(const QString &prefix, const QString &fileName)
{
	foreach(const QString &path, QDir::searchPaths(prefix))
	{
		const QDir testDir(path);
		const QString fullPath(
			QDir::toNativeSeparators(testDir.absoluteFilePath(fileName)));
		const bool finished = canWrite(fullPath);
		LOGGER_DEBUG(logger) << "forWriting " << prefix << " \"" << fileName << "\" is "
				     << finished << " at " << fullPath;
		if (finished) {
			return fullPath;
		}
	}

	LOGGER_WARN(logger) << "No forWriting path found under \"" << prefix << "\" with name \""
			    << fileName << "\"";
	return QString();
}

QString SearchPaths::forReading(const QString &prefix, const QString &fileName, bool required)
{
	const QStringList searchList = QDir::searchPaths(prefix);
	foreach(const QString &path, searchList)
	{
		const QDir testDir(path);
		const QFileInfo fullPath(
			QDir::toNativeSeparators(testDir.absoluteFilePath(fileName)));
		const bool finished = fullPath.exists();
		LOGGER_DEBUG(logger) << "forReading " << prefix << " \"" << fileName << "\" is "
				     << finished << " at " << fullPath.absoluteFilePath();
		if (finished) {
			return fullPath.absoluteFilePath();
		}
	}

	if (required) {
		throw std::runtime_error(QString("No path found for \"%1\" with name \"%2\"")
						 .arg(prefix, fileName)
						 .toStdString());
	}

	LOGGER_WARN(logger) << "No forReading path found for \"" << prefix << "\" with name \""
			    << fileName << "\"";
	return QString();
}

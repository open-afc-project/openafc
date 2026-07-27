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

/** Verify that @p fullPath resolves to a location under @p root, and hand
 * back the exact canonical path that was checked.
 *
 * Returns true only when both paths canonicalise and the file's canonical
 * path is the root or has the root (plus separator) as a prefix. Prevents
 * absolute or ../-bearing fileName arguments from escaping the registered
 * search roots.
 *
 * @param canonOut On success, set to the canonicalised path that was
 * validated. Callers must act on this path (not re-derive/re-walk the
 * original @p fullPath) so the object that was checked is the same object
 * that gets opened -- otherwise a symlink swapped into an intermediate
 * path component between the check and a later independent path-walk
 * (e.g. open()/GDAL re-resolving the raw string) can redirect resolution
 * outside the search root the check just approved (TOCTOU).
 */
bool containedUnder(const QDir &root, const QFileInfo &fullPath, QString &canonOut)
{
	const QString canonRoot = root.canonicalPath();
	if (canonRoot.isEmpty()) {
		return false;
	}
	// canonicalFilePath() is empty for nonexistent paths (the common case
	// for forWriting's not-yet-created leaf file). Resolve the parent
	// directory instead -- a purely lexical clean of the full path (the
	// prior behavior) does NOT resolve symlinks in intermediate
	// components, so a symlinked parent could pass containment on the
	// leaf name alone while resolving outside root. If the parent itself
	// does not canonicalise (does not exist / broken symlink), reject
	// rather than falling back to a lexical guess.
	QString canonFile = fullPath.canonicalFilePath();
	if (canonFile.isEmpty()) {
		const QString parentCanon = QFileInfo(fullPath.absolutePath()).canonicalFilePath();
		if (parentCanon.isEmpty()) {
			return false;
		}
		canonFile = parentCanon + QDir::separator() + fullPath.fileName();
	}
	const bool ok = canonFile == canonRoot ||
			canonFile.startsWith(canonRoot + QDir::separator());
	if (ok) {
		canonOut = canonFile;
	}
	return ok;
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
			QString canonPath;
			if (!containedUnder(testDir, QFileInfo(fullPath), canonPath)) {
				LOGGER_WARN(logger)
					<< "forWriting " << prefix << " \"" << fileName
					<< "\" rejected: resolves outside search root " << path;
				continue;
			}
			// Return the path that was actually validated, not a
			// re-derived raw string a caller's open() would re-walk.
			return canonPath;
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
			QString canonPath;
			if (!containedUnder(testDir, fullPath, canonPath)) {
				LOGGER_WARN(logger)
					<< "forReading " << prefix << " \"" << fileName
					<< "\" rejected: resolves outside search root " << path;
				continue;
			}
			// Return the canonicalised path that containedUnder() just
			// validated, not fullPath.absoluteFilePath() (a raw,
			// non-canonical string): callers open()/GDAL-load this
			// return value, and returning anything other than the
			// exact path that was checked reintroduces a check/use
			// (TOCTOU) split across independent path-walks.
			return canonPath;
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

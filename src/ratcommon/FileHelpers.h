//

#ifndef SRC_RATCOMMON_FILEHELPERS_H_
#define SRC_RATCOMMON_FILEHELPERS_H_

#include <stdexcept>
#include <QIODevice>
#include <memory>

class QFile;
class QString;
class QFileInfo;
class QDir;

namespace FileHelpers
{

/// Error indicating file system issue
struct Error : public std::runtime_error {
		Error(const QString &msg) : runtime_error(msg.toStdString())
		{
		}
};

/** Open a file for reading or writing.
 *
 * @param name The full file name to open.
 * @param mode The mode to open as.
 * @return The newly opened file.
 * @throw FileError If the file fails to open.
 */
std::unique_ptr<QFile> open(const QString &name, QIODevice::OpenMode mode);

/** Open a file for reading or writing, creating parent paths as necessary.
 *
 * @param name The full file name to open.
 * @param mode The mode to open as.
 * @return The newly opened file.
 * @throw FileError If the file fails to open.
 */
std::unique_ptr<QFile> openWithParents(const QString &name, QIODevice::OpenMode mode);

/** Ensure that parent directories of a file exist, creating as necessary.
 *
 * @param fileName The absolute file path to require.
 * @throw Error If parents cannot be created.
 */
void ensureParents(const QFileInfo &fileName);

void ensureExists(const QDir &path);

/** Remove a single file entry.
 *
 * @param filePath The path to remove.
 * @throw Error If the file cannot be removed.
 */
void remove(const QFileInfo &filePath);

/** Remove a directory tree recursively.
 *
 * @param root The root of the tree to remove, which is also removed.
 * @throw Error If the tree cannot be fully removed.
 */
void removeTree(const QDir &root);

} // End namespace

#endif /* SRC_CPOCOMMON_FILEHELPERS_H_ */

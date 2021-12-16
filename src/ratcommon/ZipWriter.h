// 

#ifndef CPOBG_SRC_CPOCOMMON_ZIPWRITER_H_
#define CPOBG_SRC_CPOCOMMON_ZIPWRITER_H_

#include <QDateTime>
#include <QIODevice>
#include <stdexcept>
#include <zlib.h>
#include <memory>

/** Write to a Zip archive file.
 * Files are written sequentially, one-at-a-time.
 * All contained (internal) files are write-only and not able to seek.
 */
class ZipWriter{
    Q_DISABLE_COPY(ZipWriter)
    /// Private implementation storage class
    class Private;
    /// IODevice subclass
    class ContentFile;
public:
    /** Any error associated with reading a Zip file is thrown as a FileError.
     */
    class FileError : public std::runtime_error{
    public:
        FileError(const std::string &msg)
          : runtime_error(msg){}
    };

    /// Determine how to handle existing Zip files when writing.
    enum WriterMode{
        /// If file exists, overwrite any content with new Zip file
        Overwrite,
        /// If file exists, append new content to end of Zip (must ensure unique filenames)
        Append,
    };
    /// Determine what type of compression to use for writing content
    enum CompressionLevel{
        /// Do not compress files, simply copy them into the Zip
        CompressCopy = Z_NO_COMPRESSION,
        /// Use minimal compression for speed
        CompressFast = Z_BEST_SPEED,
        /// Use maximum compression for size
        CompressSmall = Z_BEST_COMPRESSION,
        /// Use default compression
        CompressDefault = Z_DEFAULT_COMPRESSION,
    };

    /** Open a given file for writing.
     * @param fileName The name of the zip archive to open.
     * @param mode Determines the behavior when writing to existing files.
     * @param compress Sets the compression level for all files written to the
     * archive. This is an integer in range [0, 9], and can use the enumerated
     * values in CompressionLevel.
     */
    ZipWriter(const QString &fileName, WriterMode mode = Overwrite, int compress = CompressDefault);

    /** Close the archive.
     * This will also close any open content files.
     */
    virtual ~ZipWriter();

    /** Set the comment to be written to this file before it is closed.
     *
     * @param comment The comment text to write.
     */
    void setFileComment(const QString &comment);

    /** Open a desired internal file for writing.
     * @param intFileName The file name of the object within the Zip archive.
     * @return A reference to an object which exists until the next call
     * to openFile() or until the destruction of the parent ZipWriter.
     * @param modTime The file modification time to write.
     * The time zone of the date-time is ignored within the zip file.
     * @throw FileError if the internal file fails to open.
     *
     * Use of this function should be similar to:
     * try{
     *     auto cFile = zip.openFile("file.name");
     * }
     * catch(ZipWriter::FileError &e){
     *     ...
     * }
     */
    std::unique_ptr<QIODevice> openFile(const QString &intFileName, const QDateTime &modTime=QDateTime());

private:
    /// PIMPL instance
    std::unique_ptr<Private> _impl;
};

#endif /* ZIP_FILE_H */

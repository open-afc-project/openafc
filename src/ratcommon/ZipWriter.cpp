// Copyright (C) 2017 RKF Engineering Solutions, LLC

#include "ZipWriter.h"
#include "rkflogging/ErrStream.h"
#include <QFileInfo>
#include <QDateTime>
#include <minizip/zip.h>
#include <limits>

namespace{
/// Largest allowed block size (for 64-bit operation)
const qint64 zipMaxBlock = std::numeric_limits<int>::max();
}

class ZipWriter::Private{
public:
    /// Underlying file object
    zipFile file;
    /// File comment text
    QString comment;
    /// Compression level to use for all content (valid range [0, 9])
    int compression = 0;
    /** Pointer to child file, which is owned by this object.
     * Underlying file access allows only one content file to be opened at a
     * time and this enforces that behavior.
     */
    ContentFile *openChild = nullptr;
};

/** Represent a file within a Zip archive.
 * This interface behaves as a normal QIODevice.
 * The parent ZipWriter must not be deleted while a ContentFile is used.
 */
class ZipWriter::ContentFile : public QIODevice {
    Q_DISABLE_COPY(ContentFile)
public:
    /** Open an internal file for reading.
     * Use the given archive at the current file position.
     * The openMode() flags are set to WriteOnly.
     * @throw FileError if the internal file fails to open.
     */
    ContentFile(ZipWriter &parent);

    /** Close the internal file.
     */
    virtual ~ContentFile();

    // QIODevice interfaces. None of these throw exceptions.
    virtual bool isSequential() const override;
    /// Restrict open modes
    virtual bool open(QIODevice::OpenMode mode) override;
    /// Flush the output
    virtual void close() override;
    /// Seeking is not allowed by the zip library
    virtual bool seek(qint64 pos) override;

protected:
    /** File read accessor required by QIODevice.
     * @throw FileError in all cases, this is a write-only device.
     */
    virtual qint64 readData(char * data, qint64 maxSize) override;
    /*** File write accessor required by QIODevice.
     * @throw FileError if the write fails.
     */
    virtual qint64 writeData(const char * data, qint64 maxSize) override;

private:
    /// Underlying file object
    ZipWriter *_parent;
    /// Allow only a single open call
    bool _canOpen = true;
};

ZipWriter::ZipWriter(const QString &fileName, WriterMode mode, int compress)
  : _impl(new Private){
    _impl->compression = compress;

    int minizmode = 0;
    switch(mode){
    case Overwrite:
        minizmode = APPEND_STATUS_CREATE;
        break;
    case Append:
        minizmode = APPEND_STATUS_ADDINZIP;
        break;
    }
    _impl->file = zipOpen64(fileName.toUtf8().data(), minizmode);
    if(_impl->file == nullptr){
        throw FileError(ErrStream() << "ZipWriter failed to open \"" << fileName << "\"");
    }
}

ZipWriter::~ZipWriter(){
    // Close with comment
    zipClose(_impl->file, _impl->comment.toUtf8().data());
}

void ZipWriter::setFileComment(const QString &comment){
    _impl->comment = comment;
}

std::unique_ptr<QIODevice> ZipWriter::openFile(const QString &intFileName, const QDateTime &modTime){
    if(_impl->openChild){
        throw std::logic_error("ZipWriter can only have one ContentFile instance at a time");
    }

    // Default to current time
    QDateTime fileTime = modTime;
    if(fileTime.isNull()){
        fileTime = QDateTime::currentDateTime();
    }

    // Create file information
    zip_fileinfo fileInfo;
    // File time stamp
    {
        const auto date = fileTime.date();
        fileInfo.tmz_date.tm_year = date.year();
        fileInfo.tmz_date.tm_mon = date.month();
        fileInfo.tmz_date.tm_mday = date.day();
    }
    {
        const auto time = fileTime.time();
        fileInfo.tmz_date.tm_hour = time.hour();
        fileInfo.tmz_date.tm_min = time.minute();
        fileInfo.tmz_date.tm_sec = time.second();
    }
    fileInfo.dosDate = 0;
    // No attributes
    fileInfo.internal_fa = 0;
    fileInfo.external_fa = 0;

    // No comment or extra info, always use 64-bit mode
    int status = zipOpenNewFileInZip64(_impl->file, intFileName.toUtf8().data(), &fileInfo, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, _impl->compression, 1);
    if(status != ZIP_OK){
        throw FileError(ErrStream() << "Failed to create internal file \"" << intFileName << "\": " << status);
    }

    std::unique_ptr<ContentFile> child(new ContentFile(*this));
    child->open(QIODevice::WriteOnly);
    return std::move(child);
}

ZipWriter::ContentFile::ContentFile(ZipWriter &parentVal)
: _parent(&parentVal){
    _parent->_impl->openChild = this;
}

ZipWriter::ContentFile::~ContentFile(){
    close();
    _parent->_impl->openChild = nullptr;
}

bool ZipWriter::ContentFile::isSequential() const{
    return true;
}

bool ZipWriter::ContentFile::open(QIODevice::OpenMode mode){
    const QIODevice::OpenMode onlyFlags(QIODevice::WriteOnly | QIODevice::Text);
    if(mode & ~onlyFlags){
        return false;
    }
    // The file is only allowed to be opened once by the constructor
    // After being closed, it is closed permanently
    if(!_canOpen){
        return false;
    }
    _canOpen = false;

    return QIODevice::open(mode);
}

void ZipWriter::ContentFile::close(){
    if(!isOpen()){
        return;
    }
    
    QIODevice::close();
    const int status = zipCloseFileInZip(_parent->_impl->file);
    if(status != ZIP_OK){
        setErrorString(QString("Failed to close internal file: %1").arg(status));
        return;
    }
}

bool ZipWriter::ContentFile::seek(qint64 posVal){
    Q_UNUSED(posVal);
    return false;
}

qint64 ZipWriter::ContentFile::readData(char * data, qint64 maxSize){
    Q_UNUSED(data);
    Q_UNUSED(maxSize);
    setErrorString("Cannot read this file");
    return -1;
}

qint64 ZipWriter::ContentFile::writeData(const char * data, qint64 maxSize){
    // Clamp to maximum for zip interface
    const int blockSize = std::min(zipMaxBlock, maxSize);

    // Status from this function is purely error code, assume entire block written
    const int status = zipWriteInFileInZip(_parent->_impl->file, data, blockSize);
    if(status < 0){
        setErrorString(ErrStream() << "Failed writing to zip file: " << status);
        return -1;
    }
    emit bytesWritten(blockSize);
    return blockSize;
}

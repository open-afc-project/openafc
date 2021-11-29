//  

#include "GzipStream.h"

namespace{
/// uncompressed buffer size
const int gzBufSize = 8192;

QIODevice::OpenMode maskRw(QIODevice::OpenMode mode){
    return mode & (QIODevice::ReadOnly | QIODevice::WriteOnly);
}
}

GzipStream::GzipStream(QIODevice *dev, QObject *parentVal)
  : QIODevice(parentVal), _dev(nullptr), _pos(0){
    setSourceDevice(dev);
}

GzipStream::~GzipStream(){
    close();
}

void GzipStream::setSourceDevice(QIODevice *dev){
    close();

    if(_dev){
        disconnect(_dev, nullptr, this, nullptr);
    }
    _dev = dev;
    if(_dev){
        connect(_dev, &QIODevice::readyRead, this, &QIODevice::readyRead);
        connect(_dev, &QIODevice::readChannelFinished, this, &QIODevice::readChannelFinished);
    }
}

void GzipStream::setCompressParams(const CompressParams &params){
    _params = params;
}

bool GzipStream::open(OpenMode mode){
    if(isOpen()){
        setErrorString("GzipStream file already open");
        return false;
    }

    if(!_dev){
        setErrorString("No underlying device");
        return false;
    }
    if(!_dev->isOpen()){
        setErrorString(QString("Underlying device not open"));
        return false;
    }

    const auto rwmode = maskRw(mode);
    if(rwmode == NotOpen){
        setErrorString("Can only open for either reading or writing");
        return false;
    }
    if(!openStream(rwmode)){
        return false;
    }

    QIODevice::open(mode);
    if(rwmode == QIODevice::ReadOnly){
        emit readyRead();
    }
    return true;
}

bool GzipStream::isSequential() const{
    return true;
}

qint64 GzipStream::bytesAvailable() const{
    return QIODevice::bytesAvailable() + _readBuf.size();
}

bool GzipStream::atEnd() const{
    if(!_dev){
        return true;
    }
    return QIODevice::atEnd() && _readBuf.isEmpty() && _dev->atEnd();
}

qint64 GzipStream::pos() const{
    // subtract read-but-buffered size
    return _pos - QIODevice::bytesAvailable() + QIODevice::bytesToWrite();
}

bool GzipStream::seek(qint64 posVal){
    Q_UNUSED(posVal);
    return false;
}

bool GzipStream::reset(){
    if(!isOpen()){
        return false;
    }
    // short circuit if already at start
    if(pos() == 0){
        return true;
    }

    const auto mode = openMode();
    const auto rwmode = maskRw(mode);
    QIODevice::close();
    closeStream(rwmode);
    if(!_dev->reset()){
        return false;
    }
    openStream(rwmode);
    QIODevice::open(mode);
    if(rwmode == QIODevice::ReadOnly){
        emit readyRead();
    }
    return true;
}

void GzipStream::close(){
    if(!isOpen()){
        return;
    }

    const auto rwmode = maskRw(openMode());
    QIODevice::close();
    closeStream(rwmode);
}

qint64 GzipStream::readData(char *data, qint64 maxSize){
    // Populate the read buffer as much as possible
    while((_readBuf.size() < maxSize) && !_dev->atEnd()){
        readSource(_params.bufSize);
    }

    const qint64 copySize = std::min(maxSize, qint64(_readBuf.size()));
    ::memcpy(data, _readBuf.data(), copySize);
    // truncate remaining
    _readBuf = _readBuf.mid(copySize);

    if(copySize < maxSize){
        emit readChannelFinished();
    }
    _pos += copySize;
    return copySize;
}
qint64 GzipStream::writeData(const char *data, qint64 dataSize){
    // zlib can only write chunks sized by "uInt" and Qt copy by "int"
    const qint64 maxChunkSize = std::numeric_limits<int>::max();

    for(qint64 startIx = 0; startIx < dataSize; startIx += maxChunkSize){
        const qint64 chunkSize = std::min(dataSize - startIx, maxChunkSize);
        // zlib requires non-const input buffer
        QByteArray bufPlain(data + startIx, chunkSize);

        _stream.next_in = reinterpret_cast<uint8_t *>(bufPlain.data());
        _stream.avail_in = uInt(chunkSize);
        if(writeOut(false) < 0){
            return -1;
        }
    }

    emit bytesWritten(dataSize);
    _pos += dataSize;
    return dataSize;
}

bool GzipStream::closeStream(OpenMode rwmode){
    _pos = 0;
    _readBuf.clear();

    int status;
    switch(rwmode){
    case ReadOnly:
        status = ::inflateEnd(&_stream);
        break;

    case WriteOnly:
        _stream.next_in = nullptr;
        _stream.avail_in = 0;
        writeOut(true);
        status = ::deflateEnd(&_stream);
        break;

    default:
        return false;
    }
    if(status != Z_OK){
        setError();
        return false;
    }

    return true;
}

bool GzipStream::openStream(OpenMode rwmode){
    _pos = 0;
    _readBuf.clear();

    _stream.zalloc = Z_NULL;
    _stream.zfree = Z_NULL;
    _stream.opaque = Z_NULL;

    int status;
    switch(rwmode){
    case ReadOnly:
        if(!_dev->isReadable()){
            setErrorString(QString("Underlying device not readable"));
            return false;
        }
        status = ::inflateInit2(&_stream, _params.windowBits);
        if (status != Z_OK) {
            setError();
            return false;
        }
        if(readSource(_params.bufSize) < 0){
            ::inflateEnd(&_stream);
            return false;
        }
        break;

    case WriteOnly:
        if(!_dev->isWritable()){
            setErrorString(QString("Underlying device not writable"));
            return false;
        }
        status = ::deflateInit2(&_stream,
            _params.compressLevel, Z_DEFLATED,
            _params.windowBits, _params.memLevel,
            Z_DEFAULT_STRATEGY
        );
        if (status != Z_OK) {
            setError();
            return false;
        }
        break;

    default:
        return false;
    }

    return true;
}

qint64 GzipStream::writeOut(bool finish){
    const int flush = finish ? Z_FINISH : Z_NO_FLUSH;

    QByteArray bufComp(_params.bufSize, Qt::Uninitialized);
    qint64 totalOut = 0;
    do{
        _stream.next_out = reinterpret_cast<uint8_t *>(bufComp.data());
        _stream.avail_out = bufComp.size();

        const int status = ::deflate(&_stream, flush);
        switch(status){
        case Z_OK:
        case Z_STREAM_END:
            // Either status is acceptable pending on avail_out
            break;
        default:
            setError();
            return -1;
        }

        const qint64 have = bufComp.size() - _stream.avail_out;
        if(_dev->write(bufComp.data(), have) != have){
            setErrorString("Failed to write device");
            return -1;
        }
        totalOut += have;
    } while(_stream.avail_out == 0);

    return totalOut;
}

qint64 GzipStream::readSource(qint64 sizeVal){
    QByteArray bufComp = _dev->read(sizeVal);
    if(bufComp.isEmpty()){
        return 0;
    }
    _stream.next_in = reinterpret_cast<uint8_t *>(bufComp.data());
    _stream.avail_in = bufComp.size();

    QByteArray bufPlain(_params.bufSize, Qt::Uninitialized);
    qint64 totalIn = 0;
    do{
        _stream.next_out = reinterpret_cast<uint8_t *>(bufPlain.data());
        _stream.avail_out = bufPlain.size();

        const int status = ::inflate(&_stream, Z_NO_FLUSH);
        switch(status){
        case Z_OK:
        case Z_STREAM_END:
            // Either status is acceptable pending on avail_out
            break;
        default:
            setError();
            return -1;
        }

        const qint64 have = bufPlain.size() - _stream.avail_out;
        _readBuf += QByteArray(bufPlain.data(), have);
        totalIn += have;
    } while(_stream.avail_out == 0);

    return totalIn;
}

void GzipStream::setError(){
    setErrorString(QString::fromUtf8(_stream.msg));
}

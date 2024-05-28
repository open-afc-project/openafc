/* GET and POST files from/to remote or local file storage */

#include <stdlib.h>
#include <zlib.h>
#include <iostream>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QFile>
#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#ifdef DATA_IF_STANDALONE
    #include <QDebug>
    #define LOGGER_DEFINE_GLOBAL(a, b)
    #define LOGGER_DEBUG(a) qDebug()
    #define LOGGER_ERROR(a) qDebug()
#else
    #include "afclogging/Logging.h"
#endif
#include "data_if.h"

#define ZLIB_COMPRESS_LEVEL 6
#define ZLIB_WINDOW_BITS (MAX_WBITS + 16)
#define ZLIB_MEMORY_LEVEL 8
#if GUNZIP_INPUT_FILES
    #define ZLIB_MAX_FILE_SIZE 100000
#endif
#define MAX_NET_DELAY 5000 /* wait for download/upload, ms */

LOGGER_DEFINE_GLOBAL(logger, "AfcDataIf")

AfcDataIf::AfcDataIf(bool useUrl)
{
    AfcDataIf::_useUrl = useUrl;
    AfcDataIf::_app = NULL;
    if (AfcDataIf::_useUrl) {
        if (!QCoreApplication::instance()) {
            /* QNetworkAccessManager uses qt app events */
            int argc = 0;
            char *argv = NULL;
            AfcDataIf::_app = new QCoreApplication(argc, &argv);
        }
    }
}

AfcDataIf::~AfcDataIf()
{
    if (AfcDataIf::_app) {
        AfcDataIf::_app->quit();
        delete AfcDataIf::_app;
    }
}

bool AfcDataIf::readFile(QString fileName, QByteArray &data)
{
    LOGGER_DEBUG(logger) << "AfcDataIf::readFile(" << fileName << ")";
    if (!AfcDataIf::_useUrl) {
        QFile inFile;

        inFile.setFileName(fileName);
        if (inFile.open(QFile::ReadOnly)) {
            QByteArray gzipped = inFile.readAll();
            QByteArray *indata = &gzipped;
#if GUNZIP_INPUT_FILES
            QByteArray gunzipped;
            if (AfcDataIf::gunzipBuffer(gzipped, gunzipped)) {
                indata = &gunzipped;
            }
#endif
            data.replace(0, indata->size(), indata->data(), indata->size());
            inFile.close();
            return true;
        }
        LOGGER_ERROR(logger) << "AfcDataIf::readFile(" << fileName << ") QFile.open error";
        return false;
    } else {
        QTimer timer; /* use QNetworkRequest::setTransferTimeout after update to qt5.15 */
        QEventLoop loop;
        timer.setSingleShot(true);

        QUrl url = QUrl(fileName);
        QNetworkRequest req(url);

        QNetworkReply *reply = AfcDataIf::_mngr.get(req);

        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        timer.start(MAX_NET_DELAY);
        loop.exec();
        if (timer.isActive() && reply->error() == QNetworkReply::NoError) {
            timer.stop();
            data = reply->readAll();
            delete reply;
            return true;
        }
        LOGGER_ERROR(logger) << "readFile(" << url.toString() << ") error " << reply->error();
        delete reply;
        return false;
    }
}

bool AfcDataIf::gzipAndWriteFile(QString fileName, QByteArray &data)
{
    LOGGER_DEBUG(logger) << "gzipAndWriteFile(" << fileName << ")"
                         << " len: " << data.length();
    QByteArray gziped;
    if (!AfcDataIf::gzipBuffer(data, gziped)) {
        return false;
    }
    return AfcDataIf::writeFile(fileName, gziped);
}

bool AfcDataIf::writeFile(QString fileName, QByteArray &data)
{
    LOGGER_DEBUG(logger) << "writeFile(" << fileName << ")"
                         << " len: " << data.length();
    if (!AfcDataIf::_useUrl) {
        QFile outFile;

        outFile.setFileName(fileName);
        if (outFile.open(QFile::WriteOnly)) {
            if (outFile.write(data) == data.size()) {
                outFile.close();
                return true;
            }
            outFile.close();
        }
        return false;
    } else {
        QEventLoop loop;
        QTimer timer;
        QUrl url = QUrl(fileName);
        QNetworkRequest request(url);

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
        request.setHeader(QNetworkRequest::ContentLengthHeader, data.length());
        QNetworkReply *reply = AfcDataIf::_mngr.post(request, data);
        QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        timer.start(MAX_NET_DELAY);
        loop.exec();
        if (timer.isActive() && reply->error() == QNetworkReply::NoError) {
            timer.stop();
            delete reply;
            return true;
        }
        LOGGER_ERROR(logger) << "writeFile(" << url.toString() << ") error " << reply->error();
        delete reply;
        return false;
    }
}

bool AfcDataIf::gzipBuffer(QByteArray &input, QByteArray &output)
{
    if (!input.length()) {
        return true;
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.total_in = 0;
    strm.total_out = 0;
    strm.next_in = Z_NULL;

    int ret = deflateInit2(&strm, ZLIB_COMPRESS_LEVEL, Z_DEFLATED, ZLIB_WINDOW_BITS, ZLIB_MEMORY_LEVEL, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        LOGGER_ERROR(logger) << "deflateInit2 error";
        return false;
    }

    output.clear();
    output.resize(input.size());

    strm.avail_in = input.size();
    strm.next_in = (unsigned char *)input.data();
    strm.avail_out = output.size();
    strm.next_out = (unsigned char *)output.data();
    ret = deflate(&strm, Z_FINISH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        LOGGER_ERROR(logger) << "deflate error";
        deflateEnd(&strm);
        return false;
    }

    output.resize(output.size() - strm.avail_out);

    deflateEnd(&strm);
    return true;
}

#if GUNZIP_INPUT_FILES
bool AfcDataIf::gunzipBuffer(QByteArray &input, QByteArray &output)
{
    LOGGER_DEBUG(logger) << "AfcDataIf::gunzipBuffer()";
    if (!input.length()) {
        LOGGER_ERROR(logger) << "AfcDataIf::gunzipBuffer() empty input buffer";
        return true;
    }

    /* get output size */
    uint32_t sz = input.at(input.size() - 1);
    sz <<= 8;
    sz += input.at(input.size() - 2);
    sz <<= 8;
    sz += input.at(input.size() - 3);
    sz <<= 8;
    sz += input.at(input.size() - 4);
    if (sz > ZLIB_MAX_FILE_SIZE) {
        LOGGER_ERROR(logger) << std::hex << "AfcDataIf::gunzipBuffer() file too big " << sz;
        return false;
    }
    std::cout << std::hex << "outsize " << sz;
    output.clear();
    output.resize(sz * 2);

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    int ret = inflateInit2(&strm, -MAX_WBITS);
    if (ret != Z_OK)
        return false;
    strm.avail_in = input.size();
    strm.next_in = (unsigned char *)input.data();
    strm.avail_out = output.size();
    strm.next_out = (unsigned char *)output.data();
    ret = inflate(&strm, Z_FINISH);
    if (ret != Z_OK && ret != Z_STREAM_END) {
        LOGGER_ERROR(logger) << "inflate error " << ret;
        inflateEnd(&strm);
        return false;
    }

    inflateEnd(&strm);
    return true;
}
#endif

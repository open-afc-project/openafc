//  
#ifndef GZIP_FILE_H
#define GZIP_FILE_H

#include <zlib.h>
#include <QIODevice>

/** Represent a gzip encoded data stream with QIODevice interface.
 *
 * The "source" QIODevice is the compressed side of the file access, while
 * the GzipStream is the uncompressed side.
 * This device does not allow the size() to be pre-computed, but it does
 * accumulate total data read/written in the form of pos(). In this way
 * reading to the end of the file tells the total size.
 */
class GzipStream : public QIODevice{
    Q_OBJECT
public:
    /// Optional parameters for the compressed stream
    struct CompressParams{
        /** Compression level control between 0 and 9 inclusive.
         */
        int compressLevel = 6;
        /** Window size in number of bits between 8 and 15 inclusive.
         * Add 16 to this value to write a gzip stream.
         */
        int windowBits = MAX_WBITS + 16;
        /** Memory use control between 0 and 9 inclusive.
         */
        int memLevel = 8;
        /** The size of compressed data buffer for accessing the underlying
         * device.
         */
        int bufSize = 10240;
    };

    /** Create a new device object which wraps a source device.
     * @param dev The source device to read from or write to.
     */
    explicit GzipStream(QIODevice *dev=nullptr, QObject *parent=nullptr);

    /** Finalize and close the gzip stream.
     * @post The source device
     */
    virtual ~GzipStream();

    /** Set the underlying device to use for compressed data.
     *
     * @param dev The device to read/write compressed.
     * @post This device is closed.
     */
    void setSourceDevice(QIODevice *dev);

    /** Set parameters used for write mode (compression).
     * This is only used during open() so should be called before open().
     *
     * @param params The parameters to use when writing.
     */
    void setCompressParams(const CompressParams &params);

    /** Interface for QIODevice.
     * Open the gzip stream for reading or writing (but not both).
     *
     * @pre The source device must already be open with the same mode.
     * @param mode The mode to open, either ReadOnly or WriteOnly.
     * @return True if the stream is opened.
     */
    virtual bool open(OpenMode mode) override;

    /// Interface for QIODevice
    virtual bool isSequential() const override;
    /// Interface for QIODevice
    virtual qint64 bytesAvailable() const override;
    /// Interface for QIODevice
    virtual bool atEnd() const override;
    /// Interface for QIODevice
    virtual qint64 pos() const override;
    /// Interface for QIODevice
    virtual bool seek(qint64 pos) override;
    /// Interface for QIODevice
    virtual bool reset() override;

public slots:
    /** Interface for QIODevice.
     *  Close this device and the source device.
     *  @post Any write stream is finalized and flushed.
     */
    virtual void close() override;

protected:
    /// File read accessor required by QIODevice.
    virtual qint64 readData(char * data, qint64 maxSize) override;
    /// File write accessor required by QIODevice.
    virtual qint64 writeData(const char * data, qint64 maxSize) override;

private:
    /** Internal function to close the stream independent of this QIODevice.
     *
     * @param mode The mode that the stream was opened with.
     * @return True iff successful.
     * @post If returning false sets this errorString()
     */
    bool closeStream(OpenMode mode);
    /** Internal function to open the stream independent of this QIODevice.
     *
     * @param mode The mode to open the stream as.
     * @return True iff successful.
     * @pre The #_params must be set.
     * @post If returning false sets this errorString()
     */
    bool openStream(OpenMode mode);
    /** Encode and write part of the stream.
     *
     * @pre The input buffer of #_stream is defined.
     * @param finish True if this is the last write in the stream.
     * @return The number of compressed octets written.
     */
    qint64 writeOut(bool finish);
    /** Read and decode part of the stream.
     *
     * @param size The desired size of compressed data to read.
     * @return The number of plaintext octets read.
     */
    qint64 readSource(qint64 size);
    /// Relay any error from the socket to this device
    void setError();

    /// Underlying compressed device
    QIODevice *_dev;
    /// Count of plaintext octets read/written to current #_dev
    qint64 _pos;
    /// Set parameters
    CompressParams _params;
    /// Streaming state
    z_stream _stream;
    /// Read overflow buffer
    QByteArray _readBuf;
};


#endif // GZIP_FILE_H

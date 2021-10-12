//  

#ifndef CSV_READER_H
#define CSV_READER_H

#include <stdexcept>
#include <QTextStream>
#include <QString>
#include <QStringList>
#include <QPair>

// Handle UTF-8 properly
#define CSV_USE_TEXT_STREAM

/** Read files per comma separated value format of RFC-4180.
 * Optional file properties are non-standard separator and quotation characters.
 */
class CsvReader{
public:
    /** Any error associated with reading a CSV file.
     */
    class FileError : public std::runtime_error{
    public:
        FileError(const QString &msg) : runtime_error(msg.toStdString()){}
    };

    /** Open a file for reading upon construction.
     * @param fileName The name of the file to open for the lifetime of the
     * CsvReader object.
     * @throw FileError if file cannot be opened.
     */
    CsvReader(const QString &fileName);

    /** Bind the reader to a given input device, opening it if necessary.
     * @param device The device to read from.
     * The lifetime of the device must be longer than the CsvReader to
     * avoid a dangling pointer.
     * @throw FileError if the device is not readable.
     */
    CsvReader(QIODevice &device);

    /// Clean up after the file
    virtual ~CsvReader();

    /** Use a non-standard separator or quotation character.
     * @param separator The field separator character.
     * @param quote The field quotation character.
     * @throw std::logic_error If the characters are the same.
     */
    void setCharacters(const QChar &separator, const QChar &quote);

    /** Set strict enforcement of a specific line ending.
     *  @param validate True if validation should be performed.
     */
    void setValidateLineEnding(bool validate);

    /** Set the line ending for strict validation. Default is \n
     *  @param end Ending to expect.
     */
    void setLineEndingString(const QString &to);

    /** Determine if whitespace at start and end of fields should be removed
     * by this reader.
     * This trimming occurs after all standard CSV processing.
     *
     * @param trim If true, field strings will be trimmed by the reader.
     */
    void setFieldsTrimmed(bool trim){
        _fieldTrim = trim;
    }

    /** Keep quotes at the edges of the CSV fields.
     * @param keep If true, will keep field-surrounding quotes in field text.
     */
    void setKeepQuotes(bool keep){
        _keepQuotes = keep;
    }
    
    /** Determine if the end-of-file has been reached.
     * If this is true, then readRow will always fail.
     *
     * The conditions for being at CSV end-of-file are:
     *  - The source device is no longer readable
     *  - The source device is itself at EOF
     *
     * A sequential device may never bet at EOF and so the CSV stream will
     * never at end on a sequential source device (i.e. QFile on stdin). In
     * this case readRow() will raise an exception if no source data is
     * available.
     *
     * @return True if readRow() should not be used.
     */
    bool atEnd() const;

    /** Read a list of elements from a row in the file.
     *
     * @return The list of fields present in the next row of the file.
     * If keepQuotes is enabled, then any quotation of the text data is kept
     * in the CSV fields.
     * @throw FileError if file cannot be read from.
     */
    QStringList readRow();

    /**
     *  Set the nominal column count to speed up readRow. Note that
     *  true number of columns still driven by file contents; this just controls
     *  Qlist::reserve() in readRow().
     *
     *  @param to The new column count.
     */
    void setNominalColumnCount(int to){
        _nominalColumnCount = to;
    }

private:
    /** Set control options to defaults.
     */
    void _defaultOpts();

    /// Inserted between values
    QChar _sep;
    /// Surround values to be quoted
    QChar _quote;

    /// Keep quotes. Off by default.
    bool _keepQuotes;
    /// Strict line ending validation.
    bool _validateLineEnding;
    /// Trim field strings
    bool _fieldTrim;

    /// Line ending to check.
    QString _lineEnding;
    
    /// Set to true if this object owns the IO device
    bool _ownFile;
    /// Underlying IO device
    QIODevice *_dev;
#if defined(CSV_USE_TEXT_STREAM)
    /// Underlying input stream
    QTextStream _str;
#endif
    int _nominalColumnCount;
};

#endif // CSV_READER_H

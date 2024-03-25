//

#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include "ratcommon_export.h"
#include <stdexcept>
#include <QTextStream>
#include <QString>
#include <QRegExp>
#include <QSet>

/** Write files per comma separated value format of RFC-4180.
 * Optional file properties are non-standard separator, quotation characters,
 * and end-of-line string.
 */
class RATCOMMON_EXPORT CsvWriter
{
		/// Empty type for EOL placeholder
		struct EndRow {};

	public:
		/** Any error associated with writing a CSV file.
		 */
		class FileError : public std::runtime_error
		{
			public:
				FileError(const QString &msg) : runtime_error(msg.toStdString())
				{
				}
		};

		/** Open the file for reading upon construction.
		 * @param fileName The name of the file to open for the lifetime of the
		 * CsvWriter object.
		 * @throw FileError if file cannot be opened.
		 */
		CsvWriter(const QString &fileName);

		/** Bind the writer to a given output device, opening it if necessary.
		 * @param device The device to write to.
		 * The lifetime of the device must be longer than the CsvWriter to
		 * avoid a dangling pointer.
		 * @throw FileError if the device is not writable.
		 */
		CsvWriter(QIODevice &device);

		/** Close the file if constructed with the fileName argument.
		 * If necessary, an end-of-row marker is written.
		 */
		~CsvWriter();

		/** Use a non-standard separator or quotation character.
		 * @param separator The field separator character.
		 * @param quote The field quotation character.
		 * @throw std::logic_error If the characters are the same.
		 */
		void setCharacters(const QChar &separator, const QChar &quote);

		/** Set a static list of which columns should be unconditionally quoted.
		 * @param cols Each value is a column index to be quoted.
		 */
		void setQuotedColumns(const QSet<int> &cols)
		{
			_quotedCols = cols;
		}

		/** Define a non-standard definition of when to quote a CSV field.
		 * The standard is to quote if a quote, separator, or EOL is encountered.
		 */
		void setQuotedMatch(const QRegExp &regex)
		{
			_quotedExpr = regex;
		}

		/** Read a list of elements from a row in the file.
		 * @param records The list of records to write.
		 * @throw FileError if file write fails.
		 * @post All of the elements and an end-of-row marker are written to the stream.
		 */
		void writeRow(const QStringList &records);

		/** Write a single record to the CSV stream.
		 * When all records in a row are written, writeEndRow() should be called.
		 * @param record The element to write
		 * @throw FileError if file write fails.
		 */
		void writeRecord(const QString &record);

		/** Write the end-of-row indicator and start a new row.
		 * @pre Some number of records should be written with writeRecord().
		 */
		void writeEndRow();

		/// Placeholder for finishing row writes
		static const EndRow endr;

		/** Write a single element to the CSV stream.
		 * @param record The element to write
		 * @return The modified CSV stream.
		 */
		CsvWriter &operator<<(const QString &record)
		{
			writeRecord(record);
			return *this;
		}

		/** Write and end-of-row indicator and start a new row.
		 * @return The modified CSV stream.
		 */
		CsvWriter &operator<<(const EndRow &)
		{
			writeEndRow();
			return *this;
		}

	private:
		/** Set control options to defaults.
		 * @post The #_sep, #_quote, and #_eol characters are set to RFC defaults.
		 */
		void _defaultOpts();

		/** Write data to the device and verify status.
		 * @param text The text to write.
		 * @throw FileError if a problem occurs.
		 */
		void _write(const QString &text);

		/// Inserted between values
		QChar _sep;
		/// Surround values to be quoted
		QChar _quote;
		/// Appended to end row
		QByteArray _eol;

		/// List of strings which, if contained, will cause the value to be quoted
		QSet<QChar> _quotedChars;
		/// List of column indices (zero-indexed) to be quoted unconditionally
		QSet<int> _quotedCols;
		/// Expression used to determine which values to quote
		QRegExp _quotedExpr;

		/// Set to true if this object owns the IO device
		bool _ownFile;
		/// Underlying output stream
		QTextStream _str;

		/// The current column index (starting at zero)
		int _colI;
};

#endif // CSV_WRITER_H

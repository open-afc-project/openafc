//

#include <QFile>
#include <QStringList>
#include "CsvWriter.h"

const CsvWriter::EndRow CsvWriter::endr = CsvWriter::EndRow();

CsvWriter::CsvWriter(const QString &fileName) : _ownFile(true), _colI(0)
{
	_defaultOpts();

	// Destructor is not called if constructor throws
	QScopedPointer<QIODevice> file(new QFile(fileName));
	if (!file->open(QIODevice::WriteOnly)) {
		throw FileError(QString("Failed to open \"%1\" for writing: %2")
					.arg(fileName, file->errorString()));
	}
	_str.setDevice(file.take());
	_str.setCodec("UTF-8");
}

CsvWriter::CsvWriter(QIODevice &device) : _ownFile(false), _colI(0)
{
	_defaultOpts();

	if (!device.isOpen()) {
		if (!device.open(QIODevice::WriteOnly)) {
			throw FileError(QString("Failed to open for writing: %1")
						.arg(device.errorString()));
		}
	}
	_str.setDevice(&device);
}

CsvWriter::~CsvWriter()
{
	if (_colI > 0) {
		writeEndRow();
	}
	if (_ownFile) {
		delete _str.device();
	}
}

void CsvWriter::setCharacters(const QChar &separator, const QChar &quote)
{
	if (separator == quote) {
		throw std::logic_error("Cannot use same character for quote and separator");
	}
	_quotedChars.remove(_sep);
	_quotedChars.remove(_quote);
	_sep = separator;
	_quote = quote;
	_quotedChars.insert(_sep);
	_quotedChars.insert(_quote);
}

void CsvWriter::writeRow(const QStringList &elements)
{
	foreach(const QString &elem, elements)
	{
		writeRecord(elem);
	}
	writeEndRow();
}

void CsvWriter::writeRecord(const QString &rec)
{
	// Escape the text if necessary
	QString text = rec;

	bool doQuote = false;
	if (_quotedCols.contains(_colI)) {
		doQuote = true;
	} else if (!_quotedExpr.isEmpty() && (_quotedExpr.indexIn(text) >= 0)) {
		doQuote = true;
	} else {
		foreach(const QChar &val, _quotedChars)
		{
			if (text.contains(val)) {
				doQuote = true;
				break;
			}
		}
	}

	if (doQuote) {
		// Escape quote with an extra quote
		text.replace(_quote, QString("%1%1").arg(_quote));
		// Wrap with quote characters
		text = QString("%1%2%1").arg(_quote).arg(text);
	}

	// Insert separator character if necessary
	if (_colI > 0) {
		text.push_front(_sep);
	}
	_write(text);
	++_colI;
}

void CsvWriter::writeEndRow()
{
	_write(_eol);
	_colI = 0;
}

void CsvWriter::_defaultOpts()
{
	_sep = ',';
	_quote = '"';
	_eol = "\r\n";
	_quotedChars << _sep << _quote << '\r' << '\n';
}

void CsvWriter::_write(const QString &text)
{
	_str << text;
	if (_str.status() != QTextStream::Ok) {
		throw FileError(
			QString("Failed to write output: %1").arg(_str.device()->errorString()));
	}
}

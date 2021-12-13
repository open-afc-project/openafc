#include <QFile>
#include "CsvReader.h"

namespace{

/** Simple state machine to match a text string exactly.
 */
class TextMatcher{
    Q_DISABLE_COPY(TextMatcher)
public:
    /** Create a new matcher
     *
     * @param text The exact text to match.
     */
    TextMatcher(const QString &text){
        _pat = text;
        _curs = _pat.begin();
    }

    /** Get the text size
     *
     * @return The number of characters in the fully-matched text.
     */
    int size() const{
        return _pat.count();
    }

    /** Determine if this matcher has succeeded.
     *
     * @return True if this matcher has matched all characters.
     */
    bool matched() const{
        return (_curs == _pat.end());
    }

    /** Reset this matcher to the beginning.
     */
    void reset(){
        _curs = _pat.begin();
    }

    /** Attempt to add a character to this matcher.
     *
     * @param chr The character to attempt to accept.
     * @return True if this new character is accepted into the matcher, whether
     * or not the full match is completed.
     * If already matched, then a new character is never accepted.
     * @post If this matcher did not accept the character, the state is reset.
     * @sa matched()
     */
    bool addChar(const QChar &chr){
        if(matched()){
            return false;
        }
        if(*_curs == chr){
            ++_curs;
            return true;
        }
        else{
            reset();
            return false;
        }
    }

private:
    /// Exact text to match
    QString _pat;
    /// Current test cursor
    QString::const_iterator _curs;
};

/** A single-row state machine for parsing CSV data.
 * Each instance of this class handles exactly one row of decoding.
 */
class FieldMachine{
public:
    enum State{
        StartField,
        UnquotedField,
        QuotedField,
        QuotedFirstQuote,
        EndOfRow,
    };

    /// Create the new row extractor
    FieldMachine()
      : keepQuotes(false), state(StartField){}

    /// Clean up resources
    ~FieldMachine(){
        qDeleteAll(eol);
    }

    /** Attempt to add a new character into the parser state machine.
     *
     * @param chr The new character from CSV data.
     * @return True if the character resulted in the end of a field string
     * in #field.
     * @throw CsvReader::FileError If the new character is invalid.
     */
    bool addChar(const QChar &chr){
        switch(state){
        case StartField:
            if(chr == sep){
                // may be blank/empty
                return true;
            }
            // Only first character of field determines quote status
            else if(chr == quote){
                state = QuotedField;
                if(keepQuotes){
                    field.append(quote);
                }
            }
            else{
                state = UnquotedField;
                // Re-process in new state
                return addChar(chr);
            }
            return false;

        case UnquotedField:
            if(chr == sep){
                state = StartField;
                // Separator is dropped
                return true;
            }
            else{
                field.append(chr);
                // EOL text is kept in unquoted field until full match
                return checkEol(chr, false);
            }
            break;

        case QuotedField:
            if(chr == quote){
                state = QuotedFirstQuote;
            }
            else{
                // Any other character is part of the field
                field.append(chr);
            }
            return false;

        case QuotedFirstQuote:
            // Two quotes decode into one
            if(chr == quote){
                state = QuotedField;
                field.append(quote);
                return false;
            }
            else{
                if(keepQuotes){
                    field.append(quote);
                }
                if(chr == sep){
                    state = StartField;
                    // Separator is dropped
                    return true;
                }

                state = UnquotedField;
                // Re-process in new state
                return addChar(chr);
            }
            break;

        case EndOfRow:
            throw CsvReader::FileError("Characters after end-of-row");
        }

        return false;
    }

    /** Determine if #field is fully valid.
     *
     * @return True if the field has been completed.
     */
    bool fieldEnd() const{
        switch(state){
        case StartField:
        case EndOfRow:
            return true;

        default:
            return false;
        }
    }

    /** Determine if the entire row is finished.
     *
     * @return True if no more characters are accepted.
     */
    bool rowEnd() const{
        return (state == EndOfRow);
    }

    /// Field-separator character
    QChar sep;
    /// Quotation character
    QChar quote;
    /// Determine whether start/end quotation characters are kept
    bool keepQuotes;
    /// End-of-row alternatives. Objects are owned by this object.
    QList<TextMatcher *> eol;

    /// State machine
    State state;
    /// Field string accumulator
    QString field;

private:
    /** Read the new character into the EOL checker.
     *
     * @param chr The character to read.
     * @param require If true, then one of the EOL strings must accept the new
     * character.
     * @return The matcher object, if any of the EOL strings matched fully
     * at the current character.
     * @throw CsvReader::FileError If the EOL is required but the new character
     * was not accepted.
     * @post If returned non-null, the #state is reset to EndOfRow and the
     */
    bool checkEol(const QChar &chr, bool require){
        bool anyAccept = false;
        for(QList<TextMatcher *>::iterator it = eol.begin(); it != eol.end(); ++it){
            if((**it).addChar(chr)){
                anyAccept = true;
            }
        }

        if(require && !anyAccept){
            throw CsvReader::FileError("Bad or missing end-of-row");
        }

        for(QList<TextMatcher *>::const_iterator it = eol.begin(); it != eol.end(); ++it){
            if((**it).matched()){
                state = EndOfRow;
                field.chop((**it).size());
                return true;
            }
        }
        return false;
    }
};
}

CsvReader::CsvReader(const QString &fileName)
    : _ownFile(true), _nominalColumnCount(-1){
    _defaultOpts();

    // Destructor is not called if constructor throws
    QScopedPointer<QIODevice> file(new QFile(fileName));

    if(!file->open(QIODevice::ReadOnly)){
        throw FileError(QString("Failed to open \"%1\" for reading: %2").arg(fileName, file->errorString()));
    }
    _dev = file.take();
#if defined(CSV_USE_TEXT_STREAM)
    _str.setDevice(_dev);
#endif
}

CsvReader::CsvReader(QIODevice &device)
    : _ownFile(false), _nominalColumnCount(-1){
    _defaultOpts();

    if(!device.isOpen()){
        if(!device.open(QIODevice::ReadOnly)){
            QFile *qFile = qobject_cast<QFile *>(&device);
            if (qFile != NULL) {
                throw FileError(QString("Failed to open \"%1\" for reading: %2").arg(qFile->fileName()).arg(device.errorString()));
            }
            else {
                throw FileError(QString("Failed to open for reading: %1").arg(device.errorString()));
            }
        }
    }
    _dev = &device;
#if defined(CSV_USE_TEXT_STREAM)
    _str.setDevice(_dev);
#endif
}

CsvReader::~CsvReader(){
    if(_ownFile){
        delete _dev;
    }
}

void CsvReader::setCharacters(const QChar &separator, const QChar &quote){
    if(separator == quote){
        throw std::logic_error("Cannot use same character for quote and separator");
    }
    _sep = separator;
    _quote = quote;
}

bool CsvReader::atEnd() const{
#if defined(CSV_USE_TEXT_STREAM)
    return _str.atEnd();
#else
    return _dev->atEnd();
#endif
}

void CsvReader::setValidateLineEnding(bool to){
    _validateLineEnding = to;
}

void CsvReader::setLineEndingString(const QString &to){
    _lineEnding = to;
}

void CsvReader::_defaultOpts(){
    _sep = ',';
    _quote = '"';
    _keepQuotes = false;
    _validateLineEnding = false;
    _lineEnding = "\r\n";
    _fieldTrim = false;
}

QStringList CsvReader::readRow(){
    if(atEnd()){
        throw FileError("Attempt to read past end of CSV file");
    }

    FieldMachine process;
    process.sep = _sep;
    process.quote = _quote;
    process.keepQuotes = _keepQuotes;
    process.eol << new TextMatcher(_lineEnding);
    if(!_validateLineEnding){
        process.eol << new TextMatcher("\n");
    }

    QStringList fields;
    QChar chr;

    if(_nominalColumnCount > 0) fields.reserve(_nominalColumnCount);
    while(true){
#if defined(CSV_USE_TEXT_STREAM)
        _str >> chr;
        switch(_str.status()){
        case QTextStream::Ok:
            process.addChar(chr);
            break;

        case QTextStream::ReadPastEnd:
            // Insert trailing EOL if necessary
            if(process.state == FieldMachine::QuotedField){
                throw FileError(
                    QString("End of file within quoted field")
                );
            }
            foreach(const QChar &chrVal, _lineEnding){
                process.addChar(chrVal);
            }
            break;

        default:
            throw FileError(
                QString("Failed to read line from file: %1")
                    .arg(_str.device()->errorString())
            );
        }
#else
        if(_dev->atEnd()){
            // Insert trailing EOL if necessary
            if(process.state == FieldMachine::QuotedField){
                throw FileError(
                    QString("End of file within quoted field")
                );
            }
            foreach(const QChar &chr, _lineEnding){
                process.addChar(chr);
            }
        }
        else{
            char c;
            if(_dev->getChar(&c)){
                chr = char(c);
                process.addChar(chr);
            }
            else{
                throw FileError(
                    QString("Failed to read line from file: %1")
                    .arg(_dev->errorString())
                );
            }
        }
#endif

        if(process.fieldEnd()){
            //qDebug() << "OUT" << process.field;
            fields.append(process.field);
            process.field.clear();
        }

        if(process.rowEnd()){
            break;
        }
    }

    if(_fieldTrim){
        for(QStringList::iterator it = fields.begin(); it != fields.end(); ++it){
            *it = it->trimmed();
        }
    }

    return fields;
}

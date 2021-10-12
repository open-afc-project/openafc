//  

#ifndef FILE_TEST_HELPERS_H
#define FILE_TEST_HELPERS_H

#include <QString>
#include <QStringList>
#include <QDir>

namespace FileTestHelpers{
    struct RuntimeError : public std::runtime_error{
        RuntimeError(const QString &msg) : runtime_error(msg.toStdString()){}
    };

    /** Provide a temporary, unique test directory for a test process.
     * The environment variable FILETESTHELPERS_TESTDIR_KEEP being non-empty
     * sets the initial value of the keep property.
     */
    class TestDir : public QDir{
        bool _keep = false;
    public:
        /** Construct a new temporary testing directory.
         * @param testName The unique (to the process) name of the test.
         */
        TestDir(const QString &testName);

        /// Remove the directory
        virtual ~TestDir();

        /// Remove all directory contents
        void clean();

        /** Copy a file from outside the test diretory into the test directory.
         * @param file The file to copy in.
         */
        void takeFile(const QFileInfo &file);

        void setKeep(bool option);
    };

    /** Create a new empty file for testing.
     * @param fileName The file to create.
     * @return True if the file was created.
     */
    bool makeFile(const QString &fileName);

    /** Create a new non-empty file for testing.
     * @param fileName The file to create.
     * @param content A simple text string which is written to the file.
     * @return True if the file was created and the content written.
     */
    bool makeFile(const QString &fileName, const QByteArray &content);

    /** Read the content of a file.
     * @param fileName The file to read from.
     * @return The content of the file, or a null QByteArray if non-existant.
     */
    QByteArray content(const QString &fileName);

    /** Create a read-only file for testing.
     * @param fileName The file to create.
     * @return True if the file was created properly.
     */
    bool makeReadOnly(const QString &fileName);

    /** Surround a string by double-quotation marks.
     */
    inline QString quoted(const QString &str){
        return '"' + str + '"';
    }

    /** Surround each string by double-quotation marks.
     */
    inline QStringList quoted(const QStringList &parts){
        QStringList result;
        foreach(const QString &part, parts){
            result << quoted(part);
        }
        return result;
    }

    /** Convert from kibibytes (IEC 60050) to bytes.
     */
    inline qint64 kibToBytes(int kibibytes){
        return qint64(kibibytes) * 1024;
    }
    /** Convert from mebibytes (IEC 60050) to bytes.
     */
    inline qint64 mibToBytes(int mebibytes){
        return qint64(mebibytes) * 1024 * 1024;
    }
    /** Convert from gibibytes (IEC 60050) to bytes.
     */
    inline qint64 gibToBytes(int gibibytes){
        return qint64(gibibytes) * 1024 * 1024 * 1024;
    }


    /** Execute a shell command.
     * QFAIL if the command fails
     */
    void execSub(const QString &cmd, const QStringList &args, const QString &workingDir = QString());
}

#endif /* FILE_TEST_HELPERS_H */

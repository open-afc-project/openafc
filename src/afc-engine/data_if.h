/* GET and POST files from/to remote or local file storage */

#include <QtNetwork/QNetworkAccessManager>
#include <QtCore/QCoreApplication>

#define GUNZIP_INPUT_FILES 0 /* Do input files are gzipped? */


class AfcDataIf: public QObject
{
	Q_OBJECT
public:
	AfcDataIf(bool useUrl);
	~AfcDataIf();
	bool readFile(QString fileName, QByteArray& data);
	bool writeFile(QString fileName, QByteArray& data);
	bool gzipAndWriteFile(QString fileName, QByteArray& data);
private:
	bool _useUrl;
	QNetworkAccessManager _mngr;
	bool gzipBuffer(QByteArray& indata, QByteArray& outdata);
	QCoreApplication* _app;
#if GUNZIP_INPUT_FILES
	bool gunzipBuffer(QByteArray &input, QByteArray &output);
#endif
};

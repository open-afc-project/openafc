/* GET and POST files from/to remote or local file storage */

#include <QtNetwork/QNetworkAccessManager>
#include <QtCore/QCoreApplication>

#define GUNZIP_INPUT_FILES 0 /* Do input files are gzipped? */


class AfcDataIf: public QObject
{
	Q_OBJECT
public:
	AfcDataIf(std::string remote);
	~AfcDataIf();
	bool readFile(QString fileName, QByteArray& data);
	bool writeFile(QString fileName, QByteArray& data);
private:
	QString _remote;
	QNetworkAccessManager _mngr;
	bool gzipBuffer(QByteArray& indata, QByteArray& outdata);
	QCoreApplication* _app;
#if GUNZIP_INPUT_FILES
	bool gunzipBuffer(QByteArray &input, QByteArray &output);
#endif
};

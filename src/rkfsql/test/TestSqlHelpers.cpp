// Copyright (C) 2017 RKF Engineering Solutions, LLC

#include "../SqlHelpers.h"
#include "rkfunittest/UnitTestHelpers.h"
#include "rkfunittest/GtestShim.h"
#include <QSqlDatabase>
#include <QDateTime>

class TestSqlHelpers : public testing::Test {
public:
    void SetUp() override{
        QSqlDatabase::addDatabase("QMYSQL", "mysql");
        _conns.push_back("mysql");
    }

    void TearDown() override{
        for(const QString &name : _conns){
            QSqlDatabase::removeDatabase(name);
        }
        _conns.clear();
    }

    std::vector<QString> _conns;
};

TEST_F(TestSqlHelpers, testWrite){
}

#if 0
void TestSqlHelpers::testQuotedVariant_data(){
    QTest::addColumn<QVariant>("var");
    QTest::addColumn<QString>("encoded");

    QTest::newRow("null") << QVariant() << "NULL";
    QTest::newRow("text") << QVariant("test") << "'test'";
    QTest::newRow("empty") << QVariant(QString()) << "NULL";
    QTest::newRow("bytes") << QVariant(QByteArray("hello")) << "'68656c6c6f'";
    QTest::newRow("number") << QVariant(2) << "2";
    QTest::newRow("utctime") << QVariant(QDateTime(QDate(2013, 1, 2), QTime(3, 4), Qt::UTC)) << "'2013-01-02 03:04:00'";
    //QTest::newRow("localtime") << QVariant(QDateTime(QDate(2013, 1, 2), QTime(3, 4), Qt::LocalTime)) << "'2013-01-02 03:04:00'";
}

#include <QSqlField>
void TestSqlHelpers::testQuotedVariant(){
    const QFETCH(QVariant, var);
    const QFETCH(QString, encoded);

    foreach(const QString &name, _conns){
        QSqlDriver *drv = QSqlDatabase::database(name, false).driver();
#if 0
        QSqlField field;
        field.setValue(var);
        qDebug() << drv << var << drv->formatValue(field) << SqlHelpers::quoted(drv, var);
#endif
        QCOMPARE(SqlHelpers::quoted(drv, var), encoded);
    }
}
#endif

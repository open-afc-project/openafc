//

#include "../SqlConnectionDefinition.h"
#include "../SqlScopedConnection.h"
#include "../SqlHelpers.h"
#include "../SqlError.h"

#if 0
AUTOTEST_REGISTER(TestSqlTimeout);

void TestSqlTimeout::initTestCase(){
    _mysqlPort = UnitTestHelpers::randomTcpPort();
    _mysql.reset(new MysqlTestServer(_mysqlPort, QStringList()
        << "wait_timeout=1"
    ));
}

void TestSqlTimeout::cleanupTestCase(){
    _mysql.reset();
}

void TestSqlTimeout::testAutoReconnect(){
    SqlConnectionDefinition defn;
    defn.driverName = "QMYSQL";
    defn.hostName = "127.0.0.1";
    defn.port = _mysqlPort;

    {
        SqlScopedConnection<SqlExceptionDb> conn;
        *conn = defn.newConnection();
        QVERIFY(!conn->isOpen());
        conn->tryOpen();
        QVERIFY(conn->isOpen());
        QVERIFY_CATCH("query", SqlHelpers::exec(*conn, "SELECT 1"););

        // wait past timeout
        QTest::qWait(1300);

        QVERIFY(conn->isOpen());
        QVERIFY_THROW(SqlError, SqlHelpers::exec(*conn, "SELECT 1"););
    }

    // now with reconnect
    defn.options = "MYSQL_OPT_RECONNECT=1";
    {
        SqlScopedConnection<SqlExceptionDb> conn;
        *conn = defn.newConnection();
        QVERIFY(!conn->isOpen());
        conn->tryOpen();
        QVERIFY(conn->isOpen());
        QVERIFY_CATCH("query", SqlHelpers::exec(*conn, "SELECT 1"););

        // wait past timeout
        QTest::qWait(1300);

        QVERIFY(conn->isOpen());
        QVERIFY_CATCH("query", SqlHelpers::exec(*conn, "SELECT 1"););
    }
}
#endif

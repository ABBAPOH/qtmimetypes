#include <QtCore/QFile>

#include <QtTest/QtTest>

#include <QMimeDatabase>

class tst_QMimeType : public QObject
{
    Q_OBJECT

public:
    tst_QMimeType();
    ~tst_QMimeType();

private Q_SLOTS:
    void initTestCase();

    void findByName_data();
    void findByName();

    void findByData_data();
    void findByData();

    void findByFile_data();
    void findByFile();

private:
    QMimeDatabase database;
};

tst_QMimeType::tst_QMimeType() :
        database()
{
}

tst_QMimeType::~tst_QMimeType()
{
}

void tst_QMimeType::initTestCase()
{
}

void tst_QMimeType::findByName_data()
{
    QTest::addColumn<QString>("filePath");
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QString>("xFail");

    QString prefix = QLatin1String(SRCDIR "testfiles/");

    QFile f(prefix + QLatin1String("list"));
    QVERIFY(f.open(QIODevice::ReadOnly));

    QByteArray line(1024, Qt::Uninitialized);

    while (!f.atEnd()) {
        int len = f.readLine(line.data(), 1023);

        if (len <= 2 || line.at(0) == '#')
            continue;

        QString string = QString::fromLatin1(line.constData(), len - 1).trimmed();
        QStringList list = string.split(QLatin1Char(' '), QString::SkipEmptyParts);
        QVERIFY(list.size() >= 2);

        QString filePath = list.at(0);
        QString mimeType = list.at(1);
        QString xFail;
        if (list.size() == 3)
            xFail = list.at(2);

        QTest::newRow(filePath.toLatin1().constData()) << prefix + filePath << mimeType << xFail;
    }
}

void tst_QMimeType::findByName()
{
    QFETCH(QString, filePath);
    QFETCH(QString, mimeType);
    QFETCH(QString, xFail);

    //qDebug() << Q_FUNC_INFO << filePath;

    const QString resultMimeTypeName = database.findByName(filePath).name();
    //qDebug() << Q_FUNC_INFO << "findByName() returned" << resultMimeTypeName;

    // Results are ambiguous when multiple MIME types have the same glob
    // -> accept the current result if the found MIME type actually
    // matches the file's extension.
    const QMimeType foundMimeType = database.mimeTypeForName(resultMimeTypeName);
    const QString extension = QFileInfo(filePath).suffix();
    //qDebug() << Q_FUNC_INFO << "globPatterns:" << foundMimeType.globPatterns() << "- extension:" << QString() + "*." + extension;
    if (foundMimeType.globPatterns().contains("*." + extension))
        return;

    const bool shouldFail = (xFail.length() >= 1 && xFail.at(0) == QLatin1Char('x'));
    if (shouldFail) {
        // Expected to fail
        QVERIFY2(resultMimeTypeName != mimeType, qPrintable(resultMimeTypeName));
    } else {
        QCOMPARE(resultMimeTypeName, mimeType);
    }
}

void tst_QMimeType::findByData_data()
{
    findByName_data();
}

void tst_QMimeType::findByData()
{
    QFETCH(QString, filePath);
    QFETCH(QString, mimeType);
    QFETCH(QString, xFail);

    QFile f(filePath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray data = f.readAll();

    const QString resultMimeTypeName = database.findByData(data).name();
    if (xFail.length() >= 2 && xFail.at(1) == QLatin1Char('x')) {
        // Expected to fail
        QVERIFY2(resultMimeTypeName != mimeType, qPrintable(resultMimeTypeName));
    }
    else {
        QCOMPARE(resultMimeTypeName, mimeType);
    }
}

void tst_QMimeType::findByFile_data()
{
    findByName_data();
}

void tst_QMimeType::findByFile()
{
    QFETCH(QString, filePath);
    QFETCH(QString, mimeType);
    QFETCH(QString, xFail);

    const QString resultMimeTypeName = database.findByFile(QFileInfo(filePath)).name();
    //qDebug() << Q_FUNC_INFO << filePath << "->" << resultMimeTypeName;
    if (xFail.length() >= 3 && xFail.at(2) == QLatin1Char('x')) {
        // Expected to fail
        QVERIFY2(resultMimeTypeName != mimeType, qPrintable(resultMimeTypeName));
    }
    else {
        QCOMPARE(resultMimeTypeName, mimeType);
    }
}

QTEST_APPLESS_MAIN(tst_QMimeType)

#include "tst_qmimedatabase.moc"

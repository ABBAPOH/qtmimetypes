/**************************************************************************
**
** This file is part of QMime
**
** Based on Qt Creator source code
**
** Qt Creator Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).
**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
**************************************************************************/

#include "qmimeprovider_p.h"

#include "mimetypeparser_p.h"
#include <qstandardpaths.h>

#include <QDir>
#include <QFile>
#include <QDebug>
#include <qendian.h>

static QString fallbackParent(const QString& mimeTypeName)
{
    const QString myGroup = mimeTypeName.left(mimeTypeName.indexOf(QLatin1Char('/')));
    // All text/* types are subclasses of text/plain.
    if (myGroup == QLatin1String("text") && mimeTypeName != QLatin1String("text/plain"))
        return QLatin1String("text/plain");
    // All real-file mimetypes implicitly derive from application/octet-stream
    if (myGroup != QLatin1String("inode") &&
        // ignore non-file extensions
        myGroup != QLatin1String("all") && myGroup != QLatin1String("fonts") && myGroup != QLatin1String("print") && myGroup != QLatin1String("uri")
        && mimeTypeName != QLatin1String("application/octet-stream")) {
        return QLatin1String("application/octet-stream");
    }
    return QString();
}

QMimeProviderBase::QMimeProviderBase(QMimeDatabasePrivate *db)
    : m_db(db)
{
}

QMimeBinaryProvider::QMimeBinaryProvider(QMimeDatabasePrivate *db)
    : QMimeProviderBase(db)
{
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_INTEGRITY)
#define QT_USE_MMAP
#endif

struct QMimeBinaryProvider::CacheFile
{
    CacheFile(QFile *file);
    ~CacheFile();

    bool isValid() const { return m_valid; }
    inline quint16 getUint16(int offset) const {
        return qFromBigEndian(*reinterpret_cast<quint16 *>(data + offset));
    }
    inline quint32 getUint32(int offset) const {
        return qFromBigEndian(*reinterpret_cast<quint32 *>(data + offset));
    }
    inline const char* getCharStar(int offset) const {
        return reinterpret_cast<const char *>(data + offset);
    }

    QFile *file;
    uchar *data;
    bool m_valid;
};

QMimeBinaryProvider::CacheFile::CacheFile(QFile *f)
    : file(f), m_valid(false)
{
    data = file->map(0, file->size());
    if (data) {
        const int major = getUint16(0);
        const int minor = getUint16(2);
        m_valid = (major == 1 && minor >= 1 && minor <= 2);
    }
}

QMimeBinaryProvider::CacheFile::~CacheFile()
{
    delete file;
}

QMimeBinaryProvider::~QMimeBinaryProvider()
{
    qDeleteAll(m_cacheFiles);
}

// Position of the "list offsets" values, at the beginning of the mime.cache file
enum { PosAliasListOffset = 4,
       PosParentListOffset = 8,
       PosLiteralListOffset = 12,
       PosReverseSuffixTreeOffset = 16,
       PosGlobListOffset = 20,
       PosMagicListOffset = 24,
       // PosNamespaceListOffset = 28,
       PosIconsListOffset = 32,
       PosGenericIconsListOffset = 36
     };

bool QMimeBinaryProvider::isValid()
{
#if defined(QT_USE_MMAP)
    // TODO qgetenv, so that the unittest can choose between with or without mime.cache
    return false; // HACK FOR NOW

    const QStringList cacheFilenames = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QLatin1String("mime/mime.cache"));
    qDeleteAll(m_cacheFiles);
    m_cacheFiles.clear();

    // Verify version
    foreach (const QString& cacheFilename, cacheFilenames) {
        QFile *file = new QFile(cacheFilename);
        if (file->open(QIODevice::ReadOnly)) {
            CacheFile *cacheFile = new CacheFile(file);
            if (cacheFile->isValid())
                m_cacheFiles.append(cacheFile);
            else
                delete cacheFile;
        } else
            delete file;
    }

    if (m_cacheFiles.count() > 1)
        return true;
    if (m_cacheFiles.isEmpty())
        return false;

    // We found exactly one file; is it the user-modified mimes, or a system file?
    const QString foundFile = m_cacheFiles.first()->file->fileName();
    const QString localCacheFile = QStandardPaths::storageLocation(QStandardPaths::GenericDataLocation) + QLatin1String("/mime/mime.cache");

    return foundFile != localCacheFile;
#else
    return false;
#endif
}

QMimeType QMimeBinaryProvider::mimeTypeForName(const QString &name)
{
    // TODO implement, including a QCache
    QMimeType mime;
    Q_UNUSED(name);
    return mime;
}

QStringList QMimeBinaryProvider::findByName(const QString &fileName, QString *foundSuffix)
{
    GlobMatchResult result;
    result.m_weight = 0;
    result.m_matchingPatternLength = 0;
    foreach (CacheFile *cacheFile, m_cacheFiles) {
        matchGlobList(result, cacheFile, cacheFile->getUint32(PosLiteralListOffset), fileName);
        matchGlobList(result, cacheFile, cacheFile->getUint32(PosGlobListOffset), fileName);
        const int reverseSuffixTreeOffset = cacheFile->getUint32(PosReverseSuffixTreeOffset);
        const int numRoots = cacheFile->getUint32(reverseSuffixTreeOffset);
        const int firstRootOffset = cacheFile->getUint32(reverseSuffixTreeOffset + 4);
        matchSuffixTree(result, cacheFile, numRoots, firstRootOffset, fileName, fileName.length() - 1);
    }
    *foundSuffix = result.m_foundSuffix;
    return result.m_matchingMimeTypes;
}

void QMimeBinaryProvider::GlobMatchResult::addMatch(const QString &mimeType, int weight, const QString &pattern)
{
    // Is this a lower-weight pattern than the last match? Skip this match then.
    if (weight < m_weight)
        return;
    bool replace = weight > m_weight;
    if (!replace) {
        // Compare the length of the match
        if (pattern.length() < m_matchingPatternLength)
            return; // too short, ignore
        else if (pattern.length() > m_matchingPatternLength) {
            // longer: clear any previous match (like *.bz2, when pattern is *.tar.bz2)
            replace = true;
        }
    }
    if (replace) {
        m_matchingMimeTypes.clear();
        // remember the new "longer" length
        m_matchingPatternLength = pattern.length();
        m_weight = weight;
    }
    m_matchingMimeTypes.append(mimeType);
    if (pattern.startsWith(QLatin1String("*.")))
        m_foundSuffix = pattern.mid(2);
}

void QMimeBinaryProvider::matchGlobList(GlobMatchResult& result, CacheFile *cacheFile, int off, const QString &fileName)
{
    const int numGlobs = cacheFile->getUint32(off);
    //qDebug() << "Loading" << numGlobs << "globs from" << cacheFile->file->fileName() << "at offset" << cacheFile->globListOffset;
    for (int i = 0; i < numGlobs; ++i) {
        const int globOffset = cacheFile->getUint32(off + 4 + 12 * i);
        const int mimeTypeOffset = cacheFile->getUint32(off + 4 + 12 * i + 4);
        const int flagsAndWeight = cacheFile->getUint32(off + 4 + 12 * i + 8);
        const int weight = flagsAndWeight & 0xff;
        const bool caseSensitive = flagsAndWeight & 0x100;
        const Qt::CaseSensitivity qtCaseSensitive = caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
        const QString pattern = QLatin1String(cacheFile->getCharStar(globOffset));

        const char* mimeType = cacheFile->getCharStar(mimeTypeOffset);
        qDebug() << pattern << mimeType << weight << caseSensitive;
        QMimeGlobPattern glob(pattern, QString() /*unused*/, weight, qtCaseSensitive);

        // TODO: this could be done faster for literals where a simple == would do.
        if (glob.matchFileName(fileName))
            result.addMatch(QLatin1String(mimeType), weight, pattern);
    }
}

bool QMimeBinaryProvider::matchSuffixTree(GlobMatchResult& result, QMimeBinaryProvider::CacheFile *cacheFile, int numEntries, int firstOffset, const QString &fileName, int charPos)
{
    QChar fileChar = fileName[charPos];
    int min = 0;
    int max = numEntries - 1;
    while (min <= max) {
        const int mid = (min + max) / 2;
        const int off = firstOffset + 12 * mid;
        const QChar ch = cacheFile->getUint32(off);
        if (ch < fileChar)
            min = mid + 1;
        else if (ch > fileChar)
            max = mid - 1;
        else {
            --charPos;
            int numChildren = cacheFile->getUint32(off + 4);
            int childrenOffset = cacheFile->getUint32(off + 8);
            bool success = false;
            if (charPos > 0)
                success = matchSuffixTree(result, cacheFile, numChildren, childrenOffset, fileName, charPos);
            if (!success) {
                for (int i = 0; i < numChildren; ++i) {
                    const int childOff = childrenOffset + 12 * i;
                    const int mch = cacheFile->getUint32(childOff);
                    if (mch != 0)
                        break;
                    const int mimeTypeOffset = cacheFile->getUint32(childOff + 4);
                    const char* mimeType = cacheFile->getCharStar(mimeTypeOffset);
                    const int flagsAndWeight = cacheFile->getUint32(childOff + 8);
                    const int weight = flagsAndWeight & 0xff;
                    //const bool caseSensitive = flagsAndWeight & 0x100;
                    // TODO handle caseSensitive
                    result.addMatch(QLatin1String(mimeType), weight, QLatin1String("*.") + fileName.mid(charPos));
                    success = true;
                }
            }
            return success;
        }
    }
    return false;
}

QMimeType QMimeBinaryProvider::findByMagic(const QByteArray &data, int *accuracyPtr)
{
    QMimeType candidate;
    Q_UNUSED(data);
    *accuracyPtr = 0;
    // TODO implement
    return candidate;
}

QStringList QMimeBinaryProvider::parents(const QString &mime)
{
    const QByteArray mimeStr = mime.toLatin1();
    QStringList result;
    foreach (CacheFile *cacheFile, m_cacheFiles) {
        const int parentListOffset = cacheFile->getUint32(PosParentListOffset);
        const int numEntries = cacheFile->getUint32(parentListOffset);

        int begin = 0;
        int end = numEntries - 1;
        while (end >= begin) {
            const int medium = (begin + end)/2;
            const int off = parentListOffset + 4 + 8 * medium;
            const int mimeOffset = cacheFile->getUint32(off);
            const char* aMime = cacheFile->getCharStar(mimeOffset);
            const int cmp = strcmp(aMime, mimeStr.constData());
            if (cmp < 0)
                begin = medium + 1;
            else if (cmp > 0)
                end = medium - 1;
            else {
                const int parentsOffset = cacheFile->getUint32(off + 4);
                const int numParents = cacheFile->getUint32(parentsOffset);
                for (int i = 0; i < numParents; ++i) {
                    const char* aParent = cacheFile->getCharStar(parentsOffset + 4 + 4 * i);
                    result.append(QString::fromLatin1(aParent));
                }
                break;
            }
        }
    }
    if (result.isEmpty()) {
        const QString parent = fallbackParent(mime);
        if (!parent.isEmpty())
            result.append(parent);
    }
    return result;
}

QString QMimeBinaryProvider::resolveAlias(const QString &name)
{
    // TODO implement
    return name;
}

QList<QMimeType> QMimeBinaryProvider::allMimeTypes()
{
    QList<QMimeType> result;
    // TODO implement
    return result;
}


////

QMimeXMLProvider::QMimeXMLProvider(QMimeDatabasePrivate *db)
    : QMimeProviderBase(db), m_loaded(false)
{
}

bool QMimeXMLProvider::isValid()
{
    return true;
}

QMimeType QMimeXMLProvider::mimeTypeForName(const QString &name)
{
    ensureLoaded();

    return m_nameMimeTypeMap.value(name);
}

QStringList QMimeXMLProvider::findByName(const QString &fileName, QString *foundSuffix)
{
    ensureLoaded();

    const QStringList matchingMimeTypes = m_mimeTypeGlobs.matchingGlobs(fileName, foundSuffix);
    return matchingMimeTypes;
}

QMimeType QMimeXMLProvider::findByMagic(const QByteArray &data, int *accuracyPtr)
{
    ensureLoaded();

    QMimeType candidate;

    // TODO implement properly.
    foreach (const QMimeType &mime, m_nameMimeTypeMap) {
        const int contentPriority = mime.d->matchesData(data);
        if (contentPriority && contentPriority > *accuracyPtr) {
            *accuracyPtr = contentPriority;
            candidate = mime;
        }
    }
    return candidate;
}

void QMimeXMLProvider::ensureLoaded()
{
    if (!m_loaded) {
        bool fdoXmlFound = false;
        QStringList allFiles;

        const QStringList packageDirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QLatin1String("mime/packages"), QStandardPaths::LocateDirectory);
        foreach (const QString &packageDir, packageDirs) {
            QDir dir(packageDir);
            const QStringList files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
            //qDebug() << Q_FUNC_INFO << packageDir << files;
            if (!fdoXmlFound)
                fdoXmlFound = files.contains(QLatin1String("freedesktop.org.xml"));
            QStringList::const_iterator endIt(files.constEnd());
            for (QStringList::const_iterator it(files.constBegin()); it != endIt; ++it) {
                allFiles.append(packageDir + QLatin1Char('/') + *it);
            }
        }

        if (!fdoXmlFound) {
            // TODO: putting the xml file in the resource is a hack for now
            // We should instead install the file as part of installing Qt
            load(QLatin1String(":/qmime/freedesktop.org.xml"));
        }

        foreach (const QString& file, allFiles)
            load(file);
    }
}

void QMimeXMLProvider::load(const QString &fileName)
{
    QString errorMessage;
    if (!load(fileName, &errorMessage))
        qWarning("QMimeDatabase: Error loading %s\n%s", qPrintable(fileName), qPrintable(errorMessage));
}

bool QMimeXMLProvider::load(const QString &fileName, QString *errorMessage)
{
    m_loaded = true;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromLatin1("Cannot open %1: %2").arg(fileName, file.errorString());
        return false;
    }

    if (errorMessage)
        errorMessage->clear();

    MimeTypeParser parser(*this);
    return parser.parse(&file, fileName, errorMessage);
}

void QMimeXMLProvider::addGlobPattern(const QMimeGlobPattern& glob)
{
    m_mimeTypeGlobs.addGlob(glob);
}

void QMimeXMLProvider::addMimeType(const QMimeType &mt)
{
    m_nameMimeTypeMap.insert(mt.name(), mt);
}

QStringList QMimeXMLProvider::parents(const QString &mime)
{
    QStringList result = m_parents.value(mime);
    if (result.isEmpty()) {
        const QString parent = fallbackParent(mime);
        if (!parent.isEmpty())
            result.append(parent);
    }
    return result;
}

void QMimeXMLProvider::addParent(const QString &child, const QString &parent)
{
    m_parents[child].append(parent);
}

QString QMimeXMLProvider::resolveAlias(const QString &name)
{
    return m_aliases.value(name, name);
}

void QMimeXMLProvider::addAlias(const QString &alias, const QString &name)
{
    m_aliases.insert(alias, name);
}

QList<QMimeType> QMimeXMLProvider::allMimeTypes()
{
    return m_nameMimeTypeMap.values();
}

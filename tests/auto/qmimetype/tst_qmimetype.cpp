/****************************************************************************
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the QtMimeTypes addon of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tst_qmimetype.h"

#include <qmimemagicrulematcher_p.h>
#include <qmimetype_p.h>

#include <qmimetype.h>
#include <qmimedatabase.h>

#include <QtTest/QtTest>

// ------------------------------------------------------------------------------------------------

void tst_qmimetype::initTestCase()
{
    qputenv("XDG_DATA_DIRS", "doesnotexist");
}

// ------------------------------------------------------------------------------------------------

static QString qMimeTypeName()
{
    return QString::fromLatin1("image/png");
}

// ------------------------------------------------------------------------------------------------

#if 0
static QString qMimeTypeDisplayName()
{
    return QString::fromLatin1("PNG File");
}
#endif

// ------------------------------------------------------------------------------------------------

static QString qMimeTypeGenericIconName()
{
    return QString::fromLatin1("/usr/share/icons/oxygen/64x64/mimetypes/image-x-generic.png");
}

// ------------------------------------------------------------------------------------------------

static QString qMimeTypeIconName()
{
    return QString::fromLatin1("/usr/share/icons/oxygen/64x64/mimetypes/image-x.png");
}

// ------------------------------------------------------------------------------------------------

static QStringList buildQMimeTypeFilenameExtensions()
{
    QStringList result;
    result << QString::fromLatin1("*.png");
    return result;
}

// ------------------------------------------------------------------------------------------------

static QStringList qMimeTypeGlobPatterns()
{
    static const QStringList result (buildQMimeTypeFilenameExtensions());
    return result;
}

// ------------------------------------------------------------------------------------------------

#ifndef Q_COMPILER_RVALUE_REFS
QMIMETYPE_BUILDER
#else
QMIMETYPE_BUILDER_FROM_RVALUE_REFS
#endif

// ------------------------------------------------------------------------------------------------

void tst_qmimetype::test_isValid()
{
    QMimeType instantiatedQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    QVERIFY(instantiatedQMimeType.isValid());

    QMimeType otherQMimeType (instantiatedQMimeType);

    QVERIFY(otherQMimeType.isValid());
    QCOMPARE(instantiatedQMimeType, otherQMimeType);

    QMimeType defaultQMimeType;

    QVERIFY(!defaultQMimeType.isValid());
}

// ------------------------------------------------------------------------------------------------

void tst_qmimetype::test_name()
{
    QMimeType instantiatedQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    QMimeType otherQMimeType (
                  buildQMimeType (
                      QString(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    // Verify that the Name is part of the equality test:
    QCOMPARE(instantiatedQMimeType.name(), qMimeTypeName());

    QVERIFY(instantiatedQMimeType != otherQMimeType);
    QVERIFY(!(instantiatedQMimeType == otherQMimeType));
}

// ------------------------------------------------------------------------------------------------

void tst_qmimetype::test_genericIconName()
{
    QMimeType instantiatedQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    QMimeType otherQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      QString(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    // Verify that the GenericIconName is part of the equality test:
    QCOMPARE(instantiatedQMimeType.genericIconName(), qMimeTypeGenericIconName());

    QVERIFY(instantiatedQMimeType != otherQMimeType);
    QVERIFY(!(instantiatedQMimeType == otherQMimeType));
}

// ------------------------------------------------------------------------------------------------

void tst_qmimetype::test_iconName()
{
    QMimeType instantiatedQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    QMimeType otherQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      QString(),
                      qMimeTypeGlobPatterns()
                  )
              );

    // Verify that the IconName is part of the equality test:
    QCOMPARE(instantiatedQMimeType.iconName(), qMimeTypeIconName());

    QVERIFY(instantiatedQMimeType != otherQMimeType);
    QVERIFY(!(instantiatedQMimeType == otherQMimeType));
}

// ------------------------------------------------------------------------------------------------

void tst_qmimetype::test_suffixes()
{
    QMimeType instantiatedQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      qMimeTypeGlobPatterns()
                  )
              );

    QMimeType otherQMimeType (
                  buildQMimeType (
                      qMimeTypeName(),
                      qMimeTypeGenericIconName(),
                      qMimeTypeIconName(),
                      QStringList()
                  )
              );

    // Verify that the FilenameExtensions are part of the equality test:
    QCOMPARE(instantiatedQMimeType.globPatterns(), qMimeTypeGlobPatterns());
    QCOMPARE(instantiatedQMimeType.suffixes(), QStringList() << QString::fromLatin1("png"));

    QVERIFY(instantiatedQMimeType != otherQMimeType);
    QVERIFY(!(instantiatedQMimeType == otherQMimeType));
}

// ------------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    tst_qmimetype tc;
    return QTest::qExec(&tc, argc, argv);
}

INCLUDEPATH *= $$PWD/../include
DEPENDPATH  *= $$PWD

INCLUDEPATH += $$PWD/inqt5
DEPENDPATH  += $$PWD/inqt5

QT       -= gui

DEFINES += QT_NO_CAST_FROM_ASCII

SOURCES += qmimedatabase.cpp \
    qmimetype.cpp \
    magicmatcher.cpp \
    mimetypeparser.cpp \
    qmimemagicrule.cpp \
    ../src/qmimeglobpattern.cpp \
    ../src/qmimeprovider.cpp

HEADERS += qmime_global.h \
    qmimedatabase.h \
    qmimetype.h \
    magicmatcher.h \
    qmimetype_p.h \
    magicmatcher_p.h \
    mimetypeparser_p.h \
    qmimedatabase_p.h \
    qmimemagicrule.h \
    ../src/qmimeglobpattern_p.h \
    ../src/qmimeprovider_p.h

SOURCES += inqt5/qstandardpaths.cpp
win32: SOURCES += inqt5/qstandardpaths_win.cpp
unix: {
       macx-*: {
            SOURCES += inqt5/qstandardpaths_mac.cpp
        } else {
            SOURCES += inqt5/qstandardpaths_unix.cpp
        }
}

RESOURCES += \
    ../src/qmime.qrc

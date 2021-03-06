include(../../../mimetypes.pri)

TEMPLATE = app

TARGET   = tst_qmimetype
CONFIG   += qtestlib
DEPENDPATH += .

QMAKE_CXXFLAGS += -W -Wall -Wextra -Werror -Wshadow -Wno-long-long -Wnon-virtual-dtor

CONFIG += depend_includepath


SOURCES += tst_qmimetype.cpp

HEADERS += tst_qmimetype.h


QMAKE_EXTRA_TARGETS += check
check.depends = $$TARGET
check.commands = ./$$TARGET -xunitxml -o $${TARGET}.xml

exists($${TARGET}.xml): {
    tests_results.files += $${TARGET}.xml

    unix:!symbian {
        maemo5 {
            tests_results.path = /opt/usr/share/QtMimeTypes-unittests
        } else {
            tests_results.path = /usr/share/QtMimeTypes-unittests
        }
        INSTALLS += tests_results
    }
}

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib/QtMimeTypes-tests/qmimetype
    } else {
        target.path = /usr/lib/QtMimeTypes-tests/qmimetype
    }
    INSTALLS += target
}

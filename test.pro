QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

QT += websockets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    chat.cpp \
    chatdetails.cpp \
    chatheader.cpp \
    chatsbox.cpp \
    chatwidget.cpp \
    callsession.cpp \
    clickablefilelabel.cpp \
    client.cpp \
    createchat.cpp \
    downloadmanagerworker.cpp \
    entities.cpp \
    filedownloadmanager.cpp \
    fileitemwidget.cpp \
    filesdisplaywidget.cpp \
    fileuploadmanager.cpp \
    generaldata.cpp \
    log_reg_pages.cpp \
    main.cpp \
    mainpage.cpp \
    messagehandler.cpp \
    req_resp_utils.cpp \
    request.cpp \
    response.cpp \
    uploadmanagerworker.cpp \
    utils.cpp

HEADERS += \
    chat.h \
    chatdetails.h \
    chatheader.h \
    chatsbox.h \
    chatwidget.h \
    callsession.h \
    clickablefilelabel.h \
    client.h \
    createchat.h \
    downloadmanagerworker.h \
    entities.h \
    filedownloadmanager.h \
    fileitemwidget.h \
    filesdisplaywidget.h \
    fileuploadmanager.h \
    generaldata.h \
    log_reg_pages.h \
    mainpage.h \
    messagehandler.h \
    req_resp_utils.h \
    req_utils.h \
    request.h \
    response.h \
    uploadmanagerworker.h \
    utils.h

FORMS += \
    chat.ui \
    chatdetails.ui \
    chatheader.ui \
    createchat.ui \
    log_reg_pages.ui \
    mainpage.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


win32 {
    VCPKG_ROOT = C:/Users/ikasy/vcpkg
}

win32-g++ {
    VCPKG_TRIPLET = x64-mingw-dynamic
    VCPKG_INSTALLED = $$VCPKG_ROOT/installed/$$VCPKG_TRIPLET

    INCLUDEPATH += $$VCPKG_INSTALLED/include

    exists($$VCPKG_INSTALLED/include/rtc/rtc.hpp) {
        DEFINES += HAVE_LIBDATACHANNEL
        LIBS += $$VCPKG_INSTALLED/lib/libdatachannel.dll.a

        VCPKG_BIN_DIR = $$VCPKG_INSTALLED/bin
        exists($$VCPKG_BIN_DIR/libdatachannel.dll) {
            TARGET_DLL_DIR = $$OUT_PWD
            !isEmpty(DESTDIR) {
                TARGET_DLL_DIR = $$DESTDIR
            }

            VCPKG_BIN_GLOB = $$replace(VCPKG_BIN_DIR,/,\\)\\*.dll
            TARGET_DLL_WIN = $$replace(TARGET_DLL_DIR,/,\\)

            # Копируем все DLL из vcpkg/bin (включая зависимости libdatachannel,
            # например libjuice.dll) рядом с итоговым exe.
            QMAKE_POST_LINK += cmd /c copy /Y "$$VCPKG_BIN_GLOB" "$$TARGET_DLL_WIN" $$escape_expand(\n\t)
        }
    }
}

win32-msvc {
    VCPKG_TRIPLET = x64-windows
    VCPKG_INSTALLED = $$VCPKG_ROOT/installed/$$VCPKG_TRIPLET

    INCLUDEPATH += $$VCPKG_INSTALLED/include

    exists($$VCPKG_INSTALLED/include/rtc/rtc.hpp) {
        DEFINES += HAVE_LIBDATACHANNEL
        LIBS += -L$$VCPKG_INSTALLED/lib
        LIBS += -ldatachannel
    }
}

unix {
    exists(/usr/include/rtc/rtc.hpp) {
        DEFINES += HAVE_LIBDATACHANNEL
        LIBS += -ldatachannel
    }
}

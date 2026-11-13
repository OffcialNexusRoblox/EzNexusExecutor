QT += widgets

TARGET = NovaUltimate
TEMPLATE = app

SOURCES += main.cpp \
           mainwindow.cpp

HEADERS += mainwindow.h

FORMS += 

# Optional: Add other files like injector.cpp if you split them
# SOURCES += injector.cpp

CONFIG += c++17

# For Windows, link advapi32
win32 {
    LIBS += -ladvapi32
}

# For Linux/macOS, link OpenGL
unix:!macx {
    LIBS += -lGL
}
macx {
    LIBS += -framework OpenGL
}

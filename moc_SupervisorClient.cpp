/****************************************************************************
** Meta object code from reading C++ file 'SupervisorClient.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.15)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "SupervisorClient.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SupervisorClient.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.15. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SupervisorClient_t {
    QByteArrayData data[14];
    char stringdata0[155];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SupervisorClient_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SupervisorClient_t qt_meta_stringdata_SupervisorClient = {
    {
QT_MOC_LITERAL(0, 0, 16), // "SupervisorClient"
QT_MOC_LITERAL(1, 17, 14), // "statusReceived"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 6), // "status"
QT_MOC_LITERAL(4, 40, 9), // "connected"
QT_MOC_LITERAL(5, 50, 12), // "disconnected"
QT_MOC_LITERAL(6, 63, 5), // "error"
QT_MOC_LITERAL(7, 69, 3), // "msg"
QT_MOC_LITERAL(8, 73, 11), // "onConnected"
QT_MOC_LITERAL(9, 85, 14), // "onDisconnected"
QT_MOC_LITERAL(10, 100, 11), // "onReadyRead"
QT_MOC_LITERAL(11, 112, 7), // "onError"
QT_MOC_LITERAL(12, 120, 30), // "QLocalSocket::LocalSocketError"
QT_MOC_LITERAL(13, 151, 3) // "err"

    },
    "SupervisorClient\0statusReceived\0\0"
    "status\0connected\0disconnected\0error\0"
    "msg\0onConnected\0onDisconnected\0"
    "onReadyRead\0onError\0QLocalSocket::LocalSocketError\0"
    "err"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SupervisorClient[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       8,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    1,   54,    2, 0x06 /* Public */,
       4,    0,   57,    2, 0x06 /* Public */,
       5,    0,   58,    2, 0x06 /* Public */,
       6,    1,   59,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       8,    0,   62,    2, 0x08 /* Private */,
       9,    0,   63,    2, 0x08 /* Private */,
      10,    0,   64,    2, 0x08 /* Private */,
      11,    1,   65,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QJsonObject,    3,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::QString,    7,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 12,   13,

       0        // eod
};

void SupervisorClient::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<SupervisorClient *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->statusReceived((*reinterpret_cast< const QJsonObject(*)>(_a[1]))); break;
        case 1: _t->connected(); break;
        case 2: _t->disconnected(); break;
        case 3: _t->error((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 4: _t->onConnected(); break;
        case 5: _t->onDisconnected(); break;
        case 6: _t->onReadyRead(); break;
        case 7: _t->onError((*reinterpret_cast< QLocalSocket::LocalSocketError(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (SupervisorClient::*)(const QJsonObject & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SupervisorClient::statusReceived)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (SupervisorClient::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SupervisorClient::connected)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (SupervisorClient::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SupervisorClient::disconnected)) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (SupervisorClient::*)(const QString & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SupervisorClient::error)) {
                *result = 3;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject SupervisorClient::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_SupervisorClient.data,
    qt_meta_data_SupervisorClient,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *SupervisorClient::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SupervisorClient::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SupervisorClient.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SupervisorClient::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 8)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 8;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 8)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 8;
    }
    return _id;
}

// SIGNAL 0
void SupervisorClient::statusReceived(const QJsonObject & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void SupervisorClient::connected()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void SupervisorClient::disconnected()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void SupervisorClient::error(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE

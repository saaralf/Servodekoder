/****************************************************************************
** Meta object code from reading C++ file 'dual_runtime_controller.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.4.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../../app-v3/src/dual_runtime_controller.h"
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'dual_runtime_controller.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.4.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
namespace {
struct qt_meta_stringdata_DualRuntimeController_t {
    uint offsetsAndSizes[28];
    char stringdata0[22];
    char stringdata1[17];
    char stringdata2[1];
    char stringdata3[12];
    char stringdata4[2];
    char stringdata5[3];
    char stringdata6[14];
    char stringdata7[4];
    char stringdata8[4];
    char stringdata9[4];
    char stringdata10[13];
    char stringdata11[6];
    char stringdata12[7];
    char stringdata13[5];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(sizeof(qt_meta_stringdata_DualRuntimeController_t::offsetsAndSizes) + ofs), len 
Q_CONSTINIT static const qt_meta_stringdata_DualRuntimeController_t qt_meta_stringdata_DualRuntimeController = {
    {
        QT_MOC_LITERAL(0, 21),  // "DualRuntimeController"
        QT_MOC_LITERAL(22, 16),  // "connectedChanged"
        QT_MOC_LITERAL(39, 0),  // ""
        QT_MOC_LITERAL(40, 11),  // "BackendKind"
        QT_MOC_LITERAL(52, 1),  // "b"
        QT_MOC_LITERAL(54, 2),  // "on"
        QT_MOC_LITERAL(57, 13),  // "frameReceived"
        QT_MOC_LITERAL(71, 3),  // "bus"
        QT_MOC_LITERAL(75, 3),  // "adr"
        QT_MOC_LITERAL(79, 3),  // "val"
        QT_MOC_LITERAL(83, 12),  // "trackUpdated"
        QT_MOC_LITERAL(96, 5),  // "track"
        QT_MOC_LITERAL(102, 6),  // "status"
        QT_MOC_LITERAL(109, 4)   // "text"
    },
    "DualRuntimeController",
    "connectedChanged",
    "",
    "BackendKind",
    "b",
    "on",
    "frameReceived",
    "bus",
    "adr",
    "val",
    "trackUpdated",
    "track",
    "status",
    "text"
};
#undef QT_MOC_LITERAL
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_DualRuntimeController[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       4,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   38,    2, 0x06,    1 /* Public */,
       6,    4,   43,    2, 0x06,    4 /* Public */,
      10,    2,   52,    2, 0x06,    9 /* Public */,
      12,    2,   57,    2, 0x06,   12 /* Public */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::Bool,    4,    5,
    QMetaType::Void, 0x80000000 | 3, QMetaType::Int, QMetaType::Int, QMetaType::Int,    4,    7,    8,    9,
    QMetaType::Void, 0x80000000 | 3, QMetaType::Int,    4,   11,
    QMetaType::Void, 0x80000000 | 3, QMetaType::QString,    4,   13,

       0        // eod
};

Q_CONSTINIT const QMetaObject DualRuntimeController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_DualRuntimeController.offsetsAndSizes,
    qt_meta_data_DualRuntimeController,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_DualRuntimeController_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<DualRuntimeController, std::true_type>,
        // method 'connectedChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<BackendKind, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'frameReceived'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<BackendKind, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'trackUpdated'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<BackendKind, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'status'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<BackendKind, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>
    >,
    nullptr
} };

void DualRuntimeController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<DualRuntimeController *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->connectedChanged((*reinterpret_cast< std::add_pointer_t<BackendKind>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 1: _t->frameReceived((*reinterpret_cast< std::add_pointer_t<BackendKind>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 2: _t->trackUpdated((*reinterpret_cast< std::add_pointer_t<BackendKind>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 3: _t->status((*reinterpret_cast< std::add_pointer_t<BackendKind>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (DualRuntimeController::*)(BackendKind , bool );
            if (_t _q_method = &DualRuntimeController::connectedChanged; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (DualRuntimeController::*)(BackendKind , int , int , int );
            if (_t _q_method = &DualRuntimeController::frameReceived; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (DualRuntimeController::*)(BackendKind , int );
            if (_t _q_method = &DualRuntimeController::trackUpdated; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _t = void (DualRuntimeController::*)(BackendKind , const QString & );
            if (_t _q_method = &DualRuntimeController::status; *reinterpret_cast<_t *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
    }
}

const QMetaObject *DualRuntimeController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DualRuntimeController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DualRuntimeController.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DualRuntimeController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void DualRuntimeController::connectedChanged(BackendKind _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void DualRuntimeController::frameReceived(BackendKind _t1, int _t2, int _t3, int _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void DualRuntimeController::trackUpdated(BackendKind _t1, int _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void DualRuntimeController::status(BackendKind _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE

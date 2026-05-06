#pragma once
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QWheelEvent>

/// QDoubleSpinBox that ignores scroll-wheel events so that scrolling
/// the form does not accidentally change field values.
class NoScrollSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override { e->ignore(); }
};

/// QSpinBox that ignores scroll-wheel events.
class NoScrollIntSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override { e->ignore(); }
};

/// QDoubleSpinBox that accepts wheel events only when it has keyboard focus.
class WheelWhenFocusedSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus()) QDoubleSpinBox::wheelEvent(e);
        else            e->ignore();
    }
};

/// QSpinBox that accepts wheel events only when it has keyboard focus.
class WheelWhenFocusedIntSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void wheelEvent(QWheelEvent *e) override {
        if (hasFocus()) QSpinBox::wheelEvent(e);
        else            e->ignore();
    }
};
